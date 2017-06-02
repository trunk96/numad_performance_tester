#include <stdio.h>
#include <stdlib.h>
#include <numa.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <errno.h>

#define N_PAGES 4096*64 //4096*8 //4096
#define COUNT 10
#define SECONDS 10
#define CONTROL 0

#define NODE_CONFIGURATION0(x) (x==0 || x==4 || x==8 || x==12)
#define NODE_CONFIGURATION1(x) (x==16 || x==20 || x==24 || x==28)
#define NODE_CONFIGURATION2(x) (x==1 || x==5 || x==9 || x==13)
#define NODE_CONFIGURATION3(x) (x==17 || x==21 || x==25 || x==29)
#define NODE_CONFIGURATION4(x) (x==2 || x==6 || x==10 || x==14)
#define NODE_CONFIGURATION5(x) (x==18 || x==22 || x==26 || x==30)
#define NODE_CONFIGURATION6(x) (x==19 || x==23 || x==27 || x==31)
#define NODE_CONFIGURATION7(x) (x==3 || x==7 || x==11 || x==15)
/*[nodes][cpus] = {
{0, 4, 8, 12},
{16, 20, 24, 28},
{1, 5, 9, 13},
{17, 21, 25, 29},
{2, 6, 10, 14},
{18, 22, 26, 30},
{19, 23, 27, 31},
{3, 7, 11, 15}
};*/


int nodes, cpus, all_cpus;
int* numa_configuration;





typedef struct thread_args {
	int thread_id;
	char*** mem;
} thread_args_t;


void *thread_func(void *args) {                                                                                  //test OK
	thread_args_t* t_args = (thread_args_t*)args;
	int thread_id = t_args->thread_id;
	char *** mem = t_args->mem;
	//some work - accessing to a random memory block in another node
	int i, j, done = 0;
	char a;
	printf("Hi, i'm thread #%d and i'm running on node %d\n", thread_id, numa_configuration[thread_id]);
	fflush(stdout);
#ifdef RANDOM
	//random select a memory in a node
	srand(time(NULL));
	int counter = rand();
	counter %= cpus;
#else
	int counter = 0;
	for (i = 0; i<thread_id; i++) {
		if (numa_configuration[i] == numa_configuration[thread_id]) counter++;
	}
#endif
	printf("i'm thread #%d and i have to access to memory %d on node %d\n", thread_id, counter, numa_configuration[thread_id]);
	// selected node is the next node
	int selected_node = (numa_configuration[thread_id] + 1) % nodes;

	for (i = 0; i<COUNT; i++) {
		for (j = 0; j<N_PAGES * 4096; j++) {
			a = mem[selected_node][counter][j];
		}
	}
	free(args);
	pthread_exit(0);
}


void* thread_func2(void *args) {
	thread_args_t* t_args = (thread_args_t*)args;
	int thread_id = t_args->thread_id;
	char*** mem = t_args->mem;
	int selected_mem;
	if (numa_configuration[thread_id] % 2 == 0)
		selected_mem = (numa_configuration[thread_id] + 2) % nodes;
	else
		selected_mem = (numa_configuration[thread_id] + 1) % nodes;
	//selected_mem contains node selected for memory access. (0,1)->2 (2,3)->4 (4,5)->6 (6,7)->0

	int i, j, k, done = 0;
	char a;
	for (k = 0; k<COUNT; k++) {
		for (i = 0; i<cpus; i++) {
			for (j = 0; j<N_PAGES * 4096; j++) {
				a = mem[selected_mem][i][j];
			}
		}
	}

	free(args);
	pthread_exit(0);
}




int main(int argc, char* argv[]) {
	int i, j, k;
	nodes = numa_num_configured_nodes();
	printf("%d NUMA nodes found\n", nodes);

	all_cpus = numa_num_configured_cpus();

	cpus = all_cpus / nodes; // only for balanced cpu per node systems
	printf("%d CPU per node found\n", cpus);

	numa_configuration = malloc(all_cpus * sizeof(int));
	if (numa_configuration == NULL) {
		printf("Cannot allocate memory for numa_configuration map\n");
		exit(EXIT_FAILURE);
	}
	//insert at numa_configuration[i] the node of CPU #i
	for (i = 0; i<all_cpus; i++) {
		if (NODE_CONFIGURATION0(i)) numa_configuration[i] = 0;
		else if (NODE_CONFIGURATION1(i)) numa_configuration[i] = 1;
		else if (NODE_CONFIGURATION2(i)) numa_configuration[i] = 2;
		else if (NODE_CONFIGURATION3(i)) numa_configuration[i] = 3;
		else if (NODE_CONFIGURATION4(i)) numa_configuration[i] = 4;
		else if (NODE_CONFIGURATION5(i)) numa_configuration[i] = 5;
		else if (NODE_CONFIGURATION6(i)) numa_configuration[i] = 6;
		else if (NODE_CONFIGURATION7(i)) numa_configuration[i] = 7;
		else {
			printf("Error initializing numa_configuration map\n");
			exit(EXIT_FAILURE);
		}
	}

	char*** mem_pointers = malloc(nodes * sizeof(char**));

	for (i = 0; i<nodes; i++) {
		mem_pointers[i] = malloc(cpus * sizeof(char*));
		for (j = 0; j<cpus; j++) {
			mem_pointers[i][j] = (char*)numa_alloc_onnode(N_PAGES * 4096, i); //alloc N_PAGES memory on node i and save pointer into mem_pointers matrix
			if (mem_pointers[i][j] == NULL) {
				printf("Error allocating memory on node %d\n", i);
				exit(EXIT_FAILURE);
			}

		}
	}
	printf("Memory successfully allocated\n");
	//write on each page allocated to prevent empty-zero pages

	for (i = 0; i<nodes; i++) {
		for (j = 0; j<cpus; j++) {
			for (k = 0; k<N_PAGES * 4096; k += 4096) {
				mem_pointers[i][j][k] = 'X';
			}
		}
	}
	printf("Memory successfully written\n");

	//spawn all_cpus thread
	pthread_t threads[all_cpus];
	int ret;
	for (i = 0; i<all_cpus; i++) {
		thread_args_t* p = malloc(sizeof(thread_args_t));
		if (p == NULL) {
			printf("Cannot create memory for args\n");
			exit(EXIT_FAILURE);
		}
		p->thread_id = i;
		p->mem = mem_pointers;
#ifdef F1
		ret = pthread_create(&threads[i], NULL, thread_func, (void*)p);
#else
		ret = pthread_create(&threads[i], NULL, thread_func2, (void*)p);
#endif
		if (ret != 0) {
			printf("Cannot create thread #%d\n", i);
			exit(EXIT_FAILURE);
		}
	}

	for (i = 0; i<all_cpus; i++) {
		ret = pthread_join(threads[i], NULL);
		if (ret != 0)
		{
			printf("Cannot join thread #%d\n", i);
			exit(EXIT_FAILURE);
		}
	}

	//here all threads finished their work
	//free all memory allocated
	for (i = 0; i<nodes; i++) {
		for (j = 0; j<cpus; j++) {
			numa_free(mem_pointers[i][j], N_PAGES * 4096);
		}
		free(mem_pointers[i]);
	}
	free(numa_configuration);
	free(mem_pointers);
	exit(0);
}



