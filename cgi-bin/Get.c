#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void)
{
char *buf,*p;
char content[4096];
int a=0;
int b=0;
scanf("a=%d&b=%d", &a, &b);
sprintf(content, "This is Post\r\n: ");
sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, a,b,a+b);
printf("Content-length: %d\r\n", strlen(content));
printf("Content-type: text/html\r\n\r\n");
printf("%s", content);
fflush(stdout);
exit(0);
}
