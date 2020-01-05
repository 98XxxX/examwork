#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <funct1.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
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
ssze_t nread;
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
