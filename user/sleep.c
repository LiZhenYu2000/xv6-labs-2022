#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int 
main(int argc, char* argv[]) {
	if(argc > 2) {
		printf("Too many arguments!\n");
		exit(0);
	} else if(argc < 2) {
		printf("Too few arguments!\n");
		exit(0);
	}
	
	int tiks = atoi(argv[1]);
	if(tiks > 0) {
		sleep(tiks);
	}
	printf("Ready to get up!\n");
	exit(0);
}
