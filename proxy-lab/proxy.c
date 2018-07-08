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




void doit(int fd,char *request_host);
void clienterror(int fd, char* cause,char*errnum,char*shortmsg,char*longmeg);
void forward_to_upstream(rio_t *rio,char *upstream_host,char * upstream_port, char *request_line,char *response_content);

void doit(int fd,char *request_host){
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE],method[MAXLINE],uri[MAXLINE],version[MAXLINE];
    char filename[MAXLINE],cgiargs[MAXLINE],response_content[MAXLINE]="";
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
    sprintf(cache_key,"%s.%s.%s.%s",method,uri,version,request_host);
    if (NULL == LRUCacheGet(LruCache, cache_key)){
         printf("未缓存");
         fflush(stdout);
        sprintf(request_line,"%s %s %s\r\n",method,uri,version);
        forward_to_upstream(&rio,"127.0.0.1","8000",request_line,response_content);
        //设置缓存
        LRUCacheSet(LruCache,cache_key,response_content); 
        strncpy(response_content,"",MAXLINE);
        //返回响应给客户端
        Rio_writen(fd, response_content, strlen(response_content));
    }
    else{
        printf("已缓存");
        fflush(stdout);
        char data[VALUE_SIZE]="";
        strncpy(data, LRUCacheGet(LruCache, cache_key), VALUE_SIZE);
        Rio_writen(fd, data, strlen(data));
        //直接将缓存的数据发到客户端
    }
}


void forward_to_upstream(rio_t *rio,char *upstream_host,char * upstream_port,char *request_line,char * response_content){
    int upstreamfd;
    char userbuf[MAXLINE];
    char resp_content[MAXLINE];
    rio_t upstream_rio;
    char content_length[50];
    size_t size=0,content_length_int;
    upstreamfd = open_clientfd(upstream_host,upstream_port);
   
   
    //首先发送请求行
    Rio_writen(upstreamfd,request_line,strlen(request_line));
   
    printf("hhhhhhhhhhhhhh");
    fflush(stdout);

    //接下来从客户端边读变送往服务器
    Rio_readlineb(rio,userbuf,MAXLINE);
    Rio_writen(upstreamfd,userbuf,MAXLINE);
    while(strncmp(userbuf,"\r\n",MAXLINE)){
        Rio_readlineb(rio,userbuf,MAXLINE);
        Rio_writen(upstreamfd,userbuf,MAXLINE);
    }
    Rio_writen(upstreamfd,"\r\n",MAXLINE); //发送空行表示请求结束
    

    //将upstream_rio与upstreamfd绑定在一起,
    //上游服务器的响应内容从upstream_rio中读取
    Rio_readinitb(&upstream_rio,upstreamfd);

    //读取响应头
    Rio_readlineb(&upstream_rio,userbuf,MAXLINE);
    sprintf(resp_content,"%s%s",resp_content,userbuf);
    while(strncmp(userbuf,"\r\n",MAXLINE)){
        Rio_readlineb(&upstream_rio,userbuf,MAXLINE);
        if(strstr(userbuf,"length") != NULL){
            strncpy(content_length,userbuf+16,MAXLINE);
        }
        sprintf(resp_content,"%s%s",resp_content,userbuf);
    }
    
    //如果content_lenght不为0,则继续读取响应体
    content_length_int=atoi(content_length);
    if(content_length_int!=0){
        //读取响应体
        size+=Rio_readlineb(&upstream_rio,userbuf,MAXLINE);
        sprintf(resp_content,"%s%s",resp_content,userbuf);
        while(size<content_length_int){
            size+=Rio_readlineb(&upstream_rio,userbuf,MAXLINE);
            sprintf(resp_content,"%s%s",resp_content,userbuf);
        }
    }
    strncpy(response_content,resp_content,MAXLINE);
    return; 
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
        doit(connfd,hostname);
        Close(connfd);
    }

    // Close(listenfd);
    // LRUCacheDestory(LruCache);
}
