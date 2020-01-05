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
