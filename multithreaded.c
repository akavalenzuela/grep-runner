#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>

// data struct that will be used by the queue data struct
typedef struct node{
    char path[250];			// This will be the paths that will be stored in the task queue
    struct node *next;		// This is the pointer to the next node
}node;
// The queue struct that will be used for the task queue implementation
typedef struct{
    node *head;		// a pointer to the head/front of the queue
    node *tail;		// a pointer to the tail/rear of the queue
    pthread_mutex_t head_lock, tail_lock;		//lock for head and tail pointer, used for enqueue and dequeue
}queue;
// function to initialize the queue and its pointers and locks
void init_queue(queue *q){
    node *tmp = malloc(sizeof(node));
    tmp->next=NULL;
    q->head=q->tail= tmp;
    pthread_mutex_init(&q->head_lock, NULL);
    pthread_mutex_init(&q->tail_lock, NULL);
}
// function to enqueue a char *value in the queue
void enqueue(queue *q, char *value){
    node *tmp = malloc(sizeof(node));
	// checks if malloc succeeds
    assert(tmp!=NULL);
	tmp->next = NULL;
	// Critical section of enqueue, because q->tail is involved
	pthread_mutex_lock(&q->tail_lock);
	// copies string value of value to tmp->path
	strcpy(tmp->path, value);
    q->tail->next = tmp;
    q->tail = tmp;
    pthread_mutex_unlock(&q->tail_lock);
}
// function to dequeue the front value of the queue to char * value
// returns -1 if empty, returns 0 otherwise
int dequeue(queue *q, char *value){
	// Critical section of dequeue, because q->head is involved
    pthread_mutex_lock(&q->head_lock);
    node *tmp = q->head;
    node *new_head = tmp->next;
	// Unlocks lock and returns -1 if queue is empty
    if(new_head==NULL){
        pthread_mutex_unlock(&q->head_lock);
        return -1;
    }
	// copies string value of new_head->path to value
    strcpy(value, new_head->path);
    q->head = new_head;
    pthread_mutex_unlock(&q->head_lock);
	// free allocated tmp node by enqueue
	free(tmp);
    return 0;
}

// Global Variables
int N;						// number of worker threads
int *worker_id;			// Designated Id of worker thread
int *worker_standby;		// array to check if all workers are in standby and to break loop
pthread_mutex_t t_lock;		// thread lock
char search_string[250];	// the search string of grep
queue task_queue;			// the task queue implementation

//function to create the grep command
//Command: grep "search_string" "filepath" > /dev/null
void grep_command_maker(char *command, char *filepath){
	strcpy(command, "grep \"");
	strcat(command, search_string);
	strcat(command, "\" \"");
	strcat(command, filepath);
	strcat(command, "\" > /dev/null");
}
// function to check if all workers are standby.
// If all items in worker_standby is equal to 1, return 1. Otherwise return 0.
int is_all_standby(){
    for(int i=0; i<N; i++){
        if(worker_standby[i]==0) return 0;
    }
    return 1;
}
// the worker function
void worker_thread(int *arg){
	// assign arg to worker_id
	int worker_id = *arg;
	// Variable to end loop if all threads are on standby
	int is_standby = 0;
	char localpath[250];
	DIR *dirp;
	struct dirent* dp;
	char command[250];
	// Worker will keep on checking for task by dequeueing the queue.
	// If is_stanby==1, break loop
	while(!is_standby){
		pthread_mutex_lock(&t_lock);
		is_standby = is_all_standby();
        pthread_mutex_unlock(&t_lock);
        int i = dequeue(&task_queue, localpath);
		// If dequeue returns -1, it's empty and will assign worker_standby[worker_id]
		// Otherwise, proceed.
		if(i == -1){
            pthread_mutex_lock(&t_lock);
			worker_standby[worker_id]=1;
            pthread_mutex_unlock(&t_lock);
		}
		else{
			// Assign worker_standby[worker_id] = 0;
            pthread_mutex_lock(&t_lock);
            worker_standby[worker_id]=0;
			pthread_mutex_unlock(&t_lock);
			// Able to dequeue a directory, therefore print [worker_id] DIR localpath
			printf("[%d] DIR %s\n", worker_id, localpath);
			// Used to check if localpath exists. Exits otherwise
			if((dirp = opendir(localpath))==NULL){
				printf("\n cannot open\n");
				closedir(dirp);
				exit(1);
			}
			// Loop to check all entries in the directory and will stop if dp is NULL
			for(dp=readdir(dirp); dp!=NULL; dp=readdir(dirp)){
				// Code to filter out . and .. objects
				if(strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..")){
					// Code to make the path of the file/directory
					char output_path[250];
					strcpy(output_path, localpath);
					strcat(output_path, "/");
					strcat(output_path, dp->d_name);
					// Conditional statements to check if the type is a directory or a regular file
					if(dp->d_type==DT_DIR){
						// If a directory, enqueue output and print [worker_id] ENQUEUE output_path
						enqueue(&task_queue, output_path);
						printf("[%d] ENQUEUE %s\n", worker_id, output_path);
					}
					else if(dp->d_type==DT_REG){
						// If a regular file, make the grep command grep "search_string" "output" and check the output
						grep_command_maker(command, output_path);
						if(system(command)==0){
							// If system(command) returns 0, it means search_string is present in file and prints [worker_id] PRESENT output_path
							printf("[%d] PRESENT %s\n", worker_id, output_path);
						}
						else{
							// Otherwise, it means search_string is not present in file and prints [worker_id] ABSENT output_path
							printf("[%d] ABSENT %s\n", worker_id, output_path);
						}
					}
				}
			}
			// closedir() after opening and reading the directory
			closedir(dirp);
		}
	}
}
// The main function
int main(int argc, char *argv[]){
	// Checks if 3 arguments are passed, return 0 if less than 3
	if(argc<4 || argc>4){
		printf("Invalid number of arguments: Must input 3 arguments\n");
		return 0;
	}
	// Code to convert argv[1] to integer, returns 0 if not an integer from 1-8
	N = atoi(argv[1]);
	if(N==0 || N>8 || N<1){
		printf("Not an integer from 1-8\n");
		return 0;
	}
	// Get the absolute path using realpath(argv[2], null). Also checks if path exists, outputs error otherwise
    char *rootpath = realpath(argv[2], NULL);
	if(!rootpath){
		return 0;
	}
	// Copy abspath to rootpath and argv[3] to search_string
	strcpy(search_string, argv[3]);
	// initialize task_queue and enqueue rootpath
	init_queue(&task_queue);
	enqueue(&task_queue, rootpath);
	// create p_thread t[N] and malloc worker_id and worker_standby
	pthread_t t[N];
	worker_id = (int*)malloc(N*sizeof(int));
	worker_standby = (int*)malloc(N*sizeof(int));
	// initialize t_lock
	pthread_mutex_init(&t_lock, NULL);
	// Loop about N times and assign worker_id, worker_standby and pthread_create with their worker_id
    for(int i=0; i<N; i++){
        worker_id[i]=i;
        worker_standby[i]=0;
        pthread_create(&t[i], NULL, (void *)worker_thread, &worker_id[i]);
    }
	// Loop for the pthread_join
	for(int i=0; i<N; i++){
    	pthread_join(t[i], NULL);
    }
	// free worker_id and worker_standby, queue head and rootpath
	free(worker_id);
	free(worker_standby);
    free(task_queue.head);
	free(rootpath);
	return 0;
}