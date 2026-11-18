# Application PC Qt — client UDP du radar à ultrasons

Ce document décrit le travail réalisé dans `pc-Qt/client_test` pour la séance 3
du mini-projet : un client graphique Qt qui remplace netcat pour piloter la
carte STM32 à travers le routeur. Le dossier `projet_template` n'a pas été modifié.

## 1. Ce que demande le sujet

D'après le §4 « Application PC » et la séance 3 du sujet, l'application doit :

- permettre de saisir l'adresse du routeur et le port dans l'interface, sans les coder en dur ;
- envoyer `ANGLE <a>` et `DIST`, afficher la réponse et gérer l'absence de
  réponse avec un **délai d'attente puis un nouvel essai** ;
- envoyer `SCAN <début> <fin> <pas>`, séparer les lignes jusqu'à `END` et
  afficher le tableau angle / distance ;
- **rester réactive** pendant l'attente : la fenêtre ne doit pas se figer ;
- fermer proprement ses sockets et afficher les erreurs dans l'interface ;
- avoir un **module réseau écrit en C** (sockets : `socket`, `sendto`, `recvfrom`),
  testé d'abord sans interface.

Protocole côté carte (voir `projet_template/UDP_Socket.c`) :

| Commande envoyée | Réponse attendue |
|---|---|
| `ANGLE <a>\n` (0 à 180) | `OK` ou `ERR <motif>` |
| `DIST\n` | `DIST <cm>` (une décimale) ou `DIST TIMEOUT` |
| `SCAN <d> <f> <p>\n` | un seul datagramme : lignes `<angle> <cm>`, puis `END` |
| commande inconnue ou hors plage | `ERR <motif>` |

## 2. Architecture

```
┌────────────────────── Thread de l'interface (Qt) ─────────────────────┐
│ Dialog (dialog.cpp / dialog.ui)                                       │
│  - lit et valide les champs, construit une RadarRequest               │
│  - émet radarRequested(req)  ──────────────┐   (connexion "queued")   │
│  - reçoit finished(res) et met à jour      │                          │
│    tableau, distance, journal, statut  ◄───┼──────────┐               │
└────────────────────────────────────────────┼──────────┼───────────────┘
┌──────────────── Thread réseau (QThread) ───▼──────────┼───────────────┐
│ Radar_Thread::process(req)  (radar_Thread.cpp)        │               │
│   └─ appelle le module C (bloquant) ──────────────────┘               │
└───────────────────────────────────────────────────────────────────────┘
┌──────────────────────── Module C ─────────────────────────────────────┐
│ udp_client.c :                                                        │
│  - transport : Winsock, sendto/recvfrom, délai d'attente, nouvel essai│
│  - protocole : formate ANGLE/DIST/SCAN, analyse les réponses          │
└───────────────────────────────────────────────────────────────────────┘
```

Le code est découpé en deux parties :

1. **`udp_client.c/.h`** (C) : transport UDP générique (partie 1) et protocole
   texte du radar (partie 2), dans un seul module.
2. **Qt** (`radar_Thread.*`, `dialog.*`, `main.cpp`) : thread réseau et interface.

S'y ajoute **`udp_cli.c`**, un client console qui utilise le même module C
pour le tester sans interface.

## 3. Détail des fichiers

### 3.1 `udp_client.c` (partie 1) : transport UDP (Winsock)

Fonction principale :

```c
int udp_request(const char *host, unsigned short port, const char *request,
                char *reply, size_t reply_size, int *reply_len,
                int timeout_ms, int retries, const udp_hooks_t *hooks);
```

Déroulement d'un appel :

1. `getaddrinfo` résout l'adresse saisie (IP ou nom d'hôte, IPv4).
2. `socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)` crée le socket.
3. Pour chaque essai (`retries + 1` au total) :
   - `sendto` envoie la commande ;
   - on attend une réponse pendant `timeout_ms` avec `select()`, par
     **tranches de 100 ms** ;
   - si un datagramme est lisible, `recvfrom` le lit. S'il ne vient pas de
     l'adresse et du port visés, il est ignoré et l'attente continue ;
   - sinon, on passe à l'essai suivant.
4. `closesocket` est appelé **dans tous les cas**, y compris en cas d'erreur ou d'annulation.

Choix techniques :

- **`select()` par tranches plutôt que `SO_RCVTIMEO`** : on peut vérifier
  toutes les 100 ms si l'utilisateur a cliqué sur « Annuler » ou fermé la
  fenêtre, sans attendre la fin du délai.
