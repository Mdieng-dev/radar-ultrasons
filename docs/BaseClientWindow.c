#include <winsock2.h>

int main()
{
//Spécifique à WINDOWS
    WSADATA WSAData;
    SOCKET sock;
    SOCKADDR_IN addr; 
    
    WSAStartup(MAKEWORD(2,0), &WSAData); 
 //Fin spécificité WINDOWS
    
    addr.sin_family = AF_INET; 
    addr.sin_addr.s_addr = inet_addr("127.0.0.1"); //IP Serveur destinataire de la demande de connexion
    addr.sin_port = htons(61234); //Port d'écoute du serveur destinataire
    
    if (connect(sock,(SOCKADDR *)&addr,sizeof(addr))==-1) 
    { 
        fprintf(stderr,"bind problem");
        closesocket(sock);
        exit(-1); 
    }

    nbSent=write(sock,Msg,strlen(Msg));

    printf("sent %d\n",nbSent); 
    
    closesocket(sock);
    //Spécifique à WINDOWS
    WSACleanup();
    //Fin spécificité WINDOWS
    exit(-1);
}
