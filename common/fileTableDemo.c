#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "fileTable.h"
#include "fileMonitor.h"

// for the
void * monitorThread(void * arg){
    Monitor * monArg = (Monitor * )arg;
    fileMonitor_watch(monArg);
    fileMonitor_destroy(monArg);
    pthread_exit(NULL);
}

int main(int argc, char** argv){
	char dir[] = "/Users/litianqi/Desktop/cs60/";
	struct fileTable* filetable = calloc(1, sizeof(struct fileTable));
	memset(filetable,0, sizeof(struct fileTable));

	init_fileTable(filetable, dir);
	fileTableFromDir(filetable,dir);
	printFileTable(filetable);


	//=====================================================
	//demo for fileTableToArray
	char *buf = fileTableToArray(filetable);
	printf("<trans to buf>\n");
	struct fileTable* filetable1 = calloc(1, sizeof(struct fileTable));
	memset(filetable1,0,sizeof(struct fileTable));
	strcpy(filetable1 -> dirPath, filetable -> dirPath);
	pthread_mutex_init(&filetable1 -> fileTableMutex, NULL);
	filetable1 -> nfiles = filetable -> nfiles;

	file_table_t* tmp = arrayToFileTable(buf, filetable -> nfiles);
	filetable1 -> head = tmp -> head;
	filetable1 -> tail = tmp -> tail;
	printf("<parse>\n");
	free(tmp);
	printFileTable(filetable1);
	//=====================================================

	Monitor* fileMonitor = calloc(1, sizeof(Monitor));
	fileMonitor_initialize(fileMonitor, filetable);
	//thread starts
	pthread_t mon;
	int err = pthread_create(&mon, NULL, monitorThread, fileMonitor);
	if(err != 0){
		perror("Failed to create the monitor thread\n");
		exit(1);
	}
	
	/** commented for now **/

	// fileMonitor_block(fileMonitor);
	// sleep(10);
	// fileMonitor_unblock(fileMonitor);
	// sleep(60);
	// fileMonitor_stop(fileMonitor);
	pthread_join(mon,NULL);

	return 0;
}
