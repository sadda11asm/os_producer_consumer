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
#include <math.h>
#include "prodcon.h"




int connectsock( char *host, char *service, char *protocol );
char* 	PRODUCE = "PRODUCE\r\n";
char* 	CRLF = "\r\n";
char* 	GO = "GO\r\n";
char* 	DONE = "DONE\r\n";
double 	rate = 0;
int 	bad = 0;

char		*service;		
char		*host = "localhost";

char* getRandomString() {
    int size = random()%MAX_LETTERS;
	char* str = malloc(size);	
	int i;
	for ( i = 0; i < size-1; i++ )
		str[i] = 'X';
	str[i] = '\0';
    return str;
}

double poissonRandomInterarrivalDelay( double r ) {
    return (log((double) 1.0 - 
			((double) rand())/((double) RAND_MAX)))/-r;
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
	buf[4]='\0';
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
	char done[10];
	if ( read( csock, buf, 7) <= 0 ) {
		printf( "The server has gone.\n" );
		close(csock);
		exit(-1);
	}
	printf("Success!\n");
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
int main( int argc, char *argv[] )
{
	int PRODUCERS_COUNT=10;
	switch( argc ) 
	{
		case    5:
			service = argv[1];
			PRODUCERS_COUNT = atoi(argv[2]);
			rate = atof(argv[3]);
			bad = atoi(argv[4]);
			break;
		case	6:
			host = argv[1];
			service = argv[2];
			PRODUCERS_COUNT = atoi(argv[3]);
			rate = atof(argv[4]);
			bad = atoi(argv[5]);
		default:
			fprintf( stderr, "usage: chat [host] port num rate bad_num\n" );
			exit(-1);
	}

	// printf("%f\n", rate);
	// double wt = poissonRandomInterarrivalDelay(rate);
	// printf("%f\n", wt);
	// return 0;

	for (int i = 0; i < PRODUCERS_COUNT; i++)
	{
        pthread_t	thr;
		double waiting_time = poissonRandomInterarrivalDelay(rate);
		usleep(waiting_time*1000000);
		// printf(waiting_time);
		pthread_create( &thr, NULL, produce, NULL);

	}
	pthread_exit(0);

}


