/*------------------------------------------------------------------------------
 * udp_client.c — Client UDP (Winsock) et protocole du radar.
 *
 * Partie 1 : transport UDP générique (délai d'attente et nouvel essai).
 * Partie 2 : commandes ANGLE / DIST / SCAN et analyse des réponses.
 *
 * Principe d'une requête :
 *   1. résolution de l'adresse (getaddrinfo : IP ou nom d'hôte) ;
 *   2. création d'un socket UDP (socket) ;
 *   3. pour chaque essai : sendto, puis attente d'une réponse avec select()
 *      par tranches de POLL_SLICE_MS, jusqu'à timeout_ms ;
 *   4. dès qu'un datagramme arrive de la bonne adresse : recvfrom, fin ;
 *   5. fermeture du socket (closesocket) dans tous les cas.
 *
 * Pourquoi select() par petites tranches plutôt qu'un seul recvfrom bloquant
 * avec SO_RCVTIMEO ? Pour pouvoir vérifier régulièrement si l'utilisateur a
 * demandé l'annulation, sans attendre la fin du délai.
 *----------------------------------------------------------------------------*/
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "udp_client.h"

#define POLL_SLICE_MS 100   /* période de vérification de l'annulation */

/* Dernière erreur Winsock, propre à chaque thread. */
static _Thread_local int g_last_wsa = 0;

int udp_last_wsa_error(void) { return g_last_wsa; }

/*------------------------------------------------------------------------------
 * Initialisation de Winsock (spécifique à Windows)
 *----------------------------------------------------------------------------*/
int udp_global_init(void)
{
    WSADATA wsa;
    int r = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (r != 0) {
        g_last_wsa = r;
        return UDP_ERR_WSA;
    }
    return UDP_OK;
}

void udp_global_cleanup(void)
{
    WSACleanup();
}

/*------------------------------------------------------------------------------
 * Résolution "host" + port -> struct sockaddr_in (IPv4)
 *----------------------------------------------------------------------------*/
static int resolve_ipv4(const char *host, unsigned short port,
                        struct sockaddr_in *out)
{
    struct addrinfo hints, *res = NULL;
    char port_str[8];
    int r;

    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_INET;      /* la carte est en IPv4 */
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    snprintf(port_str, sizeof port_str, "%u", (unsigned)port);
    r = getaddrinfo(host, port_str, &hints, &res);
    if (r != 0 || res == NULL) {
        g_last_wsa = r;
        return (r == WSANOTINITIALISED) ? UDP_ERR_WSA : UDP_ERR_ADDRESS;
    }
    memcpy(out, res->ai_addr, sizeof *out);
    freeaddrinfo(res);
    return UDP_OK;
}

/*------------------------------------------------------------------------------
 * Attend qu'un datagramme soit lisible pendant au plus `ms` millisecondes.
 * Retour : 1 = données prêtes, 0 = délai écoulé, UDP_ERR_xxx sinon.
 *----------------------------------------------------------------------------*/
static int wait_readable(SOCKET s, int ms, const udp_hooks_t *hooks)
{
    ULONGLONG deadline = GetTickCount64() + (ULONGLONG)ms;

    for (;;) {
        ULONGLONG now = GetTickCount64();
        int slice, r;
        fd_set rd;
        struct timeval tv;

        if (hooks && hooks->cancelled && hooks->cancelled(hooks->ctx))
            return UDP_ERR_CANCEL;
        if (now >= deadline)
            return 0;

        slice = (int)(deadline - now);
        if (slice > POLL_SLICE_MS) slice = POLL_SLICE_MS;

        FD_ZERO(&rd);
        FD_SET(s, &rd);
        tv.tv_sec  = slice / 1000;
        tv.tv_usec = (slice % 1000) * 1000;

        r = select(0, &rd, NULL, NULL, &tv);  /* 1er paramètre ignoré sous Windows */
        if (r == SOCKET_ERROR) {
            g_last_wsa = WSAGetLastError();
            return UDP_ERR_RECV;
        }
        if (r > 0)
            return 1;
        /* r == 0 : tranche écoulée, on reboucle (vérification d'annulation) */
    }
}

/*------------------------------------------------------------------------------
 * Requête / réponse avec délai d'attente et nouvel essai
 *----------------------------------------------------------------------------*/
