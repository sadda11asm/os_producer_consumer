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


#define	QLEN			5
int	ITEMSIZE	=	100;
int 	CON_COUNT 	= 	0;
int	PROD_COUNT	=	0;
int	CONS_COUNT	=	0;
int 	CON_MAX 	= 	512;
int	CON_PROD_MAX	=	480;
int 	CON_CONS_MAX	=	480;
char* PRODUCE = "PRODUCE\r\n";
char* CRLF = "\r\n";
char* GO = "GO\r\n";
char* CONSUME = "CONSUME\r\n";
char* DONE = "DONE\r\n";

int passivesock( char *service, char *protocol, int qlen, int *rport );

typedef struct item_t{
	char *product;
	int size;
} ITEM;

ITEM *makeItem(int size, char* buf){
	int i;
	ITEM *p = malloc( sizeof(ITEM) );
	p->size = size;
	p->product = malloc(p->size * sizeof(char));	
	for ( i = 0; i < p->size-1; i++ )
		p->product[i] = buf[i];
	p->product[i] = '\0';
	return p;
}

int count;
ITEM **buffer;

pthread_mutex_t mutex;
pthread_mutex_t mutex_conns;
sem_t full, empty;

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
	pthread_exit( NULL );
}

void produce(int ssock) {

    int size = 10;

	// Wait for room in the buffer
	// while ( count > BUFSIZE );
    
    if ( write( ssock, GO, 5 ) < 0 ) {
            /* This guy is dead */
			printf( "The producer has gone when should get GO.\n" );
            close_socket( ssock, 1 );
            return;
    } 

    if ( read( ssock, &size, sizeof(size)) <= 0 )
    {
        printf( "The producer has gone when should pass size of item.\n" );
        close_socket(ssock, 1);
        return;
    } 
	size = ntohl(size);
    char* buf = malloc((size + 1)*sizeof(char));

    int load = 1;
    int cursor = 0;
    while (load!=0) {
	if (cursor >= size) break;
	load = read(ssock, (void *) (buf + cursor), size - cursor);
	cursor+=load;
    }

    /*if ( read( ssock, buf, size) <= 0 )
    {
        printf( "The producer has gone when should pass buffer of item.\n" );
        close(ssock);
        return;
    }*/

   

    ITEM *p = makeItem(size, buf);
    //printf("Producing buf %s\n", p->product);
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

    if ( write( ssock, DONE, 7 ) < 0 ) {
            /* This guy is dead */
			printf( "The producer has gone when should get DONE.\n" );
            close_socket( ssock, 1 );
            exit(-1);
    } 
	// Exit
	close_socket(ssock, 1);
}

void consume(int ssock) {

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

	// Now use it
	int len = htonl(p.size);
	char *data = (char*)&len;
	
	if ( write(ssock, data , sizeof(len)) < 0 ) {
		fprintf( stderr, "client write: %s\n", strerror(errno) );
		exit( -1 );
	}
	//printf("Consuming buf %s\n", p.product);
	//fflush(stdout);
	printf("Consuming size %d\n", p.size);
	fflush(stdout);
	if ( write( ssock, p.product, p.size) < 0 ) {
		/* This guy is dead */
		close_socket( ssock, 0 );
		exit(-1);
	} 
	close_socket(ssock, 0);
	// Exit
}


void *handle( void *s ) {
	char buf[10];
	int cc;
	int ssock = (int) s;

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
	
    	if (PROD_COUNT < CON_PROD_MAX) {
		PROD_COUNT++;
	} else {
		ok = 0;
		close_socket(ssock, 10);
		printf("TOO MANY PRODUCERS! LIMIT IS REACHED!\n");
	}
	pthread_mutex_unlock( &mutex_conns );
	if (ok) {
		printf("Producer is here!\n");
        	produce(ssock);
	}	

    } else if (strcmp(buf, CONSUME) == 0) {

	pthread_mutex_lock( &mutex_conns );
	int ok = 1;
	if (CONS_COUNT < CON_CONS_MAX) {
		CONS_COUNT++;
	} else {
		ok = 0;
		close_socket(ssock, 10);
		printf("TOO MANY CONSUMERS! LIMIT IS REACHED!\n");
	}
	
	pthread_mutex_unlock( &mutex_conns );
	if (ok) {
		consume(ssock);
        	printf("Consumer is here!\n");	
	}        
	
    } else {
        printf("Unexpected action: %s\n", buf);
	close_socket(ssock, 10);
    }
	pthread_exit(0);
}


/*
*/
int main( int argc, char *argv[] )
{
	char			*service;
	struct sockaddr_in	fsin;
	int			alen;
	int			msock;
	int			ssock;
	int			rport = 0;
    pthread_mutex_init( &mutex, NULL );
    pthread_mutex_init( &mutex_conns, NULL);

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

	
	for (;;)
	{
		int	ssock;
		pthread_t	thr;

		alen = sizeof(fsin);
		ssock = accept( msock, (struct sockaddr *)&fsin, &alen );
		if (ssock < 0)
		{
			fprintf( stderr, "accept: %s\n", strerror(errno) );
			break;
		}
		
		pthread_mutex_lock( &mutex_conns );
		
		if (CON_COUNT < CON_MAX) {
		
			printf( "A client has arrived.\n" );
			fflush( stdout );
			CON_COUNT++;
			pthread_create( &thr, NULL, handle, (void *) ssock );
		} else {
			close(ssock);
			printf("TOO MUCH CONNECTIONS! LIMIT IS REACHED!");			
		}
		pthread_mutex_unlock( &mutex_conns );
	}
	pthread_exit(0);
}

