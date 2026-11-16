#include <QCoreApplication>
#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

//Penser à ajouter la bibliothèque winsock2 dans le projet quitter
//ajouter une ligne au fichier .pro
//LIBS += -lws2_32

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    WSADATA WSAData; // variable initialisée par WSAStartup
    WSAStartup(MAKEWORD(2,0), &WSAData); // indique la version utilisée, ici 2.0

    SOCKET descripteurSocket;
    int iResult;

    // ===> Crée un socket de communication : on utilisera le protocole par défaut (valeur 0) associé à SOCK_STREAM (soit TCP)
    // 
    
    if (descripteurSocket == INVALID_SOCKET) {
        printf("Erreur creation socket : %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    struct sockaddr_in pointDeRencontreDistant; // ou SOCKADDR_IN pointDeRencontreDistant;
    // Renseigne la structure sockaddr_in avec les informations du serveur distant
    pointDeRencontreDistant.sin_family = AF_INET;
    // ===> On choisit l’adresse IPv4 du serveur : à modifier selon ses besoins
    
    // ===> On choisit le numéro de port d’écoute du serveur
    

    // ===> Lance la demande de connexion vers le processus serveur distant
    iResult = 
    if (iResult == SOCKET_ERROR) {
        printf("Erreur connexion socket : %d\n", WSAGetLastError());
        iResult = closesocket(descripteurSocket); // On ferme la ressource avant de quitter
        if (iResult == SOCKET_ERROR){
            printf("Erreur fermeture socket : %d\n", WSAGetLastError());
        }
        WSACleanup();
        return 1; // On sort en indiquant un code erreur
    }

    printf("Connexion au serveur reussie ... !\n");

    char msg[256]; /* le message à envoyer, de la part de la couche Application ! */
    int ecrits,recus; /* nb d’octets envoyés  */

    // ===> Initialiser le buffer d'emission avec le message à envoyer
    sprintf(messageEnvoi,                                    );

    // ==> Effectuer l'envoi du message fraichement fabriqué
    ecrits = 
    if (ecrits == SOCKET_ERROR) {
        printf("Erreur envoi socket : %d\n", WSAGetLastError());
        closesocket(descripteurSocket);
        WSACleanup();
        return 1;
    }
    printf("Message %s envoye (%d octets)\n\n", msg, ecrits);

    // ==> Effectuer la réception de la réponse du serveur
    recus = 
    if (recus == SOCKET_ERROR) {
        printf("Erreur reception socket : %d\n", WSAGetLastError());
        closesocket(descripteurSocket);
        WSACleanup();
        return 1;
    }
    printf("Message recu : %s (%d octets)\n\n",msg);

    while(!getch());
    // ===> Fermer la socket : provoque la déconnexion du serveur
    iResult = 
    if (iResult == SOCKET_ERROR) {
        printf("Erreur fermeture socket : %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    WSACleanup(); // termine l’utilisation

    return a.exec();
}
