#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

int find(char* path, char* file) {
	char buffer[512] = {0};
	struct stat st; // data structure of fd
	struct dirent dir; // objects in dir
	int fd;
	int p_n = strlen(path),
	    f_n = strlen(file);

	// printf("Enter find!\n");

	if(p_n + 1 + f_n + 1 > sizeof(buffer)) {
		fprintf(1, "The path is too long to trace!\n");
		exit(1);
	}

	strcpy(buffer, path);
	if((fd = open(buffer, 0)) < 0) {
		fprintf(1, "Can not open the file in the path!\n");
		exit(1);
	}

	// printf("File open successfully!\n");

	if(fstat(fd, &st) < 0) {
		fprintf(1, "Can not get the data structure of file!\n");
		exit(1);
	}

	// printf("State of file open successfully!\n");

	if(st.type != T_DIR) {
		fprintf(1, "Path is not a directory!\n");
		exit(1);
	}
	
	char* tmp = buffer + p_n;
	tmp[0] = '/';
	++ tmp;

	while(read(fd, &dir, sizeof(dir)) == sizeof(dir)) {
		if(dir.inum == 0)
			continue;
		memmove(tmp, dir.name, DIRSIZ);
		tmp[DIRSIZ] = 0;
		
		// printf("%s\n", dir.name);

		if(stat(buffer, &st) < 0) {
			fprintf(2, "Can not get the data structure of file!\n");
			continue;
		}
		
		switch(st.type) {
		case T_DEVICE:
		case T_FILE:
			if(strcmp(dir.name, file) == 0) {
				printf("%s\n", buffer);
			}
			break;
		case T_DIR:
			if(strcmp(".", tmp) != 0 && strcmp("..", tmp) != 0)
				find(buffer, file);
			break;
		}
	}

	return 0;
}

int
main(int argc, char* argv[]) {
	if(argc < 3) {
		fprintf(2, "Too few arguments!\n");
		exit(1);
	} else if(argc > 3) {
		fprintf(2, "Too many arguments!\n");
		exit(1);
	}

	char* bg_path = argv[1];
	
	find(bg_path, argv[2]);

	exit(0);
}
