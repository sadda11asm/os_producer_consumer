#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "prodcon.h"
#include <sys/select.h>

int CON_COUNT 	= 	0;
int	PROD_COUNT	=	0;
int	CONS_COUNT	=	0;
char* PRODUCE = "PRODUCE\r\n";
char* CRLF = "\r\n";
char* GO = "GO\r\n";
char* CONSUME = "CONSUME\r\n";
char* DONE = "DONE\r\n";
int ITEMSIZE = BUFSIZE;
int nfds = 0;
fd_set afds;

int passivesock( char *service, char *protocol, int qlen, int *rport );

ITEM *makeItem(int size, int ssock){
	int i;
	ITEM *p = malloc( sizeof(ITEM) );
	p->size = size;
	p->psd = ssock;	
	return p;
}

int count;
ITEM **buffer;

pthread_mutex_t mutex;
pthread_mutex_t mutex_conns;
sem_t full, empty;

int min(int a, int b) {
	if (a < b) return a;
	return b;
}

void close_socket(int ssock, int con_type) {
	close(ssock);	
	pthread_mutex_lock( &mutex_conns );	
	if (con_type == 1) {
		PROD_COUNT--;
	} else if (con_type == 0) {
		CONS_COUNT--;
	}
	CON_COUNT--;
	pthread_mutex_unlock( &mutex_conns );
}

void *produce(void *ssck) {

	int ssock = (int) ssck;

    int size = 10;

	// Wait for room in the buffer
	// while ( count > BUFSIZE );
    
    if ( write( ssock, GO, 4 ) < 0 ) {
            /* This guy is dead */
			printf( "The producer has gone when should get GO.\n" );
            close_socket( ssock, 1 );
			pthread_exit( 0 );
            return;
    } 

    if ( read( ssock, &size, sizeof(size)) <= 0 )
    {
        printf( "The producer has gone when should pass size of item.\n" );
        close_socket(ssock, 1);
		pthread_exit( 0 );
        return;
    } 
	size = ntohl(size);
    
	

	ITEM *p = makeItem(size, ssock);
    //printf("Producing buf %s\n", p->letters);
    //fflush(stdout);
    printf("Producing size %d\n", p->size);
    fflush(stdout);
	sem_wait( &empty );
	pthread_mutex_lock( &mutex );
	// Put the item in the next slot in the buffer
	buffer[count] = p;
	count++;
	printf( "C Count %d.\n", count );
	pthread_mutex_unlock( &mutex );

	sem_post( &full );

	pthread_exit(0);
}

void *consume(void *ssck) {
	
	int ssock = (int) ssck;

	// Wait for items in the buffer
	// while ( count <= 0 );
	ITEM p;
	sem_wait( &full );
	pthread_mutex_lock( &mutex );
	// Remove the item and update the buffer
	p = *(buffer[count-1]);
	buffer[count-1] = NULL;
	count--;
	printf( "C Count %d.\n", count );
	pthread_mutex_unlock( &mutex );

	sem_post(&empty );

	
	int size = p.size;
	int psock = p.psd;

	int len = htonl(p.size);
	char *data = (char*)&len;

	//sending size to consumer
	if ( write(ssock, data , sizeof(len)) < 0 ) {
		fprintf( stderr, "client write: %s\n", strerror(errno) );
        close_socket( psock, 1 );
        close_socket( ssock, 0);
        pthread_exit(0);
		exit( -1 );
	}
	//reading from psock

    int load = 1;
    int cursor = 0;

	if ( write( psock, GO, 4 ) < 0 ) {
            /* This guy is dead */
			printf( "The producer has gone when should get GO.\n" );
            close_socket( psock, 1 );
            close_socket( ssock, 0);
            pthread_exit(0);
            return;
    }

    while (load!=0) {
		char* buf = malloc(BUFSIZE*sizeof(char));
		if (cursor >= size - 1) break;
		load = read(psock, (void *) buf, min(BUFSIZE, size - cursor));
		write( ssock, buf, load);
		cursor+=load;
		// printf("Load %d\n", load);
    }
	// printf("OUT OF WHILE\n");
	// buf[size] = '\0';
	

	//printf("Consuming buf %s\n", p.letters);
	//fflush(stdout);
	printf("Consuming size %d\n", p.size);
	fflush(stdout);
	// if ( write( ssock, buf, p.size) < 0 ) {
	// 	/* This guy is dead */
	// 	close_socket( ssock, 0 );
	// 	exit(-1);
	// } 
	
    if ( write( psock, DONE, 6 ) < 0 ) {
            // This guy is dead
			printf( "The producer has gone when should get DONE.\n" );
            close_socket( psock, 1 );
            close_socket(ssock, 0);
            pthread_exit(0);
            exit(-1);
    } 
	
	close_socket(psock, 1);
	close_socket(ssock, 0);
    pthread_exit(0);
	// Exit
}


