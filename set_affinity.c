#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sched.h>


#define OUTPUT "output.txt"
#define CPUS 32
#define SECONDS 3


int main(int argc, char* argv[]) {
	int fd = -1;
	while (1) {
		fd = open(OUTPUT, O_RDONLY);
		if (fd == -1) {
			printf("Cannot open " OUTPUT ", maybe the file not exists\n");
			sleep(1);
		}
		else break;
	}
	char content[8192];
	int ret = read(fd, content, 8192);
	if (ret == -1) {
		printf("Cannot read file" OUTPUT "\n");
		exit(EXIT_FAILURE);
	}
	close(fd);
	/* file should be like this (for each line):
	* emanuele  4179  4170  4179  0    1 16:17 pts/5    00:00:00 bash
	*/
	unsigned int tid[CPUS];
	int i;
	char* line = strtok(content, "\n");
	for (i = -1; i<CPUS; i++) {
		if (i == -1) {
			line = strtok(NULL, "\n");
			continue;                               //i have to discard first line
		}
		if (line == NULL) break;          //file finished
		int j, counter = 0;
		int start_tid = -1, len_tid = 0;
		int len = strlen(line);
		for (j = 0; j<len && counter<4; j++) {
			if (line[j] == ' ') counter++;
			else if (counter == 3 && start_tid == -1) {
				start_tid = j;
				len_tid++;
			}
			else if (counter == 3) len_tid++;
			else continue;
		}
		char* tid_str = malloc(len_tid * sizeof(char));
		memcpy(tid_str, line + start_tid, len_tid);
		tid[i] = atoi(tid_str);
		printf("tid[%d]= %d\n", i, tid[i]);
		free(tid_str);

		line = strtok(NULL, "\n");
	}
	//here tid[] contains all tid of the process (except main thread)
	//so i can set affinity fo each thread in process
	for (i = 0; i<CPUS; i++) {
		ulong CPU_set = 1 << i;
		cpu_set_t *mask = &CPU_set;
		ret = sched_setaffinity(tid[i], sizeof(ulong), mask);
		if (ret == -1) {
			printf("Cannot set affinity 0x%08X for TID %d\n", CPU_set, tid[i]);
			//exit(EXIT_FAILURE);
		}
		printf("Set affinity 0x%08X for TID %d\n", CPU_set, tid[i]);
	}

	sleep(SECONDS);
	printf("\n\n");

	for (i = 0; i<CPUS; i++) {
		ulong CPU_set = 0xffffffff;
		cpu_set_t *mask = &CPU_set;
		ret = sched_setaffinity(tid[i], sizeof(ulong), mask);
		if (ret == -1) {
			printf("Cannot set affinity 0x%08X for TID %d\n", CPU_set, tid[i]);
			//exit(EXIT_FAILURE);
		}
		printf("Set affinity 0x%08X for TID %d\n", CPU_set, tid[i]);
	}

	return 0;
}
