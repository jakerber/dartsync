#include "fileMonitor.h"
#include "../common/seg.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>



int fileMonitor_initialize(Monitor * monitor, file_table_t * filetable){
	monitor -> filetable = filetable;
	monitor -> isBlocked = 0;
	monitor -> isAlive = 1;
	monitor -> refreshTime = time(NULL);
	return 0;
}
//start watcher
int fileMonitor_watch(Monitor * monitor, int connfd, file_table_t *filetable) {
	struct hostent *hostInfo;
	char hostname[50];
	peerEntry_t peerentry;

    gethostname(hostname, 50);
    hostInfo = gethostbyname(hostname);
    strcpy(peerentry.ip, inet_ntoa(*(struct in_addr *) hostInfo->h_addr_list[0]));
    peerentry.port = NETWORK_PORT;

	while(monitor -> isAlive){
		if(monitor -> isBlocked){
			sleep(MONITOR_REFRESH_INTERVAL);
			continue;
		}else{
			int isChanged = 0;
			file_table_t* newFileTable = calloc(1, sizeof(file_table_t));
			time_t newRefreshTime = time(NULL);
			init_fileTable(newFileTable,monitor -> filetable -> dirPath);
			fileTableFromDir(newFileTable, monitor -> filetable -> dirPath);
			file_entry_t* curFile = newFileTable -> head -> next;
			while(curFile != NULL){
				if(curFile -> timestamp > monitor -> refreshTime){
					//change occurred
					destroyFileTable(monitor -> filetable);
					monitor -> filetable = newFileTable;
					addPeerToFileTable(monitor->filetable, &peerentry);
					printf("#FILEMONITOR: Change Detected!\n");
					printFileTable(monitor -> filetable);
					memcpy(filetable, monitor->filetable, sizeof(file_table_t));
					send_update(monitor -> filetable, connfd);
					//send file table from peer to tracker

					monitor -> refreshTime = newRefreshTime;
					isChanged = 1;
					break;
				}
				curFile= curFile -> next;
			}
			if(isChanged == 0){
				destroyFileTable(newFileTable);
			}
			sleep(MONITOR_REFRESH_INTERVAL);
		}
	}
	return 0;
}


//block the file monitor
int fileMonitor_block(Monitor * monitor){
	monitor -> isBlocked = 1;
	printf("<file monitor is blocked!>\n");

	return 0;
}

//unblock the file monitor
int fileMonitor_unblock(Monitor * monitor){
	monitor -> isBlocked = 0;
	printf("<file monitor is unblocked!>\n");

	return 0;
}

//destroy the file monitor, also will distroy the file table inside
int fileMonitor_destroy(Monitor * monitor){
	sleep(1);
	destroyFileTable(monitor -> filetable);
	free(monitor);

	return 0;
}

//stop the file monitor, need to call destroy function afterwards
int fileMonitor_stop(Monitor * monitor){
	monitor -> isAlive = 0;
	printf("<file monitor is stopped!>\n");

	return 0;
}

int send_update(file_table_t* filetable, int connfd) {
	ptp_peer_t update_pkt;
	char *filetable_array;

	update_pkt.type = FILE_UPDATE;
	update_pkt.file_table_size = filetable->nfiles;
	filetable_array = fileTableToArray(filetable);
	memcpy(&update_pkt.file_table_array, filetable_array, filetable->nfiles * sizeof(file_entry_send_t));
	sendSeg_peer(connfd, &update_pkt);

	return 0;
}