#include "tinyweb.h"
#include "./wrapper/csapp.h"
#include <stdio.h>
// #define MAXLINE 50


int main(int argc, char **argv){
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    if (argc!=2){
        fprintf(stderr,"Usage: %s <port>\n",argv[0]);
        exit(1);
    }

    printf("Server listened on :%s\n",argv[1]);
    // fflush(stdout);
    
    //regist signal handler here.
    Signal(SIGCHLD,sigchld_handler);

    listenfd=Open_listenfd(argv[1]);
    while(1){
        clientlen=sizeof(struct sockaddr_storage);
        connfd=Accept(listenfd,(SA*)&clientaddr,&clientlen);
        if(Fork()==0){
            Close(listenfd); //important here!!!!!
            Signal(SIGCHLD,sigchld_handler);
            Getnameinfo((SA*)&clientaddr,clientlen,hostname,MAXLINE,port,MAXLINE,0);
            printf("Accept connection from (%s:%s)\n",hostname,port);
            doit(connfd);
            Close(connfd);
            exit(1);
        }
        Close(connfd); //important here！！！！！
    }
}


/**
 * 子进程关闭监听描述符，父进程关闭连接描述符
 **/ 