/*------------------------------------------------------------------------------
 * MDK Middleware - Component ::Network:Service
 * Name:    UDP_Socket.c
 * Purpose: Serveur UDP Radar Ultrason (ANGLE, DIST, SCAN) - STM32F746
 *----------------------------------------------------------------------------*/
#include "rl_net.h"
#include "stm32f7xx.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>

// CONSTANTS
#define UDP_PORT 5000
#define FLAG_START_SCAN (1<<0)
#define MAX_UDP_LEN 1400  // Limite sous le MTU Ethernet (1500 octets)
#define SERVO_SABILISATION_MS 120   // Temps de stabilisation mécanique du servomoteur
#define SENSOR_DELAY_MS 60    // Intervalle minimal entre deux mesures HC-SR04

int32_t udp_sock;                  // UDP socket handle

// Fonctions et variables externes définies dans main.c
extern void  servo_set_angle(int deg);
extern float distance_cm(void);
extern float capteur_d_cm;

// Contexte partagé entre udp_cb_func et Thread_Scan
static osThreadId_t tid_Scan = NULL;
volatile uint8_t scan_en_cours = 0;

static int scan_debut = 0;
static int scan_fin = 180;
static int scan_pas = 10;
static NET_ADDR scan_client_addr;

// Variable intermédiaire partagée avec ID_SCREEN_Slots.c pour la récupération de l'angle du servo
int angle = 0;

/* Envoie un texte ASCII à l'expéditeur */
static void reply(const NET_ADDR *addr, const char *msg) {
  uint32_t n = (uint32_t)strlen(msg);
  if (n == 0 || udp_sock <= 0) return;

  uint8_t *b = netUDP_GetBuffer(n);
  if (b != NULL) {
    memcpy(b, msg, n);
    netUDP_Send(udp_sock, addr, b, n);
  }
}

// Tâche RTOS dédiée à l'exécution asynchrone du balayage SCAN
static void Thread_Scan(void *argument) {
  (void)argument;
  char udp_len[MAX_UDP_LEN];
  char ligne[32];

  while (1) {
    // Attente passive du déclenchement par udp_cb_func
    osThreadFlagsWait(FLAG_START_SCAN, osFlagsWaitAll, osWaitForever);

    size_t offset = 0;
    udp_len[0] = '\0';

    for (int a = scan_debut; a <= scan_fin; a += scan_pas) {
			angle = a;
			
      // 1. Positionnement du servomoteur et stabilisation
      servo_set_angle(a);
      osDelay(SERVO_SABILISATION_MS);

      // 2. Mesure directe de distance (distance_cm gère déjà le timeout de 30 ms)
      osMutexAcquire(mutex_Capteur, 60);
      float d = distance_cm();
      osMutexRelease(mutex_Capteur);
      capteur_d_cm = d;

      // 3. Formatage de la ligne "<angle> <cm>\n" (1 décimale)
      int len_ligne;
      if (d < 0.0f) {
        len_ligne = snprintf(ligne, sizeof(ligne), "%d -1.0\n", a);
      }
			
			else {
        len_ligne = snprintf(ligne, sizeof(ligne), "%d %.1f\n", a, d);
      }

      // Vérification pour garantir que le balayage tient dans un seul datagramme UDP
      if (len_ligne > 0 && (offset + (size_t)len_ligne + 5U) < sizeof(udp_len)) {
        memcpy(&udp_len[offset], ligne, (size_t)len_ligne);
        offset += (size_t)len_ligne;
        udp_len[offset] = '\0';
      }
			
			else {
        break; // Arrêt propre si le pas demandé génère trop de lignes pour un datagramme
      }

      // 4. Espacement obligatoire d'au moins 60 ms entre deux mesures ultrason
      osDelay(SENSOR_DELAY_MS);
    }

    // Ajout du marqueur de fin "END\n"
    memcpy(&udp_len[offset], "END\n", 5U);
    offset += 4;

    // Envoi groupé vers le client
    reply(&scan_client_addr, udp_len);

    scan_en_cours = 0;
  }
}

/* Fonction de rappel appelée à chaque datagramme UDP reçu */
uint32_t udp_cb_func(int32_t socket, const NET_ADDR *addr, const uint8_t *buf, uint32_t len) {
  (void)socket;
  char cmd[64];
  char rep[32];
  int cmd_angle, d_start, d_end, d_step;

  //--- Copier buf en vraie chaîne C terminée par '\0' en s'appuyant sur len
  if (len >= sizeof(cmd)) len = (uint32_t)(sizeof(cmd) - 1);
  memcpy(cmd, buf, len);
  cmd[len] = '\0';

  //--- Supprimer les retours chariot / sauts de ligne (\r, \n) et espaces finaux
  while (len > 0 && (cmd[len - 1] == '\n' || cmd[len - 1] == '\r' || cmd[len - 1] == ' ')) {
    cmd[--len] = '\0';
  }

  //---- Analyse des commandes
  if (sscanf(cmd, "ANGLE %d", &cmd_angle) == 1) {
    if (scan_en_cours) {
      reply(addr, "ERR BUSY\n");
    }
		
		else if (cmd_angle >= 0 && cmd_angle <= 180) {
      angle = cmd_angle;
      servo_set_angle(cmd_angle);
      reply(addr, "OK\n");
    }
		
		else {
      reply(addr, "ERR RANGE\n");
    }
  }
	
  else if (strcmp(cmd, "DIST") == 0) {
    if (scan_en_cours) {
      reply(addr, "ERR BUSY\n");
    }
		
		else {
      float d = capteur_d_cm;
      if (d < 0.0f) {
        reply(addr, "DIST TIMEOUT\n");
      }
			
			else {
        snprintf(rep, sizeof(rep), "DIST %.1f\n", d);
        reply(addr, rep);
      }
    }
  }
  else if (sscanf(cmd, "SCAN %d %d %d", &d_start, &d_end, &d_step) == 3) {
    if (scan_en_cours) {
      reply(addr, "ERR BUSY\n");
    }
		
		else if (d_start < 0 || d_end > 180 || d_start > d_end || d_step <= 0) {
      reply(addr, "ERR RANGE\n");
    }
		
		else if (tid_Scan == NULL) {
      reply(addr, "ERR INTERNAL\n");
    }
		
		else {
      scan_debut = d_start;
      scan_fin = d_end;
      scan_pas = d_step;
      scan_client_addr = *addr;
      scan_en_cours = 1;
      osThreadFlagsSet(tid_Scan, FLAG_START_SCAN);
    }
  }
	
  else {
    reply(addr, "ERR UNKNOWN\n");
  }

  return 0;
}

/* Initialise la pile réseau, la tâche de scan et ouvre le socket UDP */
void Init_Ethernet(void) {
  netInitialize();

  if (tid_Scan == NULL) {
    tid_Scan = osThreadNew(Thread_Scan, NULL, NULL);
  }

  udp_sock = netUDP_GetSocket(udp_cb_func);
  if (udp_sock > 0) {
    netUDP_Open(udp_sock, UDP_PORT);
  }
}