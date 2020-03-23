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
sem_t full, empty;

void produce(int ssock) {

    int size = 10;

	// Wait for room in the buffer
	// while ( count > BUFSIZE );
    
    if ( write( ssock, GO, 5 ) < 0 ) {
            /* This guy is dead */
			printf( "The producer has gone when should get GO.\n" );
            close( ssock );
            return;
    } 

    if ( read( ssock, &size, sizeof(size)) <= 0 )
    {
        printf( "The producer has gone when should pass size of item.\n" );
        close(ssock);
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
printf("Producing buf %s\n", p->product);
    fflush(stdout);
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
            close( ssock );
            exit(-1);
    } 
	// Exit
	close(ssock);
	pthread_exit( NULL );
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
	printf("Consuming buf %s\n", p.product);
	fflush(stdout);
	printf("Consuming size %d\n", p.size);
	fflush(stdout);
	if ( write( ssock, p.product, p.size) < 0 ) {
		/* This guy is dead */
		close( ssock );
		exit(-1);
	} 
	close(ssock);
	// Exit
	pthread_exit( NULL );
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
        close(ssock);
        exit(-1);
    } 

    if (strcmp(buf, PRODUCE) == 0) {
        printf("Producer is here!\n");
        produce(ssock);
    } else if (strcmp(buf, CONSUME) == 0) {
        consume(ssock);
        printf("Consumer is here!\n");
    } else {
        printf("Unexpected action: %s\n", buf);
		close(ssock);
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
	sem_init( &full, 0, 0 );
	sem_init( &empty, 0, ITEMSIZE );

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

		printf( "A client has arrived.\n" );
		fflush( stdout );

		pthread_create( &thr, NULL, handle, (void *) ssock );

	}
	pthread_exit(0);
}

