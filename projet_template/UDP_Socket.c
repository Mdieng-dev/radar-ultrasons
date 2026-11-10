/*------------------------------------------------------------------------------
 * MDK Middleware - Component ::Network:Service
 * Copyright (c) 2004-2019 Arm Limited (or its affiliates). All rights reserved.
 *------------------------------------------------------------------------------
 * Name:    UDP_Socket.c
 * Purpose: UDP Socket Code Template
 * Rev.:    V7.0.0
 *----------------------------------------------------------------------------*/
//! [code_UDP_Socket]
#include "rl_net.h"
#include "stm32f7xx.h"                  // Device header
#include "AppWizard.h"
#include <stdio.h>
<<<<<<< HEAD
#include <string.h>

//------- CONSTANT
#define UDP_PORT 5000
 
=======

#define UDP_PORT 5000

>>>>>>> 424c8c0316fd9cd369dc2d05654b014940417d7f
int32_t udp_sock;                       // UDP socket handle
 
 
extern void  servo_set_angle(int deg);
extern float distance_cm(void);    // < 0 si pas d'écho

/* Envoie un texte à l'expéditeur */
static void reply(const NET_ADDR *addr, const char *msg) {
  uint32_t n = strlen(msg);
  uint8_t *b = netUDP_GetBuffer(n);
  if (b != NULL) {
    memcpy(b, msg, n);
    netUDP_Send(udp_sock, addr, b, n);
  }
}

// Notify the user application about UDP socket events.
<<<<<<< HEAD
uint32_t udp_cb_func (int32_t socket, const NET_ADDR *addr, const uint8_t *buf, uint32_t len) {
  uint32_t i;
  int angle = 0;
  int d10;
  float d;
  char rep[32];

  // DIST : au moins 4 octets, et ce sont "DIST"
  if (len >= 4 && memcmp(buf, "DIST", 4) == 0) {
    d = distance_cm();
    if (d < 0) {
      reply(addr, "DIST TIMEOUT\n");
    } else {
      d10 = (int)(d * 10 + 0.5f);
      sprintf(rep, "DIST %d.%d\n", d10 / 10, d10 % 10);
      reply(addr, rep);
    }
  }
  // ANGLE : "ANGLE " puis au moins un chiffre
  else if (len >= 7 && memcmp(buf, "ANGLE ", 6) == 0) {
    for (i = 6; i < len && buf[i] >= '0' && buf[i] <= '9' && angle <= 180; i++) {
      angle = angle * 10 + (buf[i] - '0');
    }
    if (i == 6) {
      reply(addr, "ERR FORMAT\n");      // aucun chiffre après "ANGLE "
    } else if (angle > 180) {
      reply(addr, "ERR RANGE\n");
    } else {
      servo_set_angle(angle);
      reply(addr, "OK\n");
    }
  }
  else {
    reply(addr, "ERR UNKNOWN\n");
=======
uint32_t udp_cb_func (int32_t socket, const  NET_ADDR *addr, const uint8_t *buf, uint32_t len) {
	int val_angle = 0;
  // Data received
  if ((buf[0] == 0x41) && (buf[1] == 0x4E) && (buf[2] == 0x47) && (buf[3] == 0x4C) && (buf[4] == 0x45)) {
>>>>>>> 424c8c0316fd9cd369dc2d05654b014940417d7f
  }
  return (0);
}
 
// Send UDP data to destination client.
void send_udp_data (void) {
 
  if (udp_sock > 0) {
<<<<<<< HEAD
    // Example
=======
>>>>>>> 424c8c0316fd9cd369dc2d05654b014940417d7f
    // IPv4 address: 192.168.0.1
    NET_ADDR addr = { NET_ADDR_IP4, 5000, 192, 168, 0, 1 };
    uint8_t *sendbuf;
 
    sendbuf = netUDP_GetBuffer (2);
    sendbuf[0] = 0x01;
    sendbuf[1] = 0xAA;
 
    netUDP_Send (udp_sock, &addr, sendbuf, 2);
<<<<<<< HEAD
    
=======
>>>>>>> 424c8c0316fd9cd369dc2d05654b014940417d7f
  }
}
 
// Allocate and initialize the socket.
void Init_Ethernet (void) {
 
  netInitialize ();
 
  // Initialize UDP socket and open port 5000
  udp_sock = netUDP_GetSocket (udp_cb_func);
  if (udp_sock > 0) {
    netUDP_Open (udp_sock, UDP_PORT);
  }
}

//! [code_UDP_Socket]

/* Single Port Forwarding :

port externe : 8003
port interne : 5000
protocole : UDP.
adresse IP : 192.168.0.15
règle activée

3. Depuis le PC(raspberry), côté WAN, à travers le routeur (dans cmd, avec ncat si nc n’existe pas) :

echo "ANGLE 90"| nc -u -w1 <IP WAN du routeur> 8000

4. Dans Wireshark, sur le PC, utilise le filtre udp.port == 8003. On voit normalement partir le datagramme et revenir la réponse. 

Comment Commit ?
git add .
git commit -m "Serveur UDP : commandes ANGLE et DIST avec réponses ERR"
git push

*/