int udp_request(const char *host, unsigned short port,
                const char *request,
                char *reply, size_t reply_size, int *reply_len,
                int timeout_ms, int retries,
                const udp_hooks_t *hooks)
{
    struct sockaddr_in dest;
    SOCKET s = INVALID_SOCKET;
    int status = UDP_ERR_TIMEOUT;
    int attempts, attempt, icmp_count = 0;
    int req_len;

    if (reply_len) *reply_len = 0;
    if (!host || !*host || !request || !reply || reply_size < 2
        || timeout_ms <= 0 || retries < 0 || port == 0)
        return UDP_ERR_PARAM;
    reply[0] = '\0';
    g_last_wsa = 0;

    status = resolve_ipv4(host, port, &dest);
    if (status != UDP_OK)
        return status;

    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        g_last_wsa = WSAGetLastError();
        return (g_last_wsa == WSANOTINITIALISED) ? UDP_ERR_WSA : UDP_ERR_SOCKET;
    }

    req_len  = (int)strlen(request);
    attempts = retries + 1;
    status   = UDP_ERR_TIMEOUT;

    for (attempt = 1; attempt <= attempts; ++attempt) {
        ULONGLONG deadline;
        int got_icmp = 0;

        if (hooks && hooks->cancelled && hooks->cancelled(hooks->ctx)) {
            status = UDP_ERR_CANCEL;
            goto done;
        }
        if (hooks && hooks->on_attempt)
            hooks->on_attempt(hooks->ctx, attempt, attempts);

        /* --- Envoi du datagramme --- */
        if (sendto(s, request, req_len, 0,
                   (const struct sockaddr *)&dest, sizeof dest) == SOCKET_ERROR) {
            g_last_wsa = WSAGetLastError();
            status = UDP_ERR_SEND;
            goto done;
        }

        /* --- Attente de la réponse pendant timeout_ms --- */
        deadline = GetTickCount64() + (ULONGLONG)timeout_ms;
        for (;;) {
            struct sockaddr_in from;
            int from_len = sizeof from;
            int n, w;
            ULONGLONG now = GetTickCount64();

            if (now >= deadline)
                break;                                  /* délai écoulé */

            w = wait_readable(s, (int)(deadline - now), hooks);
            if (w < 0) { status = w; goto done; }       /* erreur ou annulation */
            if (w == 0) break;                          /* délai écoulé */

            n = recvfrom(s, reply, (int)reply_size - 1, 0,
                         (struct sockaddr *)&from, &from_len);
            if (n == SOCKET_ERROR) {
                int e = WSAGetLastError();
                g_last_wsa = e;
                if (e == WSAECONNRESET) {
                    /* Particularité Windows : un ICMP « port inaccessible »
                     * reçu après notre sendto est signalé ainsi. La requête
                     * est perdue : on passe directement à l'essai suivant. */
                    got_icmp = 1;
                    break;
                }
                /* WSAEMSGSIZE : réponse plus grande que le tampon */
                status = UDP_ERR_RECV;
                goto done;
            }

            /* On ignore tout datagramme qui ne vient pas de l'adresse visée */
            if (from.sin_addr.s_addr != dest.sin_addr.s_addr
                || from.sin_port != dest.sin_port)
                continue;

            reply[n] = '\0';
            if (reply_len) *reply_len = n;
            status = UDP_OK;
            goto done;
        }
        if (got_icmp) ++icmp_count;
        /* aucune réponse : essai suivant */
    }
    status = (icmp_count == attempts) ? UDP_ERR_UNREACH : UDP_ERR_TIMEOUT;

done:
    closesocket(s);   /* fermeture propre dans tous les cas */
    return status;
}

const char *udp_strerror(int status)
{
    switch (status) {
    case UDP_OK:          return "succès";
    case UDP_ERR_WSA:     return "Winsock non initialisé (WSAStartup)";
    case UDP_ERR_ADDRESS: return "adresse invalide ou introuvable";
    case UDP_ERR_SOCKET:  return "création du socket impossible";
    case UDP_ERR_SEND:    return "échec de l'envoi (sendto)";
    case UDP_ERR_RECV:    return "échec de la réception (recvfrom)";
    case UDP_ERR_TIMEOUT: return "pas de réponse (délai dépassé après tous les essais)";
    case UDP_ERR_CANCEL:  return "opération annulée";
    case UDP_ERR_PARAM:   return "paramètre invalide";
    case UDP_ERR_UNREACH: return "port inaccessible (ICMP reçu à chaque essai)";
    default:              return "erreur inconnue";
    }
}

