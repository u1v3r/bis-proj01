/*------------------------------------------------------------------------------

    1. Projekt na BIS

    Autor: Radovan Dvorsky, xdvors08

------------------------------------------------------------------------------*/
#define _POSIX_C_SOURCE 199506L

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

#define DEBUG

#define SERVER_PORT 8000
#define BUFFER_SIZE 256
#define BACKLOG_QUEUE 5

pthread_t thread;
int sockfd, newsockfd;
pid_t app_pid;
struct sockaddr_in server_addr;

void *handle_client(void*);

int main(int argc, char **argv)
{

    #ifdef DEBUG
        printf("Application pid: %d\n",getpid());
    #endif

    struct sockaddr_in client_addr;
    pthread_attr_t attr;
    int res, client_len, yes = 1;
    app_pid = getpid();


    /* inicializacia atributov pre vlakno */
    if((res = pthread_attr_init(&attr)) != 0){
        printf("pthread_attr_init() error %d\n",res);
        return 1;
    }

    /* nesakaj na join */
    if((res = pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED)) != 0){
        printf("pthread_attr_setdetachstate() error %d\n",res);
        return 1;
    }

    /* Vytvorenie socketu */
    if((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0){
        perror("Opening socket");
        exit(1);
    }

    /* po restarte umozni znovu bind*/
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            perror("setsockopt");
            exit(1);
    }

    /* Inicializacia addries socketu */
    bzero((char *) &server_addr,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);


    if(bind(sockfd,(struct sockaddr *  ) &server_addr, sizeof(server_addr)) < 0){
        perror("Server binding");
        exit(1);
    }


    /* pocuvaj na danom sockete */
    if(listen(sockfd,BACKLOG_QUEUE) != 0){
        perror("listen");
        exit(1);
    }

    client_len = sizeof(client_addr);

    while(1){

        /* cakaj az kym sa niekdo nepripoji */
        newsockfd = accept(sockfd, (struct sockaddr *) &client_addr, (socklen_t*)&client_len);

        if(newsockfd < 0){
            perror("Server accept");
            exit(1);
        }

        #ifdef DEBUG
            printf("Client connected from %s:%i\n",
                    inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        #endif
        if(pthread_create(&thread,&attr,handle_client,(void *)&newsockfd)){
                perror("pthread_create");
                exit(1);
        }
    }


    /* atributy uz netreba */
    if((res = pthread_attr_destroy(&attr)) != 0){
        printf("pthread_attr_destroy() error %d\n",res);
        return 1;
    }


    return 0;
}

void *handle_client(void* param){

    int *client_socket_addr = (int*)param;
    int n, client_socket = *client_socket_addr;
    char buffer[BUFFER_SIZE];

    #ifdef DEBUG
        printf("Client socket id: %d\n",client_socket);
    #endif

    while(1){

        /* nastavenie bufferu */
        bzero(buffer,BUFFER_SIZE);

        /* citanie zo socketu */
        if((n = read(client_socket,buffer,BUFFER_SIZE)) < 0){
            perror("Reading from socket");
               exit(1);
        }

        #ifdef DEBUG
            printf("Message from %d: %s\n",client_socket,buffer);
        #endif

        if(write(client_socket,buffer,strlen(buffer)) == -1){
            perror("Socket writing");
        }
    }


    return (void *)1;
}
