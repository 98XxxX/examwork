#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void)
{
char content[4096];
int m=0;
int n=0;
scanf("m=%d&n=%d", &m, &n);
sprintf(content, "This is Post\r\n: ");
sprintf(content, "%sThe answer is: %d *%d = %d\r\n<p>", content, m,n,m*n);
printf("Content-length: %d\r\n", strlen(content));
printf("Content-type: text/html\r\n\r\n");
printf("%s", content);
fflush(stdout);
exit(0);
}