/*==============================================================================
 * Protocole du radar (ANGLE, DIST, SCAN)
 *
 * Règles du protocole (sujet, §4) :
 *  - messages ASCII terminés par '\n', distances en cm avec une décimale ;
 *  - un datagramme = une commande ; la réponse revient en un datagramme ;
 *  - SCAN : une ligne "<angle> <cm>" par mesure, puis "END", dans UN seul
 *    datagramme ;
 *  - toute commande refusée reçoit "ERR <motif>".
 *
 * Les nombres décimaux sont analysés « à la main » : strtod/sscanf("%lf")
 * dépendent de la locale (une locale française attend une virgule), alors
 * que la carte envoie toujours un point.
 *============================================================================*/

#define LINE_MAX_LEN 128

/*------------------------------------------------------------------------------
 * Outils d'analyse
 *----------------------------------------------------------------------------*/

/* Copie la ligne suivante de *p dans `line` (sans '\r', '\n' ni espaces en
 * début et fin) et avance *p. Retourne 0 quand le texte est épuisé. */
static int next_line(const char **p, char *line, size_t size)
{
    const char *s = *p, *e;
    size_t len;

    if (*s == '\0')
        return 0;

    e = strchr(s, '\n');
    if (e == NULL) e = s + strlen(s);
    *p = (*e == '\n') ? e + 1 : e;

    while (s < e && isspace((unsigned char)*s)) ++s;
    while (e > s && isspace((unsigned char)e[-1])) --e;

    len = (size_t)(e - s);
    if (len >= size) len = size - 1;      /* ligne tronquée si trop longue */
    memcpy(line, s, len);
    line[len] = '\0';
    return 1;
}

/* Première ligne non vide du texte. */
static void first_line(const char *text, char *line, size_t size)
{
    const char *p = text;
    line[0] = '\0';
    while (next_line(&p, line, size))
        if (line[0] != '\0')
            return;
}

