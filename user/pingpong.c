#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define RD 0
#define WT 1

int
main(int argc, char* argv[]) {
	int ptr[2];
	if(pipe(ptr) == -1) {
		fprintf(2, "Error, can not open pipe!\n");
		exit(1);
	}

	if(fork() == 0) {
		char s_data = 's', r_data = 0;
		read(ptr[RD], &r_data, 1);
		close(ptr[RD]);
		printf("%d: received ping\n", getpid());

		write(ptr[WT], &s_data, 1);
		close(ptr[WT]);
		
	} else {
		char s_data = 'f', r_data = 0;
		int status = 0;
		write(ptr[WT], &s_data, 1);
		close(ptr[WT]);
		
		wait(&status);

		read(ptr[RD], &r_data, 1);
		close(ptr[RD]);
		printf("%d: received pong\n", getpid());
	}

	exit(0);
}
