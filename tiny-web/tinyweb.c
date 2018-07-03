#include "./wrapper/csapp.h"
#include "tinyweb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #define MAXLINE 50

//处理http事务，fd为连接描述符
void doit(int fd){
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE],method[MAXLINE],uri[MAXLINE],version[MAXLINE];
    char filename[MAXLINE],cgiargs[MAXLINE];
    rio_t rio;
    
    Rio_readinitb(&rio,fd);
    Rio_readlineb(&rio,&buf,MAXLINE);
    printf("Request Headers:\n");
    printf("%s",buf);
    sscanf(buf,"%s %s %s",method,uri,version);//类比sprintf
    if (!strcasecmp(method,"GET")){
        read_requestheaders(&rio);

        /*Parse URI from GET request*/
        is_static = parse_uri(uri,filename,cgiargs);
        if (stat(filename,&sbuf)<0){
            clienterror(fd,filename,"404","Not Found","TinyWeb couldn't find this file");
            return;
        }

        if(is_static){
            if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
                clienterror(fd,filename,"403","Forbidden","TinyWeb couldn't read this file");
                return;
            }

            serve_static(fd,filename,sbuf.st_size);
        }
        else{
            if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
                clienterror(fd,filename,"403","Forbidden","TinyWeb could't run this file");
                return;        
            }
            serve_dynamic(fd,filename,cgiargs);
        }
    }else if(!strcasecmp(method,"POST")){
        //post method implemented here
    }else if(!strcasecmp(method,"PUT")){
        //put method implemented here
    }else if(!strcasecmp(method,"DELETE")){
        //delete method implemented here
    }else{
        clienterror(fd,method,"501",
        "Not implemented","TinyWeb does not support this method");
        return;
    }   
}


void clienterror(int fd,char* cause,char* errnum,char* shortmsg, char* longmsg){
    char buf[MAXLINE],body[MAXLINE];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

void read_requestheaders(rio_t *rp){
    char buf[MAXLINE];
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while(strcmp(buf, "\r\n")) {          
	Rio_readlineb(rp, buf, MAXLINE);
	printf("%s", buf);
    }
    return;
}


int parse_uri(char *uri, char *filename, char *cgiargs) 
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {  /* Static content */
	strcpy(cgiargs, "");                            
	strcpy(filename, ".");                           
	strcat(filename, uri);                           
	if (uri[strlen(uri)-1] == '/')                   
	    strcat(filename, "home.html");              
	return 1;
    }
    else {  /* Dynamic content */                        
	ptr = index(uri, '?');                           
	if (ptr) {
	    strcpy(cgiargs, ptr+1);
	    *ptr = '\0';
	}
	else 
	    strcpy(cgiargs, "");                         
	strcpy(filename, ".");                           
	strcat(filename, uri);                           
	return 0;
    }
}

void serve_static(int fd, char *filename, int filesize) 
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];
 
    /* Send response headers to client */
    get_filetype(filename, filetype);       
    sprintf(buf, "HTTP/1.0 200 OK\r\n");    
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));      
    printf("Response headers:\n");
    printf("%s", buf);

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0);    
    
    //将被请求文件内容映射到一个虚拟地址空间
    //调用mmap将文件srcfd的前filesize个字节
    //映射到一个从地址srcp开始的私有只读虚拟内存区域
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);//因为已经将文件映射到了内存，所以不在需要文件描述符了                           
    Rio_writen(fd, srcp, filesize);        
    Munmap(srcp, filesize);

    //上面使用的是Mmap,Munmap,也可以使用下面的方式
    // char *data = (char*)malloc(filesize * sizeof(char));
    // while(rio_readn(srcfd, data, filesize) > 0) {
    //     rio_writen(fd, data, filesize);
    // }
    // Close(srcfd);
   
}


void get_filetype(char *filename, char *filetype) 
{
    if (strstr(filename, ".html"))
	    strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
	    strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
	    strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
	    strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mpg"))
        strcpy(filetype, "video/mpeg");
    else
	    strcpy(filetype, "text/plain");
}  


/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs) 
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
  
    if (Fork() == 0) { /* Child */ 
	/* Real server would set all CGI vars here */
	setenv("QUERY_STRING", cgiargs, 1); 
	Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */
	Execve(filename, emptylist, environ); /* Run CGI program */ 
    }

    //now subprocess was reaped by SIGCHLD handler
    // Wait(NULL); /* Parent waits for and reaps child */ //line:netp:servedynamic:wait
}