static int starts_with(const char *s, const char *prefix)
{
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/* Réponse d'erreur de la carte : "ERR" seul ou "ERR <motif>". */
static int is_err_line(const char *line)
{
    return strcmp(line, "ERR") == 0 || starts_with(line, "ERR ");
}

/* Nombre décimal « [-]chiffres[.chiffres] » occupant TOUTE la chaîne. */
static int parse_decimal(const char *s, double *out)
{
    double v = 0.0, f = 0.1;
    int neg = 0, digits = 0;

    if (*s == '-' || *s == '+') { neg = (*s == '-'); ++s; }
    while (isdigit((unsigned char)*s)) { v = v * 10.0 + (*s - '0'); ++s; ++digits; }
    if (*s == '.') {
        ++s;
        while (isdigit((unsigned char)*s)) { v += (*s - '0') * f; f /= 10.0; ++s; ++digits; }
    }
    if (digits == 0 || *s != '\0')
        return 0;
    *out = neg ? -v : v;
    return 1;
}

/* Entier occupant TOUTE la chaîne. */
static int parse_int(const char *s, int *out)
{
    char *end;
    long v;
    if (*s == '\0') return 0;
    v = strtol(s, &end, 10);
    if (*end != '\0' || v < -100000 || v > 100000) return 0;
    *out = (int)v;
    return 1;
}

/* Interprète un jeton de distance : "TIMEOUT", nombre >= 0, ou nombre < 0
 * (le template de la carte utilise une valeur négative pour « pas d'écho »). */
static int parse_distance_token(const char *tok, double *cm, int *no_echo)
{
    double v;
    if (strcmp(tok, "TIMEOUT") == 0) {
        *cm = 0.0; *no_echo = 1;
        return 1;
    }
    if (!parse_decimal(tok, &v))
        return 0;
    *no_echo = (v < 0.0);
    *cm = (v < 0.0) ? 0.0 : v;
    return 1;
}

static void copy_raw(char *raw, size_t raw_size, const char *src)
{
    if (raw && raw_size > 0) {
        strncpy(raw, src, raw_size - 1);
        raw[raw_size - 1] = '\0';
    }
}

/*------------------------------------------------------------------------------
 * Analyseurs de réponses (sans réseau)
 *----------------------------------------------------------------------------*/

int radar_parse_angle_reply(const char *text)
{
    char line[LINE_MAX_LEN];
    first_line(text, line, sizeof line);
    if (strcmp(line, "OK") == 0) return UDP_OK;
    if (is_err_line(line))       return RADAR_ERR_BOARD;
    return RADAR_ERR_PROTOCOL;
}

int radar_parse_dist_reply(const char *text, double *cm, int *no_echo)
{
    char line[LINE_MAX_LEN];
    first_line(text, line, sizeof line);

    if (is_err_line(line))
        return RADAR_ERR_BOARD;
    if (!starts_with(line, "DIST "))
        return RADAR_ERR_PROTOCOL;
    return parse_distance_token(line + 5, cm, no_echo) ? UDP_OK
                                                       : RADAR_ERR_PROTOCOL;
}

int radar_parse_scan_reply(const char *text,
                           radar_point_t *pts, int max_pts, int *n_pts)
{
    const char *p = text;
    char line[LINE_MAX_LEN];
    int n = 0, ended = 0;

    *n_pts = 0;
    while (next_line(&p, line, sizeof line)) {
        char angle_tok[16], dist_tok[32], extra[2];
        int  angle;
        radar_point_t pt;

        if (line[0] == '\0')
            continue;                          /* ligne vide tolérée */
        if (n == 0 && is_err_line(line))
            return RADAR_ERR_BOARD;
        if (strcmp(line, "END") == 0) {
            ended = 1;
            break;                             /* fin du balayage */
        }

        /* Exactement deux jetons : "<angle> <distance>" */
        if (sscanf(line, "%15s %31s %1s", angle_tok, dist_tok, extra) != 2)
            return RADAR_ERR_PROTOCOL;
        if (!parse_int(angle_tok, &angle))
            return RADAR_ERR_PROTOCOL;
        if (!parse_distance_token(dist_tok, &pt.cm, &pt.no_echo))
            return RADAR_ERR_PROTOCOL;
        if (n >= max_pts)
            return RADAR_ERR_PROTOCOL;         /* plus de lignes que prévu */

        pt.angle = angle;
        pts[n++] = pt;
    }
    if (!ended)
        return RADAR_ERR_PROTOCOL;             /* END absent : réponse incomplète */

    *n_pts = n;
    return UDP_OK;
}

/*------------------------------------------------------------------------------
 * Commandes (réseau + analyse)
 *----------------------------------------------------------------------------*/

/* Envoie `cmd` et récupère la réponse brute. */
static int transact(const radar_conn_t *c, const char *cmd,
                    char *reply, size_t reply_size)
{
    int len;
    return udp_request(c->host, c->port, cmd, reply, reply_size, &len,
                       c->timeout_ms, c->retries, c->hooks);
}

int radar_angle(const radar_conn_t *c, int deg, char *raw, size_t raw_size)
{
    char cmd[32], reply[RADAR_REPLY_SIZE];
    int st;

    if (deg < RADAR_ANGLE_MIN || deg > RADAR_ANGLE_MAX)
        return RADAR_ERR_RANGE;

    snprintf(cmd, sizeof cmd, "ANGLE %d\n", deg);
    st = transact(c, cmd, reply, sizeof reply);
    if (st != UDP_OK) return st;

    copy_raw(raw, raw_size, reply);
    return radar_parse_angle_reply(reply);
}

int radar_dist(const radar_conn_t *c, double *cm, int *no_echo,
               char *raw, size_t raw_size)
{
    char reply[RADAR_REPLY_SIZE];
    int st = transact(c, "DIST\n", reply, sizeof reply);
    if (st != UDP_OK) return st;

    copy_raw(raw, raw_size, reply);
    return radar_parse_dist_reply(reply, cm, no_echo);
}

int radar_scan(const radar_conn_t *c, int start, int end, int step,
               radar_point_t *pts, int max_pts, int *n_pts,
               char *raw, size_t raw_size)
{
    char cmd[48], reply[RADAR_REPLY_SIZE];
    int st;

    *n_pts = 0;
    if (start < RADAR_ANGLE_MIN || start > RADAR_ANGLE_MAX
        || end < RADAR_ANGLE_MIN || end > RADAR_ANGLE_MAX || step <= 0)
        return RADAR_ERR_RANGE;

    snprintf(cmd, sizeof cmd, "SCAN %d %d %d\n", start, end, step);
    st = transact(c, cmd, reply, sizeof reply);
    if (st != UDP_OK) return st;

    copy_raw(raw, raw_size, reply);
    return radar_parse_scan_reply(reply, pts, max_pts, n_pts);
}

const char *radar_strerror(int status)
{
    switch (status) {
    case RADAR_ERR_BOARD:    return "la carte a refusé la commande (ERR)";
    case RADAR_ERR_PROTOCOL: return "réponse inattendue ou incomplète";
    case RADAR_ERR_RANGE:    return "paramètre hors plage";
    default:                 return udp_strerror(status);
    }
}
