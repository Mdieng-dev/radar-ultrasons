/*------------------------------------------------------------------------------
 * udp_client.h — Client UDP (Winsock) et protocole du radar, écrits en C.
 *
 * Partie 1 — transport : envoyer UNE requête (un datagramme) et attendre UNE
 * réponse, avec un délai d'attente et un nombre de nouveaux essais.
 *
 * Partie 2 — protocole texte du radar : construit les commandes, les envoie
 * avec udp_request() et analyse les réponses de la carte :
 *   ANGLE <a>               -> "OK" ou "ERR <motif>"
 *   DIST                    -> "DIST <cm>" ou "DIST TIMEOUT"
 *   SCAN <debut> <fin> <pas>-> lignes "<angle> <cm>" ... puis "END"
 *----------------------------------------------------------------------------*/
#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Codes de retour (0 = succès, négatif = erreur) */
typedef enum {
    UDP_OK           =  0,
    UDP_ERR_WSA      = -1,  /* WSAStartup a échoué ou n'a pas été appelé    */
    UDP_ERR_ADDRESS  = -2,  /* adresse ou nom d'hôte invalide / introuvable */
    UDP_ERR_SOCKET   = -3,  /* création du socket impossible               */
    UDP_ERR_SEND     = -4,  /* sendto a échoué                              */
    UDP_ERR_RECV     = -5,  /* recvfrom / select a échoué                   */
    UDP_ERR_TIMEOUT  = -6,  /* aucune réponse après tous les essais         */
    UDP_ERR_CANCEL   = -7,  /* opération annulée par l'appelant             */
    UDP_ERR_PARAM    = -8,  /* paramètre invalide                           */
    UDP_ERR_UNREACH  = -9   /* ICMP « port inaccessible » reçu à chaque essai */
} udp_status_t;

/* Fonctions de rappel optionnelles (pointeurs NULL autorisés).
 *  - cancelled  : interrogée régulièrement pendant l'attente ; retourner
 *                 une valeur non nulle interrompt l'opération.
 *  - on_attempt : appelée avant chaque envoi (essai 1, 2, ...).
 * Elles sont appelées dans le thread qui exécute udp_request(). */
typedef struct {
    int  (*cancelled)(void *ctx);
    void (*on_attempt)(void *ctx, int attempt, int max_attempts);
    void *ctx;
} udp_hooks_t;

/* À appeler une fois au démarrage du programme / à la fin. */
int  udp_global_init(void);
void udp_global_cleanup(void);

/* Envoie `request` à host:port, attend une réponse pendant `timeout_ms`,
 * renvoie la requête jusqu'à `retries` fois supplémentaires si rien
 * n'arrive. La réponse est copiée dans `reply` (terminée par '\0').
 *
 * Un socket est ouvert au début de l'appel et fermé à la fin, quel que
 * soit le résultat.
 *
 * Retourne UDP_OK ou un code d'erreur ; *reply_len reçoit la longueur. */
int udp_request(const char *host, unsigned short port,
                const char *request,
                char *reply, size_t reply_size, int *reply_len,
                int timeout_ms, int retries,
                const udp_hooks_t *hooks);

/* Message lisible associé à un code de retour. */
const char *udp_strerror(int status);

/* Dernier code d'erreur Winsock (WSAGetLastError) mémorisé par le module
 * pour le thread courant, utile pour le diagnostic. */
int udp_last_wsa_error(void);

/*==============================================================================
 * Protocole du radar (ANGLE, DIST, SCAN)
 *============================================================================*/
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

#endif /* UDP_CLIENT_H */
