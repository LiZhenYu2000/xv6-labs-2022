#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define RD_END 0
#define WT_END 1

int
main(int argc, char* argv[]) {
	if(argc > 1) {
		printf("Too many arguments!\n");
		exit(1);
	}
	
	int is_first = 1;
	int fds[2], tmp_buffer[40] = {0};
	do {
		if(pipe(fds) == -1) {
			printf("Failed to creat a new pipe!\n");
			exit(1);
		}


		if(fork() != 0) {
			// in father process
			int buffer[40] = {0};
			if(is_first == 1) {
				int idx = 0;
				for(int i = 2; i <= 35; ++ i) {
					buffer[idx ++] = i;
				}
				write(fds[WT_END], buffer, 40 * sizeof(int));
			} else {
				int id = tmp_buffer[0];
				int i = 1, idx = 0;
				while(tmp_buffer[i] != 0) {
					if(tmp_buffer[i] % id != 0) {
						buffer[idx ++] = tmp_buffer[i];	
					}
					tmp_buffer[i] = 0;
					++ i;
				}
				printf("prime %d\n", id);
				write(fds[WT_END], buffer, 40 * sizeof(int));
			}
			// father wait after finished his job
			wait(0);
			exit(0);
		} else {
			// in son process
			is_first = 0;
			int buffer[40] = {0};
			read(fds[RD_END], buffer, 40 * sizeof(int));
			close(fds[RD_END]);
			close(fds[WT_END]);
			int id = buffer[0];
			int i = 1, idx = 0;
			while(buffer[i] != 0) {
				if(buffer[i] % id != 0) {
					tmp_buffer[idx ++] = buffer[i];
				}
				++ i;
			}
			if(id != 0)	printf("prime %d\n", id);
			if(id == 31)	exit(0); // time to exit
		}
	} while(1); // forever loop

	// if control reached here,it means something went wrong.
	exit(1);
}
