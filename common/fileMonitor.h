#ifndef FILEMONITOR_H
#define FILEMONITOR_H

#include "filetable.h"
#include <time.h>

typedef struct monitor{
	int 			isBlocked;			//block mark of the monitor
	int 			isAlive;			//alive mark of the monitor
	time_t			refreshTime;		//last fresh time of the monitor
	file_table_t *	filetable;			//file monitor maintains a file table
}Monitor;

//initializer, use malloc before calling this function
//this initializer don't do the malloc work
int fileMonitor_initialize(Monitor * monitor, file_table_t * filetable);
//start watcher
int fileMonitor_watch(Monitor * monitor, int connfd, file_table_t *filetable);

//===============================================
//call block function when you begin to download
//===============================================


//block the file monitor
int fileMonitor_block(Monitor * monitor);

//unblock the file monitor
int fileMonitor_unblock(Monitor * monitor);

//destroy the file monitor, also will distroy the file table inside
int fileMonitor_destroy(Monitor * monitor);

//stop the file monitor, need to call destroy function afterwards
int fileMonitor_stop(Monitor * monitor);

int send_update(file_table_t* filetable, int connfd);

#endif
