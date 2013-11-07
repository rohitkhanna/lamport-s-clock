/*
	project3app.c - User Application for Project 3
	By - Rohit Khanna
*/

#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <string.h>
#include <curses.h>
#include <pthread.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>


#define BUFFER_SIZE 236
#define IP_ADDR_SIZE 16

#define SERVER_PORT 23456 


char wr_buf[252];		//236 data + 16 ip
char rd_buf[236], wr_data[236] ;
char ip_addr[20];
FILE *fd;

const char SERVER_IP[] = "192.168.1.118";


int sendToUDPServer(char *buf){

	   struct sockaddr_in client, server;
	   struct hostent *hp;
	   int len, ret, n;
	   int s, new_s;
	   char app_buf[236];
	   
	   sprintf(app_buf, "%s", buf);
	  
	   
	   bzero((char *)&server, sizeof(server));
	   server.sin_family = AF_INET;
	   server.sin_addr.s_addr = INADDR_ANY;
	   server.sin_port = htons(0);

	   s = socket(AF_INET, SOCK_DGRAM, 0);				//create UDP socket
	   if (s < 0)
	   {
			perror("simplex-talk: UDP_socket error");
			exit(1);
	   }

	   if ((bind(s, (struct sockaddr *)&server, sizeof(server))) < 0)
	   {
			perror("simplex-talk: UDP_bind error");
			exit(1);
	   }

	   hp = gethostbyname(SERVER_IP);		 // server ip address
	   if( !hp )
	   {
	      	fprintf(stderr, "Unknown host %s\n", "localhost");
	      	exit(1);
	   }

	   bzero( (char *)&server, sizeof(server));
	   server.sin_family = AF_INET;
	   bcopy( hp->h_addr, (char *)&server.sin_addr, hp->h_length );
	   server.sin_port = htons(SERVER_PORT); 

	       n = strlen(app_buf);
	       printf("sending app_buf=%s, to UDP server\n", app_buf);
	       ret = sendto(s, app_buf, n, 0,(struct sockaddr *)&server, sizeof(server));
		if( ret != n)
		{
			fprintf( stderr, "Datagram Send error %d\n", ret );
			exit(1);
		}
		
	   return ret;
	   
	   
}



//create a thread with function func and aregument arg
pthread_t start_thread(void *func, int *arg){
   pthread_t thread_id;
   int rc;
      printf("In main: creating thread\n");
      rc = pthread_create(&thread_id, NULL, func, arg);
      if (rc){
         printf("ERROR; return code from pthread_create() is %d\n", rc);
         exit(-1);
        }
    return(thread_id);
 }



void clear_newlines(void)
{
    int c;
    do
    {
        c = getchar();
    } while (c != '\n' && c != EOF);
}	



void append_IP_address(char *ip_addr, char *wr_data){
	
	memset(wr_buf, '\0', sizeof(wr_buf));				//clear string
	strncpy(wr_buf, ip_addr, strlen(ip_addr));			//copy ip_addr
	
	wr_buf[strlen(ip_addr)] = '_';				//add delimiter '_'
	strncat(wr_buf+strlen(ip_addr), wr_data, strlen(wr_data) );	//copy data
	//printf("wr_buf=%s\n",wr_buf);
}




void READ(void)
{
    while (1){ 
    	int len;
    	len = Recv(fd, rd_buf);
    	if( (len>0 ) && (strcmp(rd_buf,"NO DATA TO READ !!")) ){
		
		printf("\n\nMESG RECEIVED ==> %s\n\n",rd_buf);
		char sub[5];
		strncpy(sub, rd_buf, 3);
		sub[3]='\0';
		//printf("sub=%s\n",sub);
		if( (len>0 ) && (strcmp(rd_buf,"NO DATA TO READ !!")!=0) && (strcmp(sub,"ACK")!=0) ){
			//printf("sending to UDP Server, rd_buf=%s\n", rd_buf);
			len = sendToUDPServer(rd_buf);
			printf("sent to UDP Server, len=%d\n", len);
		}
	}
    	
        sleep(1);
    }
}  



