#include "tinyweb.h"
#include "./wrapper/csapp.h"
#include <stdio.h>
// #define MAXLINE 50

void sigchld_handler(int sig){
    int olderrno=errno;
    pid_t pid;
    while((pid=waitpid(-1,NULL,0))>0){
        printf("subprocess [%d] was reaped!-----------------------\n",pid);
    }
    if (errno!=ECHILD)
        Sio_error("waitpid error");
    errno=olderrno;
}

int main(int argc, char **argv){
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    if (argc!=2){
        fprintf(stderr,"Usage: %s <port>\n",argv[0]);
        exit(1);
    }
    
    //regist signal handler here.
    Signal(SIGCHLD,sigchld_handler);

    listenfd=Open_listenfd(argv[1]);
    while(1){
        clientlen=sizeof(struct sockaddr_storage);
        connfd=Accept(listenfd,(SA*)&clientaddr,&clientlen);
        Getnameinfo((SA*)&clientaddr,clientlen,hostname,MAXLINE,port,MAXLINE,0);
        printf("Accept connection from (%s:%s)\n",hostname,port);
        doit(connfd);
        Close(connfd);
    }

}