#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#define RIO_BUFSIZE 8192
#define MAXLINE 8192
#define MAXBUF 8192
#define LISTENQ 1024
typedef struct{
int rio_fd;
int rio_cnt;
char *rio_bufptr;
char rio_buf[RIO_BUFSIZE];
}rio_t;
ssize_t rio_readn(int fd,void *usrbuf,size_t n){
size_t nleft=n;
ssize_t nread;
typedef struct sockaddr SA;
char *bufp=usrbuf;
while(nleft>0){
if((nread=read(fd,bufp,nleft))<0){
if(errno=EINTR)
	nread=0;
else
	return -1;
}
else if(nread==0)
	break;
	nleft-=nread;
	bufp+=nread;
}
return (n-nleft);
}
ssize_t rio_writen(int fd,void *usrbuf,size_t n)
{
size_t nleft=n;
ssize_t nwritten;
char *bufp=usrbuf;
while(nleft>0){
   if ((nwritten = write(fd, bufp, nleft)) <= 0) {
        if (errno == EINTR)  
        nwritten = 0;    
        else
        return -1;       
    }
    nleft -= nwritten;
    bufp += nwritten;
    }
    return n;
}
void rio_readinitb(rio_t *rp,int fd)
{
rp->rio_fd=fd;
rp->rio_cnt=0;
rp->rio_bufptr=rp->rio_buf;
}
static ssize_t rio_read(rio_t *rp,char *usrbuf,size_t n)
{
int cnt;
while(rp->rio_cnt<=0){
rp->rio_cnt=read(rp->rio_fd,rp->rio_buf,sizeof(rp->rio_buf));
 if (rp->rio_cnt < 0) {
        if (errno != EINTR)
        return -1;
    }
    else if (rp->rio_cnt == 0)
        return 0;
    else 
        rp->rio_bufptr = rp->rio_buf;
}
    cnt = n;          
    if (rp->rio_cnt < n)   
    cnt = rp->rio_cnt;
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}
ssize_t rio_readnb(rio_t *rp,void *usrbuf,size_t n){
size_t nleft=n;
ssize_t nread;
char *bufp=usrbuf;
 while (nleft > 0) {
    if ((nread = rio_read(rp, bufp, nleft)) < 0) 
            return -1; 
    else if (nread == 0)
        break;
    nleft -= nread;
    bufp += nread;
    }
    return (n - nleft); 
}
ssize_t rio_readlineb(rio_t *rp,void *usrbuf,size_t maxlen)
{
int n, rc;
char c,*bufp=usrbuf;
 for (n = 1; n < maxlen; n++) { 
        if ((rc = rio_read(rp, &c, 1)) == 1) {
        *bufp++ = c;
        if (c == '\n') {
                n++;
            break;
            }
    } else if (rc == 0) {
        if (n == 1)
        return 0; 
        else
        break;  
    } else
        return -1;
    }
    *bufp = 0;
    return n-1;
}
int open_listen_sock(int port)
{
int listen_sock,optval=1;
struct sockaddr_in serveraddr;
if((listen_sock=socket(AF_INET,SOCK_STREAM,0))<0) return -1;
if(setsockopt(listen_sock,SOL_SOCKET,SO_REUSEADDR,(const void *)&optval,sizeof(int))<0)
return-1;
bzero((char *)&serveraddr,sizeof(serveraddr));
serveraddr.sin_family=AF_INET;
serveraddr.sin_addr.s_addr=htonl(INADDR_ANY);
serveraddr.sin_addr.s_addr=htonl(INADDR_ANY);
serveraddr.sin_port=htons((unsigned short)port);
if(bind(listen_sock,(SA *)&serveraddr,sizeof(serveraddr))<0)
return -1;
if(listen(listen_sock,LISTENQ)<0)
return -1;
return listen_sock;
}
typedef struct sockaddr SA;
void process_trans(int fd);
void read_requesthdrs(rio_t *rp);
int is_static(char *uri);
void parse_static_uri(char *uri, char *filename);
void parse_dynamic_uri(char *uri, char *filename, char *cgiargs);
void feed_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void feed_dynamic(int fd, char *fileName, char *cgiargs);
void error_request(int fd, char *cause, char *errnum,char *shortmsg, char *description);
void process_trans(int fd)
{
    int static_flag;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;
    rio_readinitb(&rio, fd);
    rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) {
       error_request(fd, method, "501", "Not Implemented",
                "weblet does not implement this method");
       return;
    }
    read_requesthdrs(&rio);
    static_flag=is_static(uri);
    if(static_flag)
        parse_static_uri(uri, filename);
    else
        parse_dynamic_uri(uri, filename, cgiargs);

    if (stat(filename, &sbuf) < 0) {
	    error_request(fd, filename, "404", "Not found",
		    "weblet could not find this file");
	    return;
    }
    if (static_flag) { 
	    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
	       error_request(fd, filename, "403", "Forbidden",
			    "weblet is not permtted to read the file");
	        return;
	    }
	    feed_static(fd, filename, sbuf.st_size);
    }
    else { 
	    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
	        error_request(fd, filename, "403", "Forbidden",
			"weblet could not run the CGI program");
	        return;
	    }
	    feed_dynamic(fd, filename, cgiargs);
    }
}
int is_static(char *uri)
{
   if(!strstr(uri,"cgi-bin"))
	   return 1;
   else
	   return 0;
}
void error_request(int fd, char *cause, char *errnum,char *shortmsg, char *description)
{
char buf[MAXLINE],body[MAXBUF];
 sprintf(body, "<html><title>error request</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, description, cause);
    sprintf(body, "%s<hr><em> webserver Web server</em>\r\n", body);
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    rio_writen(fd, buf, strlen(buf));
    rio_writen(fd, body, strlen(body));
}
void read_requesthdrs(rio_t *rp) 
{
    char buf[MAXLINE];
    rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) {
	    printf("%s", buf);
	    rio_readlineb(rp, buf, MAXLINE);
    }
    return;
}
void parse_static_uri(char *uri,char *filename)
{
    char *ptr;
    strcpy(filename,".");
    strcat(filename,uri);
    if(uri[strlen(uri)-1]=='/')
    strcat(filename,"home.html");
}
void parse_dynamic_uri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;
    ptr = index(uri, '?');
    if (ptr) {
	strcpy(cgiargs, ptr+1);
	*ptr = '\0';
    }
    else
	strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
}
void feed_static(int fd,char *fileName,int filesize)
{
    int sfd;
    char *srcp,filetype[MAXLINE],buf[MAXBUF];
    //向客户端发送响应头部
    get_filetype(filename, filetype);   //获取文件的类型
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: webserver Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    rio_writen(fd, buf, strlen(buf));
    //向服务端发送响应的内容
    sfd = open(fileName, O_RDONLY, 0);
    srcp = mmap(0, filesize, PROT_READ, MAP_PRIVATE, sfd, 0);
    close(sfd);
    rio_writen(fd, srcp, filesize);
    munmap(srcp, filesize);
}
void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))             //判断.html是否在文件名里面
                strcpy(filetype, "text/html");
        else if (strstr(filename, ".jpg"))     //判断.jpg是否在文件名里面
                strcpy(filetype, "image/jpeg");
        else if (strstr(filename, ".mpeg"))    //判断.mpeg是否在文件名里面
            strcpy(filename, "video/mpeg");
        else
            strcpy(filetype, "text/html");
}
void feed_dynamic(int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE],*emptylist[]={NULL};
    int pfd[2];
    //返回http响应的第一部分
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: webserver Web Server\r\n");
    rio_writen(fd, buf, strlen(buf));
    pipe(pfd);
        if (fork()==0){
            close(pfd[1]);                   //关闭管道写端pfd[1]
            dup2(pfd[0],STDIN_FILENO);       //将读端pfd[0]重定向为子进程的标准输入
            dup2(fd, STDOUT_FILENO);         //把与套接字描述符fd重定向为子进程的标准输出
            execve(filename, emptylist, NULL);    //加载cgi程序
        }
        close(pfd[0]);                       //关闭读端
        write(pfd[1],cgiargs,strlen(cgiargs)+1);  //将cgiargs中保存的CGI参数写入管道
        wait(NULL);                              //等待dgi进程结束并回收
        close(pfd[1]);                           //关闭管道写端
}
int main(int argc, char **argv)
{
    int listen_sock, conn_sock, port, clientlen;
    struct sockaddr_in clientaddr;
    //判断是否给出响应的端口号
    if (argc!=2) {
       fprintf(stderr, "usage: %s <port>\n",argv[0]);
       exit(1);
       }
    port=atoi(argv[1]); //把端口号转换成整型
    listen_sock=open_listen_sock(port);           //打开一个监听套接字
        while(1){                                 //循环接受连接请求
                clientlen=sizeof(clientaddr);
                conn_sock=accept(listen_sock,(SA *)&clientaddr,&clientlen);
                process_trans(conn_sock);
                close(conn_sock);                    //关闭连接
        }

}
