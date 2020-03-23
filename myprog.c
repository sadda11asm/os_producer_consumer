#include <stdio.h>
#include<fcntl.h> 
#include<errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
extern int errno; 



int replace_at(int fd,off_t offset, char *s, int n) {
	off_t end = lseek(fd, 0, SEEK_END);
	off_t start = lseek(fd, 0, SEEK_SET);
	off_t cur = lseek(fd, offset, SEEK_SET);
	write(fd, s, n);
	return cur+n; 
}


int insert_at(int fd, off_t offset, char*s, int n) {
	off_t end = lseek(fd, 0, SEEK_END);
	off_t cur = lseek(fd, offset, SEEK_SET);
	if (end>cur) {
		char *buf = (char *) calloc(end-cur, sizeof(char));
		read(fd, buf, end-cur);
		off_t new = lseek(fd, offset+n, SEEK_SET);
		write(fd, buf, end-cur);
	}
	cur = lseek(fd, offset, SEEK_SET);
	write(fd, s, n);	 
	return cur+n;
}

int main(int argc, char *argv[]) {
	if (argc != 5) {
		printf("Incorrect number of arguments given!");
		return 0;
	}
	char* source = argv[1];
	char* name = argv[2];
	off_t ofs = atoi(argv[3]);
	int num = atoi(argv[4]);
	int fd_s = open(source, O_RDONLY);
	if (fd_s == -1) {
		printf("Error Number % d\n", errno);
		perror(source);
		exit(-1);		
	}
	char *buf = (char *) calloc(num, sizeof(char));
	off_t end = lseek(fd_s, 0, SEEK_END);
	off_t start = lseek(fd_s, 0, SEEK_SET);
	if (ofs < start || ofs+num>=end) { 
		printf("Source file constraints are not coincide with the given offset and size");
		exit(-1);
	}
	lseek(fd_s, ofs, SEEK_SET);
	read(fd_s, buf, num);
	int fd = open(name, O_RDWR | O_CREAT, 0777);
	if (fd == -1) {
		printf("Error Number % d\n", errno);  
        	perror(name);
		exit(-1);		
	}
	insert_at(fd, ofs, buf, num);
	return 0;

}
