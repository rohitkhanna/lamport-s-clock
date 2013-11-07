/*UDP Server Program using Berkley Sockets*/

/* Compile Command: cc -o udps udps.c -lnsl -lsocket */
/* Run Command: ./udps */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#define SERVER_PORT 23456 
#define MAX_LINE 256

int main (int argc, char * argv[])
{
   struct hostent *hp;
   struct sockaddr_in server, client;
   char buf[MAX_LINE];
   int s, ret;
   int length=0;
	
   memset(buf,'\0', sizeof(buf));
   hp = gethostbyname("192.168.1.118");  // replace this ip with your host name or ip
   if (!hp)			
   {
		fprintf(stderr,"simplex-talk:Unknown host: %s\n",(char*)hp);
		exit(1);
   }

   bzero((char *)&server, sizeof(server));
   server.sin_family = AF_INET;
   bcopy(hp->h_addr, (char *)&server.sin_addr,hp->h_length);
   server.sin_port = htons(SERVER_PORT);

   if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
   {
		perror("simplex_talk: socket error");
		exit(1);
   }

   ret = bind(s, (struct sockaddr *)&server, sizeof(server));
   if( ret < 0)
   {
		fprintf( stderr, "Bind Error: can't bind local address");
		exit(1);
   }

   length = sizeof(client);
   while(1)
   {
	      ret = recvfrom(s, buf, MAX_LINE, 0, (struct sockaddr *)&client, &length);
	      printf("ret=%d, buf=|%s|, strlen(buf)=%d\n", ret, buf, strlen(buf));	
		if( ret < 0 )
		{
			printf("Send Error %d\n", ret );
			exit(1);
		}
		buf[ret] = '\0';
		printf( "Working!!!> %s", buf);
   }
   return 0;
}
