#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcnt1.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#define RIO_BUFSIZE 8192
#define MAXLINE 8192
#define MAXBUF 8192
#define LISTENQ 1024
extern int h_errno;
extern char **environ;
typedef struct{
int rio_fd;
int rio_cnt;
char *rio_bufptr;
char rio_buf[RIO_BUFSIZE];
}rio_t;
ssize_t rio_readn(int fd,void *usrbuf,size_t n){
size_t nleft=n;
ssize_t nread;
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
void feed_dynamic(int fd, char *filename, char *cgiargs);
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
