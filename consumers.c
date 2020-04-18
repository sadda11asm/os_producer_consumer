#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <fcntl.h> 
#include <pthread.h>
#include <math.h>
#include "prodcon.h"


int connectsock( char *host, char *service, char *protocol );
char* CONSUME = "CONSUME\r\n";
char* CRLF = "\r\n";
char* GO = "GO\r\n";
char* DONE = "DONE\r\n";

char		*service;		
char		*host = "localhost";
double      rate = 0;
int         bad = 0;

double poissonRandomInterarrivalDelay( double r )
{
    return (log((double) 1.0 - 
			((double) rand())/((double) RAND_MAX)))/-r;
}

void *consume(void *tid) {
    int id = (int) tid;
	int	csock;
	/*	Create the socket to the controller  */
	if ( ( csock = connectsock( host, service, "tcp" )) == 0 )
	{
		fprintf( stderr, "Cannot connect to server.\n" );
		exit( -1 );
	}

	printf( "The server is ready for consumer!\n" );
	fflush( stdout );

    if ( write(csock, CONSUME , 10) < 0 ) {
        printf( "The server has gone when getting CONSUME\n" );
        fprintf( stderr, "Consumer write: %s\n", strerror(errno) );
        exit( -1 );
	}

    int size = 10;
    if ( read( csock, &size, sizeof(size)) <= 0 )
    {
        printf( "The server has gone when should pass size of item\n" );
        close(csock);
        exit(-1);
    } 
    size = ntohl(size);
    printf("SIZE %d\n", size);
    fflush( stdout );
    char *buf = malloc((size + 1)*sizeof(char));

    int load = 1;
    int cursor = 0;
    while (load!=0) {
	if (cursor >= size) break;
	load = read(csock, (void *) (buf + cursor), size - cursor);
	cursor+=load;
    }

    /*if ( read( csock, buf, size) <= 0 )
    {
        printf( "The server has gone when should pass buffer of item.\n" );
        close(csock);
        exit(-1);
    }*/

    //printf("Consuming: %s", buf);
    //fflush( stdout );
    char* name = malloc(10*sizeof(char));
    sprintf(name, "%d.txt", id);
    //printf("NAME: %s\n", name);
    //fflush( stdout );
    int fd = open(name, O_RDWR | O_CREAT, 0777);
	if (fd == -1) {
		printf("Error Number % d\n", errno);  
        perror(buf);
		exit(-1);		
	}
    write(fd, buf, strlen(buf));
    close(fd);
    free(name);
	pthread_exit( NULL );
}

/*
**	Client
*/
int
main( int argc, char *argv[] )
{
	int CONSUMERS_COUNT = 10;
	switch( argc ) 
	{

		case    5:
			service = argv[1];
			CONSUMERS_COUNT = atoi(argv[2]);
            rate = atof(argv[3]);
            bad = atoi(argv[4]);
			break;
		case	6:
			host = argv[1];
			service = argv[2];
			CONSUMERS_COUNT = atoi(argv[3]);
            rate = atof(argv[4]);
            bad = atoi(argv[5]);
		default:
			fprintf( stderr, "usage: chat [host] port rate bad_num\n" );
			exit(-1);
	}


	for (int i = 0; i < CONSUMERS_COUNT; i++)
	{
        pthread_t	thr;
        double waiting_time = poissonRandomInterarrivalDelay(rate);
        usleep(waiting_time*1000000);
		pthread_create( &thr, NULL, consume, (void *) i);

	}
	pthread_exit(0);

}