- **Rappels `udp_hooks_t`** (`cancelled`, `on_attempt`) : le module C reste
  indépendant de Qt, mais Qt peut annuler et afficher « essai 2/3 ».
- **Particularité Windows `WSAECONNRESET`** : quand un ICMP « port
  inaccessible » revient (carte absente, mauvais port), Windows fait échouer
  le `recvfrom` suivant avec l'erreur 10054. On le traite comme une perte et on
  passe directement à l'essai suivant. Si cela se produit à chaque essai, le
  code `UDP_ERR_UNREACH` donne un diagnostic plus précis qu'un simple délai dépassé.
- **Un socket par requête** : une réponse tardive d'une ancienne requête
  ne peut pas être confondue avec la réponse de la requête suivante.
- Codes d'erreur explicites (`UDP_ERR_TIMEOUT`, `UDP_ERR_ADDRESS`, …) et
  `udp_strerror()` pour les afficher. Le dernier code Winsock est conservé
  par thread (`udp_last_wsa_error()`).
- `udp_global_init()` / `udp_global_cleanup()` encapsulent
  `WSAStartup` / `WSACleanup`, appelés une seule fois dans `main.cpp`.

### 3.2 `udp_client.c` (partie 2) : commandes ANGLE, DIST, SCAN

- `radar_angle()`, `radar_dist()`, `radar_scan()` construisent la commande
  terminée par `\n`, l'envoient via `udp_request()`, puis analysent la réponse.
- Les paramètres sont vérifiés **avant l'envoi** (angle de 0 à 180, pas > 0) :
  `RADAR_ERR_RANGE`.
- Analyse des réponses, faite par des fonctions pures, testables sans réseau :
  - `ANGLE` : `OK` → succès ; `ERR …` → `RADAR_ERR_BOARD` ; autre chose → `RADAR_ERR_PROTOCOL`.
  - `DIST` : `DIST 23.5` → distance ; `DIST TIMEOUT` ou valeur négative → « pas d'écho ».
  - `SCAN` : la réponse est découpée ligne par ligne. Chaque ligne doit contenir
    exactement deux éléments, `<angle> <cm|TIMEOUT>`. La lecture s'arrête à `END`.
    Si `END` est absent, la réponse est rejetée comme **incomplète**.
- Les `\r`, espaces et lignes vides parasites sont tolérés.
- Les décimaux sont lus **sans `strtod`/`%lf`**, car ces fonctions dépendent de la
  locale : en français elles attendent une virgule, alors que la carte envoie un point.

Pour SCAN, le délai d'attente est **séparé** et plus long (10 s par défaut),
car la carte déplace le servo et mesure à chaque angle avant de répondre.

### 3.3 `radar_Thread.cpp/.h` : interface non bloquante

Les fonctions C sont bloquantes. Appelées directement dans un slot de bouton,
elles bloqueraient la boucle d'événements Qt et la fenêtre se figerait
(« Ne répond pas »). Elles s'exécutent donc dans un **`QThread` dédié** :

- `Radar_Thread` est déplacé dans le thread par `moveToThread`.
- Le dialogue émet `radarRequested(RadarRequest)`. Comme le worker vit dans un
  autre thread, Qt livre le signal **en file d'attente** et `process()`
  s'exécute dans le thread réseau. Le clic retourne immédiatement.
- À la fin, le worker émet `finished(RadarResult)`, qui revient dans le thread
  de l'interface. Seul ce thread modifie les widgets.
- Les données échangées sont **copiées** (`RadarRequest`, `RadarResult`,
  déclarées avec `Q_DECLARE_METATYPE`). Aucun widget n'est lu depuis le thread réseau.
- L'annulation passe par un `std::atomic_bool`, lu par le rappel C `cancelled`.

### 3.4 `dialog.cpp/.ui` : interface

- **Connexion** : adresse du routeur, port (8000 par défaut, le port externe
  de la redirection), délai d'attente, nombre de nouveaux essais, délai SCAN.
  Ces réglages sont mémorisés entre deux lancements (`QSettings`).
- **Servo et mesure** : angle (0 à 180), boutons « Envoyer ANGLE » et
  « Mesurer DIST », affichage de la dernière distance.
- **Balayage** : début, fin, pas, bouton « Lancer SCAN ». Le tableau
  angle / distance affiche « pas d'écho » pour les TIMEOUT.
- **Journal horodaté** : commande envoyée (→), réponse brute (←) avec la durée,
  nouveaux essais, erreurs.
- **Barre d'état** : barre de progression animée pendant l'attente, message
  « essai n/N », erreurs en rouge, bouton **Annuler**.
