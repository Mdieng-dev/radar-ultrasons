/*------------------------------------------------------------------------------
 * radar_proto.h — Protocole texte du radar (commandes ANGLE, DIST, SCAN), en C.
 *
 * Construit les commandes, les envoie avec udp_request() et analyse les
 * réponses de la carte :
 *   ANGLE <a>               -> "OK" ou "ERR <motif>"
 *   DIST                    -> "DIST <cm>" ou "DIST TIMEOUT"
 *   SCAN <debut> <fin> <pas>-> lignes "<angle> <cm>" ... puis "END"
 *----------------------------------------------------------------------------*/
#ifndef RADAR_PROTO_H
#define RADAR_PROTO_H

#include <stddef.h>
#include "udp_client.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RADAR_ANGLE_MIN     0
#define RADAR_ANGLE_MAX     180
#define RADAR_MAX_POINTS    181    /* SCAN 0 180 1 au maximum            */
#define RADAR_REPLY_SIZE    2048   /* taille du tampon de réponse brute  */

/* Codes d'erreur propres au protocole (en plus de ceux de udp_client.h) */
enum {
    RADAR_ERR_BOARD    = -20,  /* la carte a répondu "ERR <motif>"            */
    RADAR_ERR_PROTOCOL = -21,  /* réponse inattendue ou mal formée            */
    RADAR_ERR_RANGE    = -22   /* paramètre refusé avant envoi (angle, pas...) */
};

/* Paramètres de connexion pour une commande */
typedef struct {
    const char        *host;        /* adresse WAN du routeur (ou IP de la carte) */
    unsigned short     port;        /* port externe redirigé (ex. 8000)           */
    int                timeout_ms;  /* délai d'attente d'un essai                 */
    int                retries;     /* nombre de nouveaux essais                  */
    const udp_hooks_t *hooks;       /* annulation / suivi des essais (optionnel)  */
} radar_conn_t;

/* Une mesure du balayage */
typedef struct {
    int    angle;     /* degrés                                  */
    double cm;        /* distance en cm (si no_echo == 0)        */
    int    no_echo;   /* 1 si la carte a répondu TIMEOUT         */
} radar_point_t;

/* Chaque fonction copie la réponse brute de la carte dans `raw` (peut être
 * NULL) pour l'affichage, et retourne UDP_OK ou un code d'erreur. */

int radar_angle(const radar_conn_t *c, int deg,
                char *raw, size_t raw_size);

int radar_dist(const radar_conn_t *c, double *cm, int *no_echo,
               char *raw, size_t raw_size);

int radar_scan(const radar_conn_t *c, int start, int end, int step,
               radar_point_t *pts, int max_pts, int *n_pts,
               char *raw, size_t raw_size);

/* Analyseurs purs (sans réseau), utilisables pour les tests. */
int radar_parse_angle_reply(const char *text);
int radar_parse_dist_reply(const char *text, double *cm, int *no_echo);
int radar_parse_scan_reply(const char *text,
                           radar_point_t *pts, int max_pts, int *n_pts);

/* Message lisible pour un code de retour (UDP_xxx ou RADAR_ERR_xxx). */
const char *radar_strerror(int status);

#ifdef __cplusplus
}
#endif

#endif /* RADAR_PROTO_H */
