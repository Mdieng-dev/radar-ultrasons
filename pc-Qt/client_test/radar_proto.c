/*------------------------------------------------------------------------------
 * radar_proto.c — Commandes ANGLE / DIST / SCAN et analyse des réponses.
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
 *----------------------------------------------------------------------------*/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "radar_proto.h"

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
