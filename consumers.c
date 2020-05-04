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

int min(int a, int b) {
	if (a < b) return a;
	return b;
}

double poissonRandomInterarrivalDelay( double r )
{
    return (log((double) 1.0 - 
			((double) rand())/((double) RAND_MAX)))/-r;
}


void slow_down() {
	sleep(SLOW_CLIENT);
}

struct args {
    int tid;
    int is_bad;
};


void *consume(void *bundle) {
    
    int tid = ((struct args*)bundle)->tid;
    int is_bad = ((struct args*)bundle)->is_bad;

    pid_t id = pthread_self();


	int	csock;
	/*	Create the socket to the controller  */
	if ( ( csock = connectsock( host, service, "tcp" )) == 0 )
	{
		fprintf( stderr, "Cannot connect to server.\n" );
      	pthread_exit(NULL);
	}

     if (is_bad == 1) {
        slow_down();
    }

	printf( "The server is ready for consumer!\n" );
	fflush( stdout );

    if ( write(csock, CONSUME , 9) < 0 ) {
        printf( "The server has gone when getting CONSUME\n" );
        fprintf( stderr, "Consumer write: %s\n", strerror(errno) );
        close(csock);
      	pthread_exit(NULL);
	}

    char* name = malloc(10*sizeof(char));
    sprintf(name, "%d.txt", id);
    //printf("NAME: %s\n", name);
    //fflush( stdout );
    int fd = open(name, O_RDWR | O_CREAT, 0777);
	if (fd == -1) {
		printf("Error Number % d\n", errno);  
        perror("file open");
		close(csock);
      	pthread_exit(NULL);	
	}

    int size = 1000000;
    if ( read( csock, &size, sizeof(size)) <= 0 )
    {
        printf( "The server has gone when should pass size of item\n" );
        write(fd, REJECT, strlen(REJECT));
        close(fd);
        close(csock);
      	pthread_exit(NULL);
    }
    size = ntohl(size);
    printf("SIZE %d\n", size);
    fflush( stdout );

    int fd_dev = open("/dev/null", O_RDWR, 0777);
    if (fd_dev == -1) {
        printf("Error Number % d\n", errno);  
        perror("dev/null");
		close(csock);
      	pthread_exit(NULL);
    }
    
   

    int load = 1;
    int cursor = 0;
    printf("IN WHILE\n");
    while (load!=0) {
        char *buf = malloc((BUFSIZE+1)*sizeof(char));
        load = read(csock, (void *) buf, min(size - cursor, BUFSIZE));
        buf[load+1] = '\0';
        if (load < 0) {
            char err_str[strlen(BYTE_ERROR) + (int)log10(size) + 3];
            sprintf(err_str, "%s %d", BYTE_ERROR, size);
            write(fd, err_str, strlen(err_str));
            close(fd);
            break;
        }
        write(fd_dev, buf, strlen(buf));
        cursor+=load;
        // printf("cursor %d\n", cursor);
        if (cursor >= size || load == 0) {
            char print_str[strlen(SUCCESS) + (int)log10(size) + 3];
            sprintf(print_str, "%s %d", SUCCESS, size); 
            write(fd, print_str, strlen(print_str));
            close(fd);
            break;
        }
        free(buf);
    }
    printf("OUT OF WHILE\n");
    free(name);
    close(fd_dev);
    /*if ( read( csock, buf, size) <= 0 )
    {
        printf( "The server has gone when should pass buffer of item.\n" );
        close(csock);
        exit(-1);
    }*/

    //printf("Consuming: %s", buf);
    //fflush( stdout );
    close(csock);    
    printf("SUCCESS\n");
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

    int number = (int) (bad*1.0*CONSUMERS_COUNT/100.0);
	int dif = CONSUMERS_COUNT/(number+1); 
	if (dif == 0) {
		dif = 1;
	}
	int count = 0;

	for (int i = 0; i < CONSUMERS_COUNT; i++)
	{
        pthread_t	thr;
        double waiting_time = poissonRandomInterarrivalDelay(rate);
        usleep(waiting_time*1000000);

        int is_bad = 0;
		
		if (i!=0 && i%dif == 0) is_bad = 1;
		if (i == 0 && number == CONSUMERS_COUNT) is_bad = 1;
		if (count == number) is_bad = 0;

		if (is_bad == 1) {
			count++;
		}

        struct args *bundle = (struct args *)malloc(sizeof(struct args));
        bundle->tid = i;
        bundle->is_bad = is_bad;
		
        pthread_create( &thr, NULL, consume, (void *) bundle);

	}

    // sleep(10);
	// printf("CHECK BAD %d\n", count);
	pthread_exit(0);

}


