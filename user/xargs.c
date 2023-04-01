#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

void
skip_space(char* buf, int* idx) {
	while(buf[*idx] != 0) {
		if(buf[*idx] != ' ')
			break;
		(*idx) += 1;
	}
}

void
skip_content(char* buf, int* idx) {
	while(buf[*idx] != 0) {
		if(!(( buf[*idx] >= 'a' && buf[*idx] <= 'z' ) ||
		     ( buf[*idx] >= 'A' && buf[*idx] <= 'Z' )))
			break;
		(*idx) += 1;
	}
}

void
parse(char* buf, char* argv[]) {
	int ptr = 0,
	    idx = 0;
	// Skip leading space first to allow all space input.
	skip_space(buf, &ptr);
	while(buf[ptr] != 0) {
		skip_space(buf, &ptr);
		argv[idx] = buf + ptr;
		idx ++;
		skip_content(buf, &ptr);
		if(buf[ptr] != 0)
			buf[ptr ++] = 0;
	}
}

int
getline(char* buf) {
	char tmp = 0;
	int ok = 0;
	while(((ok = read(0, &tmp, sizeof(tmp))) == sizeof(tmp)) && tmp != '\n') {
		*buf ++ = tmp;
	}
	buf[0] = 0;
	return ok;
}

int
main(int argc, char* argv[]) {
	if(argc < 2) {
		fprintf(2, "Too few arguments!\n");
		exit(1);
	}
	
	int pid = -1;

	char buf[1024];

	while(getline(buf)) {
		pid = fork();
		if(pid == 0) {
			// son
			char* t_argv[MAXARG];
			// Copy arguments to t_argv except for argv[0]
			memmove(t_argv, argv + 1, argc * sizeof(char*));
			// Parse the line buffer and return pointers point to additional arguments
			// by second argument, remember that the first argc - 1 elements of t_argv
			// have been occupied.
			parse(buf, t_argv + argc - 1);
			exec(t_argv[0], t_argv);
		} else if(pid > 0) {
			// father
			wait((int*)0);
		} else {
			fprintf(1, "Failed to create new process!\n");
			exit(1);
		}
	}

	exit(0);
}