- Pendant une requête, les boutons de commande et les champs de connexion sont
  grisés : une seule requête à la fois.
- Validation des saisies : adresse non vide, sans espace ni caractère non
  ASCII ; début ≤ fin pour SCAN. Les bornes des champs numériques sont imposées
  par les `QSpinBox`.
- **Fermeture propre** : le destructeur demande l'annulation, arrête le thread
  et attend sa fin. Le module C ferme alors son socket. `WSACleanup()` est
  appelé dans `main.cpp` seulement après la destruction de la fenêtre.

### 3.5 `CMakeLists.txt`

- Le langage C est activé (`LANGUAGES C CXX`, C11).
- Les sources C et Qt sont ajoutées, et la bibliothèque **`ws2_32`** (Winsock 2) est liée.
- Une seconde cible, **`udp_cli`**, compile le client console.

## 4. Utilisation

### Application graphique

Ouvrir `pc-Qt/client_test/CMakeLists.txt` dans Qt Creator (kit Qt 6 MinGW 64 bits),
compiler et lancer `client_test`. Saisir l'**adresse WAN du routeur** et le
**port externe** de la redirection (ex. 8000 → 5000 sur la carte).

Pour tester depuis le LAN sans la redirection : IP de la carte et port 5000.

### Client console (test sans interface)

```
udp_cli [-t délai_ms] [-r essais] <hôte> <port> ANGLE <a>
udp_cli [-t délai_ms] [-r essais] <hôte> <port> DIST
udp_cli [-t délai_ms] [-r essais] <hôte> <port> SCAN <début> <fin> <pas>
udp_cli --selftest        # tests des analyseurs de réponses, sans réseau
```

## 5. Tests réalisés

La carte n'était pas disponible : les tests ont été faits contre un
**simulateur de carte** (petit serveur UDP en C sur 127.0.0.1). Il applique le
même protocole et **perd volontairement un datagramme sur trois**.

| Test | Résultat |
|---|---|
| Compilation avec `-Wall -Wextra` (Qt 6.12, MinGW 13.1) | aucun avertissement |
| `udp_cli --selftest` (OK/ERR, DIST, DIST TIMEOUT, SCAN avec `\r\n`, END absent, ligne mal formée, trop de points) | tous les tests passent |
| `ANGLE 45` avec 1er datagramme perdu | `OK` reçu au 2ᵉ essai |
| `SCAN 0 180 30` avec 1er datagramme perdu | 7 mesures, dont `90 TIMEOUT` affiché comme « pas d'écho » |
| `ANGLE 200` | refusé avant envoi (paramètre hors plage) |
| Port fermé (rien n'écoute) | `port inaccessible (ICMP)` en ~70 ms (WSA 10054) |
| Hôte muet, délai 400 ms, 1 nouvel essai | abandon après ~0,9 s, `pas de réponse` |
| Nom d'hôte invalide | `adresse invalide ou introuvable` |
| Interface : ANGLE, DIST, SCAN 0–180 pas 10 | 19 lignes affichées dans le tableau |
| Réactivité : minuteur témoin de 20 ms pendant un SCAN | boucle d'événements jamais bloquée plus de ~33 ms |
| Bouton Annuler pendant une attente | « Commande annulée » en moins de 100 ms |
| Fermeture de la fenêtre pendant une attente | fermeture immédiate, sans blocage |
| Adresse vide | message d'erreur en rouge dans l'interface |

**À faire avec la vraie carte** : test à travers le routeur (adresse WAN,
port 8000 → 5000), capture Wireshark (`udp.port == 8000`) et réglage du
délai SCAN selon la durée réelle du balayage.

## 6. Points d'attention pour l'intégration

- La commande `SCAN` n'est pas encore présente dans `UDP_Socket.c` : la carte
  répond pour l'instant `ERR UNKNOWN`, que l'application affiche comme une
  erreur de la carte. Le format attendu est une ligne `<angle> <cm>` par mesure,
  ou `<angle> TIMEOUT` sans écho, puis `END`, dans un seul datagramme.
- Côté carte, `udp_cb_func()` ne doit pas bloquer trop longtemps pendant un
  SCAN. Si le balayage est long, augmenter le « Délai SCAN » de l'application.
- Renvoyer une commande est sans danger : `ANGLE` et `DIST` sont idempotentes.
  Si la réponse d'un `SCAN` est perdue, le nouvel essai relance un balayage complet.

## 7. Pistes d'amélioration

- Affichage graphique « radar » (vue polaire) des mesures du balayage.
- Export du tableau en CSV.
- Envoi du SCAN par paquets (plusieurs commandes) pour des balayages très fins.