void WRITE(void){

	int rc;
	while(1){
		printf("\n\nENTER DATA TO BE SENT ==> ");
		if ( fgets(wr_data, sizeof(wr_data), stdin) != NULL ) {
			if (wr_data[strlen(wr_data)-1] == '\n')			//remove new line character '\n'
				wr_data[strlen(wr_data)-1] = '\0';
		
			append_IP_address(ip_addr, wr_data);
			//printf("...data to be sent = |%s|\n", wr_buf);
			rc = Send(wr_buf, fd);
			printf("...DATA sent with rc = %d...\n", rc);
		}
	}
	
}



int main(int argc, char **argv)
{
	printf("\t\tUser Application for Project 3");

	fd = NULL;
	char buffer[128];
	size_t count;
	int rc, len, ch;
	char c;

	fd = open("/dev/cse5361", O_RDWR);
	if (!fd)
	{	printf("File error opening file\n");
		exit(1);
	}
	
	ch=0;
	while(ch!=4){
	start:
		printf("\n-----------------------------------------------------------------\n");
		printf("1. Enter IP Address \t2. WRITE Data   \t3. READ Data  \t4. READ/WRITE  \t5. EXIT\n");
		printf("-----------------------------------------------------------------\n");
		printf("Enter choice --> ");
		
		scanf("%d",&ch);
		clear_newlines();
		
		switch(ch){
			case 1:
				printf("Enter the IP Address of the node where you want to read/write from == > ");
				if( fgets(ip_addr, sizeof(ip_addr), stdin) != NULL)  {
					if (ip_addr[strlen(ip_addr)-1] == '\n')		//remove new line character '\n'
		       				ip_addr[strlen(ip_addr)-1] = '\0';
		       			
					printf("ip_addr = %s, strlen(ip_addr) = %d \n", ip_addr, strlen(ip_addr));
				}
				break;
		
			case 2:
				printf("\nENTER DATA TO BE SENT ==> ");
				if ( fgets(wr_data, sizeof(wr_data), stdin) != NULL ) {
					if (wr_data[strlen(wr_data)-1] == '\n')			//remove new line character '\n'
		       				wr_data[strlen(wr_data)-1] = '\0';
		       				
					append_IP_address(ip_addr, wr_data);
					printf("...data to be sent = |%s|\n", wr_buf);
					rc = Send(wr_buf, fd);
					printf("...DATA sent with rc = %d...\n", rc);
				}
				break;

			case 3:	
				len = Recv(fd, rd_buf);
				//printf("...DATA received with len = %d...\n", len);
				printf("\n\nMESG RECEIVED ==> %s\n\n",rd_buf);
				char sub[5];
				strncpy(sub, rd_buf, 3);
				sub[3]='\0';
				//printf("sub=%s\n",sub);
				if( (len>0 ) && (strcmp(rd_buf,"NO DATA TO READ !!")!=0) && (strcmp(sub,"ACK")!=0) ){
					//printf("sending to UDP Server, rd_buf=%s\n", rd_buf);
					len = sendToUDPServer(rd_buf);
					printf("sent to UDP Server, len=%d\n", len);
				}
				
				break;


			case 4:
				start_thread(READ, NULL);
    				start_thread(WRITE, NULL);
    				
    				while(1) sleep(1);		//otherwise the main thread terminates 
				
				
			case 5:	printf("Closing App\n");
				fclose(fd);
				printf("File Descriptor closed\n");
				break;
			
			default:
				printf("Wrong choice ch=%d entered, enter again -->\n",ch);
				goto start;
		}
	}
	return 0;
}



int Send( char *wr_buf, FILE *fd)
{
	int rc;
	
	printf("...Writing |%s| with strlen |%d| to device file.. \n", wr_buf, strlen(wr_buf));
	rc = write(fd, wr_buf, sizeof(wr_buf) );
	
	if ( rc == -1 ) {
		perror("write failed");
		close(fd);
		exit(-1);
	}
	return rc;
}



int Recv(FILE *fd, char *rd_buf ){

	int len;
	//printf("\n...READING NOW...\n");

	len = read(fd, rd_buf, sizeof(rd_buf));	
	if ( len < 0 ) {
		perror("read failed");
		close(fd);
		exit(-1);
	}
	//printf("len = %d and strlen(rd_buf)=%d\n", len, strlen(rd_buf));
	
	rd_buf[len]='\0';
	//printf("MESG RECEIVED ==> %s\n",rd_buf);

	return len;
}

