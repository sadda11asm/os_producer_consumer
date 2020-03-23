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


#define PRODUCERS_COUNT 10

int connectsock( char *host, char *service, char *protocol );
char* PRODUCE = "PRODUCE\r\n";
char* CRLF = "\r\n";
char* GO = "GO\r\n";
char* DONE = "DONE\r\n";

char		*service;		
char		*host = "localhost";

char* getRandomString() {
    int size = random()%80;
	char* str = malloc(size);	
	int i;
	for ( i = 0; i < size-1; i++ )
		str[i] = 'X';
	str[i] = '\0';
    return str;
}

void *produce() {
	int		csock;
	/*	Create the socket to the controller  */
	if ( ( csock = connectsock( host, service, "tcp" )) == 0 )
	{
		fprintf( stderr, "Cannot connect to server.\n" );
		exit( -1 );
	}

	printf( "The server is ready for producer!\n" );
	fflush( stdout );

    if ( write(csock, PRODUCE , 10) < 0 ) {
        fprintf( stderr, "Producer write: %s\n", strerror(errno) );
        exit( -1 );
	}
    char buf[5];
    if ( read( csock, buf, 5) <= 0 ) {
        printf( "The server has gone.\n" );
        close(csock);
        exit(-1);
    }
    if (strcmp(buf, GO) == 0) {
        char* item = getRandomString();
        int len = htonl(strlen(item));
        char *data = (char*)&len;
        if ( write(csock, data , sizeof(len)) < 0 ) {
            fprintf( stderr, "server write: %s\n", strerror(errno) );
            exit( -1 );
	    }

        if ( write(csock, item , strlen(item)) < 0 ) {
            fprintf( stderr, "server write: %s\n", strerror(errno) );
            exit( -1 );
	    }
        close( csock );
    } else {
        printf("Unexpected action: %s", buf);
        close(csock);
    }

	pthread_exit( NULL );;
}

/*
**	Client
*/
int
main( int argc, char *argv[] )
{
	
	switch( argc ) 
	{
		case    2:
			service = argv[1];
			break;
		case    3:
			host = argv[1];
			service = argv[2];
			break;
		default:
			fprintf( stderr, "usage: chat [host] port\n" );
			exit(-1);
	}


	for (int i = 0; i < PRODUCERS_COUNT; i++)
	{
        pthread_t	thr;
		pthread_create( &thr, NULL, produce, NULL);

	}
	pthread_exit(0);

}


