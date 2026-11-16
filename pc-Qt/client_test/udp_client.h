/*------------------------------------------------------------------------------
 * udp_client.h — Client UDP générique (Winsock), écrit en C.
 *
 * Rôle : envoyer UNE requête (un datagramme) et attendre UNE réponse,
 * avec un délai d'attente et un nombre de nouveaux essais.
 * Ce module ne connaît rien du protocole du radar (voir radar_proto.h).
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

#ifdef __cplusplus
}
#endif

#endif /* UDP_CLIENT_H */