void handle( int ssock, pthread_t	thr ) {
	char buf[10];
	int cc;

	/* start working for this guy */
	/* ECHO what the client says */

    if ( (cc = read( ssock, buf, 10)) <= 0 )
    {
        printf( "The client has gone.\n" );
		close_socket(ssock, 10);
        exit(-1);
    } 

    if (strcmp(buf, PRODUCE) == 0) {
	
		int ok = 1;
	
    	pthread_mutex_lock( &mutex_conns );
	
    	if (PROD_COUNT < MAX_PROD) {
			PROD_COUNT++;
		} else {
			ok = 0;
		}
		pthread_mutex_unlock( &mutex_conns );
		if (ok) {
			printf("Producer is here!\n");
			pthread_create( &thr, NULL, produce, (void *) ssock );
		} else {
			close_socket(ssock, 10);
			printf("TOO MANY PRODUCERS! LIMIT IS REACHED!\n");
		}

    } else if (strcmp(buf, CONSUME) == 0) {

		pthread_mutex_lock( &mutex_conns );
		int ok = 1;
		if (CONS_COUNT < MAX_CON) {
			CONS_COUNT++;
		} else {
			ok = 0;
		}
		
		pthread_mutex_unlock( &mutex_conns );
		if (ok) {
			printf("Consumer is here!\n");
			pthread_create( &thr, NULL, consume, (void *) ssock );
		} else {
			close_socket(ssock, 10);
			printf("TOO MANY CONSUMERS! LIMIT IS REACHED!\n");
		}        
	
    } else {
        printf("Unexpected action: %s\n", buf);
		close_socket(ssock, 10);	
        exit(-1);
    }
}


/*
*/
int main( int argc, char *argv[] ) {
	char			*service;
	struct sockaddr_in	fsin;
	int			alen;
	int			msock;
	int			ssock;
	int			rport = 0;
    pthread_mutex_init( &mutex, NULL );
    pthread_mutex_init( &mutex_conns, NULL);
	// pthread_mutex_init( &mutex_select, NULL);


    count = 0;

	
	switch (argc) 
	{
		case	1:
			// No args? let the OS choose a port and tell the user
			rport = 1;
			break;
		case	2:
			ITEMSIZE = atoi(argv[1]);
			break;
		case	3:
			// User provides a port? then use it
			service = argv[1];
			ITEMSIZE = atoi(argv[2]);
			break;
		default:
			fprintf( stderr, "usage: server [port]\n" );
			exit(-1);
	}

	// printf("MAX_LETTERS %d\n", MAX_LETTERS);
	sem_init( &full, 0, 0 );
	sem_init( &empty, 0, ITEMSIZE );

	buffer = malloc(ITEMSIZE * sizeof(ITEM*));

	msock = passivesock( service, "tcp", QLEN, &rport );
	if (rport)
	{
		//	Tell the user the selected port
		printf( "server: port %d\n", rport );	
		fflush( stdout );
	}

	nfds = msock+1;
	FD_ZERO(&afds);
	FD_SET( msock, &afds );

	
	for (;;)
	{
		int	ssock;
		pthread_t	thr;

		fd_set rfds;
		memcpy((char *)&rfds, (char *)&afds, sizeof(rfds));

		// pthread_mutex_lock( &mutex_select );

		if (select(nfds, &rfds, (fd_set *)0, (fd_set *)0,
				(struct timeval *)0) < 0)
		{
			fprintf( stderr, "server select: %s\n", strerror(errno) );
			exit(-1);
		}
		// pthread_mutex_unlock( &mutex_select );

		if (FD_ISSET( msock, &rfds)) {
			
			int	ssock;

			alen = sizeof(fsin);
			ssock = accept( msock, (struct sockaddr *)&fsin, &alen );
			if (ssock < 0)
			{
				fprintf( stderr, "accept: %s\n", strerror(errno) );
				exit(-1);
			}

			// If a new client arrives, we must add it to our afds set
			FD_SET( ssock, &afds );

			// and increase the maximum, if necessary
			if ( ssock+1 > nfds )
				nfds = ssock+1;
		} 


		for (int fd = 0; fd < nfds; fd++ )
		{
			// check every socket to see if it's in the ready set
			// But don't recheck the main socket
			if (fd != msock && FD_ISSET(fd, &rfds))
			{

				int ok = 0;
				FD_CLR( fd, &afds );
				if ( nfds == fd+1 )
					nfds--;
				
				pthread_mutex_lock( &mutex_conns );
				if (CON_COUNT < MAX_CLIENTS) {
				
					printf( "A client has arrived.\n" );
					fflush( stdout );
					CON_COUNT++;
					ok = 1;
					// pthread_create( &thr, NULL, handle, (void *) ssock );
				
					// you can read without blocking because data is there
					// the OS has confirmed this					
				} 
				pthread_mutex_unlock( &mutex_conns );

				if (ok == 1) {
					handle(fd, thr);
				} else {
					(void) close(fd);		
					printf("TOO MANY CONNECTIONS! LIMIT IS REACHED!");		
					fflush( stdout );	
				}
			}

		}
		
	}
	pthread_exit(0);
}