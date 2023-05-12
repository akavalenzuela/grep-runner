#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
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
}queue;
// function to initialize the queue and its pointers and locks
void init_queue(queue *q){
    node *tmp = malloc(sizeof(node));
    tmp->next=NULL;
    q->head=q->tail= tmp;
}
// function to enqueue a char *value in the queue
void enqueue(queue *q, char *value){
    node *tmp = malloc(sizeof(node));
	// checks if malloc succeeds
    assert(tmp!=NULL);
	tmp->next = NULL;
	// copies string value of value to tmp->path
	strcpy(tmp->path, value);
    q->tail->next = tmp;
    q->tail = tmp;
}
// function to dequeue the front value of the queue to char * value
// returns -1 if empty, returns 0 otherwise
int dequeue(queue *q, char *value){
    node *tmp = q->head;
    node *new_head = tmp->next;
    if(new_head==NULL){
        return -1;
    }
	// copies string value of new_head->path to value
    strcpy(value, new_head->path);
    q->head = new_head;
	// free allocated tmp node by enqueue
	free(tmp);
    return 0;
}
// Global Variable
queue task_queue;			// the task queue implementation

//function to create the grep command
//Command: grep "search_string" "filepath" > /dev/null
void grep_command_maker(char *command, char *search_string, char *filepath){
	strcpy(command, "grep \"");
	strcat(command, search_string);
	strcat(command, "\" \"");
	strcat(command, filepath);
	strcat(command, "\" > /dev/null");
}
// the worker function
void worker_thread(char *search_string){
	// assign localpath and command
	char localpath[250];
	char command[250];
	// DIR variables and struct dirent
	DIR *dirp;
	struct dirent* dp;
	// Worker will keep on checking for task by dequeueing the queue and will break loop if empty
	while(dequeue(&task_queue, localpath)==0){
		// If worker finds a task, output [n] DIR path and perform opendir()
		// Check if opendir() can be opened and exits if error
		printf("[0] DIR %s\n", localpath);
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
					printf("[0] ENQUEUE %s\n", output_path);
				}
				else if(dp->d_type==DT_REG){
					// If a regular file, make the grep command grep "search_string" "output" and check the output
					grep_command_maker(command, search_string, output_path);
					if(system(command)==0){
						// If system(command) returns 0, it means search_string is present in file and prints [worker_id] PRESENT output_path
						printf("[0] PRESENT %s\n", output_path);
					}
					else{
						// Otherwise, it means search_string is not present in file and prints [worker_id] ABSENT output_path
						printf("[0] ABSENT %s\n", output_path);
					}
				}
			}
		}
		closedir(dirp);
	}
}
int main(int argc, char *argv[]){
	// Checks if 3 arguments are passed, return 0 if less than 3
	if(argc<4 || argc>4){
		printf("Invalid number of arguments: Must input 3 arguments\n");
		return 0;
	}
	// Get the absolute path using realpath(argv[2], null). Also checks if path exists, outputs error otherwise
    char *rootpath = realpath(argv[2], NULL);
	// Assign argv[3] to search_string
	char *search_string = argv[3];
	if(!rootpath){
		return 0;
	}
	// initialize task_queue and enqueue rootpath
	init_queue(&task_queue);
	enqueue(&task_queue, rootpath);
	// calls worker_thread function
	worker_thread(search_string);
	// free task_queue head and rootpath
	free(task_queue.head);
	free(rootpath);
	return 0;
}