#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void)
{
char content[4096];
int a=0,b=0;
scanf("%d&%d",&a,&b);
sprintf(content,"You can calculate multiplication here\r\n<p>");
sprintf(content,"%sThe answer is :%d*%d=%d\r\n<p>",content,a,b,a*b);
 printf("Content-length: %d\r\n", strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    printf("%s", content);
    fflush(stdout);
    exit(0);
}
