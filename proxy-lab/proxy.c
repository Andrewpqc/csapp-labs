#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csapp.h"

#include "./sbuf/sbuf.h"
#include "./lrucache/lru_cache.h"
#include "./lrucache/lru_cache_impl.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

void *LruCache; //全局的缓存器变量


/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) \
                                        Gecko/20120305 Firefox/10.0.3\r\n";




void doit(int fd,char *request_host, char *request_port);
void read_requestheaders(rio_t *rp);
int parse_uri(char* uri,char*filename,char* cgiargs);
void serve_static(int fd,char* filename,int filesize);
void get_filetype(char* filename,char* filetype);
void serve_dynamic(int fd, char*filename,char* cgiargs);
void clienterror(int fd, char* cause,char*errnum,char*shortmsg,char*longmeg);

char *forward_to_upstream(rio_t *rio,char *upstream_host,char * upstream_port, char *request_line);
//处理http事务，fd为连接描述符
void doit(int fd,char *request_host, char *request_port){
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE],method[MAXLINE],uri[MAXLINE],version[MAXLINE];
    char filename[MAXLINE],cgiargs[MAXLINE],resp_content[MAXLINE];
    char cache_key[KEY_SIZE],cache_value[VALUE_SIZE],request_line[MAXLINE];
    rio_t rio;
    
    Rio_readinitb(&rio,fd);
    Rio_readlineb(&rio,&buf,MAXLINE);
    printf("Request Headers:\n");
    printf("%s",buf);
    sscanf(buf,"%s %s %s",method,uri,version);//类比sprintf
    // read_requestheaders(&rio);

    //非GET请求被拦截
    if (strcasecmp(method,"GET")){
        clienterror(fd,method,"501",
        "Not implemented","TinyWeb does not support this method");
        return;
    }
    
    //根据请求生成标识该请求的key
    sprintf(cache_key,"%s.%s.%s.%s.%s",method,uri,version,request_host,request_port);
    if (NULL == LRUCacheGet(LruCache, cache_key)){
        //未缓存，此时，应该向上游服务器取数据，
        //并且将数据缓存在缓存器中，同时发一份到客户端
        sprintf(request_line,"%s %s %s\r\n",method,uri,version);
        strncmp(resp_content,forward_to_upstream(&rio,"127.0.0.1","8000",request_line),MAXLINE);
        
        // //设置缓存
        // LRUCacheSet(LruCache,cache_key,resp_content);
        
        // //返回响应给客户端
        // Rio_writen(fd, resp_content, strlen(resp_content));


        ///////////////////////////////////////////////////////
        printf("未缓存");
    }
    else{
        char data[VALUE_SIZE];
        strncpy(data, LRUCacheGet(LruCache, cache_key), VALUE_SIZE);
        Rio_writen(fd, data, strlen(data));
        //直接将缓存的数据发到客户端
    }
}


char *forward_to_upstream(rio_t *rio,char *upstream_host,char * upstream_port,char *request_line){
    int upstreamfd;
    char userbuf[MAXLINE];
    char resp_content[MAXLINE];
    rio_t upstream_rio;
     
    upstreamfd = open_clientfd(upstream_host,upstream_port);
   
   
    //将upstream_rio与upstreamfd绑定在一起,
    //上游服务器的响应内容从upstream_rio中读取
    Rio_readinitb(&upstream_rio,upstreamfd);

    //首先发送请求行
    Rio_writen(upstreamfd,request_line,strlen(request_line));
   
    //接下来从客户端边读变送往服务器


    //这里发生了段错误
    Rio_readlineb(rio,userbuf,MAXLINE);
    printf("userbuf:%s",userbuf);
    fflush(stdout);
    Rio_writen(upstreamfd,userbuf,MAXLINE);
    while(strncmp(userbuf,"\r\n",MAXLINE)){
        Rio_readlineb(rio,userbuf,MAXLINE);
        Rio_writen(upstreamfd,userbuf,MAXLINE);
    }
    Rio_writen(upstreamfd,"\r\n",MAXLINE); //发送空行表示请求结束
    
    //发送完毕就需要接收服务器的返回内容了    
    if(!Rio_readlineb(&upstream_rio,userbuf,MAXLINE)){
    //     //上游服务器没有响应任何内容
    }
    sprintf(resp_content,"%s",userbuf);
    printf(">>>>>>>>>>>>>>>>");
    while(Rio_readlineb(&upstream_rio,userbuf,MAXLINE)){//EOF时为0
        printf("mmmmm:%s",userbuf);
        fflush(stdout);
        sprintf(resp_content,"%s%s",resp_content,userbuf);
    }
    
    return resp_content;
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
    fflush(stdout);
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




//the main code
int main(int argc,char** argv)
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    //检查启动参数
    if (argc!=3){
        fprintf(stderr,"Usage: %s <port> <cache capacity>\n",argv[0]);
        exit(1);
    }

    //创建缓存器
    if (0 != LRUCacheCreate(atoi(argv[2]), &LruCache)){
        printf("Cache create failed!");
        exit(1);
    }

    printf("Server listening on :%s\n",argv[1]);
    fflush(stdout);

   

    listenfd=Open_listenfd(argv[1]);
    while(1){
        clientlen=sizeof(struct sockaddr_storage);
        connfd=Accept(listenfd,(SA*)&clientaddr,&clientlen);
        Getnameinfo((SA*)&clientaddr,clientlen,hostname,MAXLINE,port,MAXLINE,0);
        printf("Accept connection from (%s:%s)\n",hostname,port);
        doit(connfd,hostname,port);
        Close(connfd);
    }
}
