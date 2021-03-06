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

char* getRandomString(int size) {
    char* str = malloc(size);	
	int i;
	for ( i = 0; i < size; i++ )
		str[i] = 'X';
    return str;
}

double poissonRandomInterarrivalDelay( double r ) {
    return (log((double) 1.0 - 
			((double) rand())/((double) RAND_MAX)))/-r;
}

void slow_down() {
	sleep(SLOW_CLIENT);
}

int min(int a, int b) {
	if (a < b) return a;
	return b;
}

void *produce(void *is_b) {

	int is_bad = (int) is_b;


	int		csock;
	/*	Create the socket to the controller  */
	if ( ( csock = connectsock( host, service, "tcp" )) == 0 )
	{
		fprintf( stderr, "Cannot connect to server.\n" );
		pthread_exit(NULL);
	}

	if (is_bad == 1) {
		slow_down();
	}

	printf( "The server is ready for producer!\n" );
	fflush( stdout );

    if ( write(csock, PRODUCE , 9) < 0 ) {
        fprintf( stderr, "Producer write: %s\n", strerror(errno) );
		close(csock);
        pthread_exit(NULL);
	}
    char buf[5];
    if ( read( csock, buf, 4) <= 0 ) {
        printf( "The server has gone.\n" );
        close(csock);
      	pthread_exit(NULL);
    }
	buf[4]='\0';
    if (strcmp(buf, GO) == 0) {
		int size = random()%MAX_LETTERS;
		// printf("MAX_LETTERS %d\n", MAX_LETTERS);
        int len = htonl(size);
        char *data = (char*)&len;
        if ( write(csock, data , sizeof(len)) < 0 ) {
            fprintf( stderr, "server write: %s\n", strerror(errno) );
            close(csock);
      		pthread_exit(NULL);
	    }

		if ( read( csock, buf, 4) <= 0 ) {
			printf( "The server has gone.\n" );
			close(csock);
      		pthread_exit(NULL);
		}
		buf[4]='\0';
		
		if (strcmp(buf, GO) != 0) {
			printf("Unexpected action: %s", buf);
       		close(csock);
			pthread_exit( NULL );;
		}

		int cursor = 0;
		int window = BUFSIZE;
		while (cursor < size) {
			char* item = getRandomString(min(size-cursor, window));
			int load;
			if ( (load = write(csock, item , strlen(item))) < 0 ) {
				fprintf( stderr, "server write: %s\n", strerror(errno) );
				exit( -1 );
			}
			cursor+=load;
			free(item);
			if (load == 0) {
				break;
			}	
		}
		// printf("OUT OF WHILE\n");
		char done[7];
		if ( read( csock, done, 6) <= 0 ) {
			printf( "The server has gone.\n" );
			close(csock);
      		pthread_exit(NULL);
		}
		done[6] = '\0';
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

	int number = (int) (bad*1.0*PRODUCERS_COUNT/100.0);
	int dif = PRODUCERS_COUNT/(number+1); 
	if (dif == 0) {
		dif = 1;
	}
	int count = 0;

	for (int i = 0; i < PRODUCERS_COUNT; i++) {
        pthread_t	thr;
		double waiting_time = poissonRandomInterarrivalDelay(rate);
		usleep(waiting_time*1000000);
		// printf(waiting_time);
		int is_bad = 0;
		
		if (i!=0 && i%dif == 0) is_bad = 1;
		if (i == 0 && number == PRODUCERS_COUNT) is_bad = 1;
		if (count == number) is_bad = 0;

		if (is_bad == 1) {
			count++;
		}

		pthread_create( &thr, NULL, produce, (void *) is_bad);


	}
	// sleep(10);
	// printf("CHECK BAD %d\n", count);
	pthread_exit(0);

}


