/*------------------------------------------------------------------------------
 * udp_client.c — Client UDP générique (Winsock) : délai d'attente et nouvel essai.
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
#include <stdio.h>
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
