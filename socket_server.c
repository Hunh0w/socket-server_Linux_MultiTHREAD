#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#define PORT 9242

typedef struct CThread CThread;
typedef struct CLIENT CLIENT;
CThread createThread(pthread_t id, void *ptr_func, CLIENT* client);
void memfree_client(CLIENT* client);
void broadcast(char* str, int sockexcept);
void *recvThread(void *clientptr);

struct CThread {
    pthread_t thread_ID;
    void *thread_func;
};
struct CLIENT {
    int sock;
    struct sockaddr_in address;
    socklen_t addrlen;

    char* ipaddr;
    char* buffer;
    int buffer_len;

    CThread thrrecv;
};

static unsigned long currentID = 1;
static CLIENT clients[9999];

void broadcast(char* str, int sockexcept){
    for(int i = 0; i < 9999; i++){
        if(clients[i].buffer == NULL) continue;
        if(sockexcept != -2)
            if(sockexcept == clients[i].sock) continue;
        send(clients[i].sock, str, strlen(str), 0);
    }
}
void *recvThread(void *clientptr){
    CLIENT* client = (CLIENT*)clientptr;
    client->buffer_len = 4000;
    client->buffer = malloc(client->buffer_len*sizeof(char));
    int nulrecv = 0;
    while(1){
        int recvcode = read(client->sock, client->buffer, client->buffer_len);
        if(recvcode < 0) break;
        if(!strcasecmp(client->buffer, "exit\r\n") || !strcasecmp(client->buffer, "quit\r\n"))
            break;
        
        unsigned long stlen = strlen(client->buffer);
        if(client->buffer[stlen-1] == '\n') client->buffer[stlen-1] = '\0';
        if(client->buffer[stlen-2] == '\r') client->buffer[stlen-2] = '\0';
        if(strlen(client->buffer) <= 0){
            nulrecv++;
            if(nulrecv >= 100) break;
            continue;
        }
        nulrecv = 0;
        printf("[%s] (%ld) : %s\n", client->ipaddr, strlen(client->buffer), client->buffer);

        char buffer[4025] = "[";
        strcat(&buffer[0], client->ipaddr);
        strcat(&buffer[0], "] : ");
        strcat(&buffer[0], client->buffer);
        broadcast(&buffer[0], client->sock);
        memset(client->buffer,0,strlen(client->buffer));
    }
    printf("Déconnexion du client %s ...\n", client->ipaddr);
    char buffer[100] = "Déconnexion du client ";
    strcat(&buffer[0], client->ipaddr);
    strcat(&buffer[0], "...");
    broadcast(&buffer[0], client->sock);
    close(client->sock);
    memfree_client(client);
    return (void*)NULL;
}

CThread createThread(pthread_t id, void *ptr_func, CLIENT* client){
    printf("Création du thread %ld...\n", id);
    CThread thr;
    thr.thread_func = ptr_func;
    thr.thread_ID = id;
    int retc = pthread_create(&id, NULL, ptr_func, (void*)client);
    int retd = pthread_detach(id);
    printf("Thread créé\n");
    if(retc != 0 || retd != 0){
        printf("Erreur lors de la création du Thread %ld de %s\n", id, client->ipaddr);
        thr.thread_ID = -1;
    }
    return thr;
}

int main(int argc, char* argv[]){
    int server_fd;
    int opt = 1;
    
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0){
        printf("socket Error: %d\n", errno);
        exit(EXIT_FAILURE);
    }
    printf("Socket créé\n");

    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
        printf("setsockopt Error: %d\n", errno);
        exit(EXIT_FAILURE);
    }
    printf("sockopts défini\n");

    struct sockaddr_in address;
    unsigned int addrlen = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    printf("structure crée\n");

    if(bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0){
        printf("bind Error: %d\n", errno);
        exit(EXIT_FAILURE);
    }
    printf("Bind success\n");

    if(listen(server_fd, 3) < 0){
        printf("listening Error: %d\n", errno);
        exit(EXIT_FAILURE);
    }
    printf("écoute sur le port %d...\n", PORT);

    int fails = 0;
    while(1){
        int new_socket;
        if((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0){
            printf("accept Error: %d\n", errno);
            fails++;
            if(fails >= 5) exit(EXIT_FAILURE);
        }
        fails = 0;
        if(currentID >= 9999) {
            close(new_socket);
            printf("Il y a trop de clients ..\n");
            continue;
        }
        clients[currentID].sock = new_socket;
        clients[currentID].address = address;
        clients[currentID].addrlen = addrlen;
        clients[currentID].ipaddr = inet_ntoa(address.sin_addr);
        printf("Connexion acceptée [ %s ] !\n\n", clients[currentID].ipaddr);
        char buffer[150] = "";
        strcat(&buffer[0], clients[currentID].ipaddr);
        strcat(&buffer[0], " s'est connecté !");
        broadcast(&buffer[0], new_socket);
        CThread thr = createThread(currentID, recvThread, &clients[currentID]);
        if(thr.thread_ID < 0){
            close(new_socket);
            memfree_client(&clients[currentID]);
            continue;
        }
        clients[currentID].thrrecv = thr;
        currentID++;
    }
    
    return 0;
}

void memfree_client(CLIENT* client){
    client->ipaddr = NULL;
    client->buffer = NULL;
}
