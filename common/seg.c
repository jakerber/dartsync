/* ========================================================================== */
/* File: seg.c
 *
 * Author:
 * Date:
 *
 * Project name:
 * Component name:
 */
/* ========================================================================== */
// ---------------- Local includes

#include "seg.h"
#include "constants.h"
#include "filetable.h"

// ---------------- System includes

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

// ---------------- Private prototypes

// ---------------- Global variables

int counter = 0;					//used for recvSeg_peerTest
file_table_t tmpTable;				//holds table

// ---------------- Functions

//Send peer segment. Return 1 if success, -1 if fail.
int sendSeg_peer(int connfd, ptp_peer_t *segPtr)
{
	char bufstart[2];
	char bufend[2];

	bufstart[0] = '!';
	bufstart[1] = '&';
	bufend[0] = '!';
	bufend[1] = '#';
	if (send(connfd, bufstart, 2, 0) < 0) {
		return -1;
	}
	if(send(connfd, segPtr, sizeof(ptp_peer_t),0)<0) {
		return -1;
	}
	if(send(connfd, bufend,2,0)<0) {
		return -1;
	}
	return 1;
}

//Recieve peer segment. Return 1 if success, -1 if fail.
int recvSeg_peer(int connfd, ptp_peer_t *segPtr)
{
	// if(recv(connfd,segPtr,sizeof(ptp_peer_t),0)<=0) {
	// 	printf("TRACKER: unable to recieve peer segment %s\n", segPtr->peer_ip);
	// 	return -1;
	// }

	char buf[sizeof(ptp_peer_t)+2]; 
	char c;
	int idx = 0;
	// state can be 0,1,2,3; 
	// 0 starting point 
	// 1 '!' received
	// 2 '&' received, start receiving segment
	// 3 '!' received,
	// 4 '#' received, finish receiving segment 
	int state = 0; 
	while(recv(connfd,&c,1,0)>0) {
		if (state == 0) {
				if(c=='!')
				state = 1;
		}
		else if(state == 1) {
			if(c=='&') 
				state = 2;
			else
				state = 0;
		}
		else if(state == 2) {
			if(c=='!') {
				buf[idx]=c;
				idx++;
				state = 3;
			}
			else {
				buf[idx]=c;
				idx++;
			}
		}
		else if(state == 3) {
			if(c=='#') {
				buf[idx]=c;
				idx++;
				state = 0;
				idx = 0;
				memcpy(segPtr ,buf,sizeof(ptp_peer_t));
				return 1;
			}
			else if(c=='!') {
				buf[idx]=c;
				idx++;
			}
			else {
				buf[idx]=c;
				idx++;
				state = 2;
			}
		}
	}
	return -1;
}

//Send artificial peer segments to test file table update
int recvSeg_peerTest(int connfd, ptp_peer_t *segPtr)
{
	// if (counter == 0) {
	// 	char dir[] = "test/";
	// 	init_fileTable(&tmpTable, dir);
	// 	fileTableFromDir(&tmpTable, dir);
	// 	printf("Initial file table from %s:\n", dir);
	// 	printFileTable(&tmpTable);
	// 	//assign values
	// 	segPtr->file_table_array = fileTableToArray(&tmpTable);
	// 	segPtr->file_table_size = tmpTable.nfiles;
	// 	segPtr->type = 0;
	// } else {
	// 	if (tmpTable.head->next->next != NULL)
	// 		deleteFile(&tmpTable, tmpTable.head->next->next);
	// 	else
	// 		return -1;
	// 	//alert user
	// 	printf("TRACKER: peer file table:\n");
	// 	printFileTable(&tmpTable);
	// 	//assign values
	// 	segPtr->file_table_array = fileTableToArray(&tmpTable);
	// 	segPtr->file_table_size = tmpTable.nfiles;
	// 	segPtr->type = 2;
	// }
	// counter++;
	return 1;
}

//Send tracker segment. Return 1 if success, -1 if fail.
int sendSeg_tracker(int connfd, ptp_tracker_t *segPtr)
{
	// if(send(connfd,segPtr,sizeof(ptp_tracker_t),0)<=0) {
	// 	printf("TRACKER: unable to send tracker segment\n");
	// 	return -1;
	// }
	// return 1;

	char bufstart[2];
	char bufend[2];

	bufstart[0] = '!';
	bufstart[1] = '&';
	bufend[0] = '!';
	bufend[1] = '#';
	if (send(connfd, bufstart, 2, 0) < 0) {
		return -1;
	}
	if(send(connfd, segPtr, sizeof(ptp_tracker_t),0)<0) {
		return -1;
	}
	if(send(connfd, bufend,2,0)<0) {
		return -1;
	}
	return 1;
}

//Recieve tracker segment. Return 1 if success, -1 if fail.
int recvSeg_tracker(int connfd, ptp_tracker_t *segPtr)
{
	// return 1;

	char buf[sizeof(ptp_tracker_t)+2]; 
	char c;
	int idx = 0;
	// state can be 0,1,2,3; 
	// 0 starting point 
	// 1 '!' received
	// 2 '&' received, start receiving segment
	// 3 '!' received,
	// 4 '#' received, finish receiving segment 
	int state = 0; 
	while(recv(connfd,&c,1,0)>0) {
		if (state == 0) {
				if(c=='!')
				state = 1;
		}
		else if(state == 1) {
			if(c=='&') 
				state = 2;
			else
				state = 0;
		}
		else if(state == 2) {
			if(c=='!') {
				buf[idx]=c;
				idx++;
				state = 3;
			}
			else {
				buf[idx]=c;
				idx++;
			}
		}
		else if(state == 3) {
			if(c=='#') {
				buf[idx]=c;
				idx++;
				state = 0;
				idx = 0;
				memcpy(segPtr ,buf,sizeof(ptp_tracker_t));
				return 1;
			}
			else if(c=='!') {
				buf[idx]=c;
				idx++;
			}
			else {
				buf[idx]=c;
				idx++;
				state = 2;
			}
		}
	}
	return -1;
}


//Send tracker segment. Return 1 if success, -1 if fail.
int sendSeg_data(int connfd, char *data, int size) {
	// if(send(connfd,segPtr,sizeof(ptp_tracker_t),0)<=0) {
	// 	printf("TRACKER: unable to send tracker segment\n");
	// 	return -1;
	// }
	// return 1;

    printf("here's the strlen of data insnide sendseg: %lu\n", strlen(data));
    printf("here's the specified size: %d\n", size);
	char bufstart[2];
	char bufend[2];

	bufstart[0] = '!';
	bufstart[1] = '&';
	bufend[0] = '!';
	bufend[1] = '#';
	if (send(connfd, bufstart, 2, 0) < 0) {
		return -1;
	}
	if(send(connfd, data, size, 0)<0) {
		return -1;
	}
	if(send(connfd, bufend,2,0)<0) {
		return -1;
	}
	return 1;
}

//Recieve tracker segment. Return 1 if success, -1 if fail.
int recvSeg_data(int connfd, char *data, int size)
{
	// return 1;
	// char buf[sizeof(ptp_tracker_t)+2]; 
	char *buf = calloc(size, sizeof(char));
	char c;
	int idx = 0;
	// state can be 0,1,2,3; 
	// 0 starting point 
	// 1 '!' received
	// 2 '&' received, start receiving segment
	// 3 '!' received,
	// 4 '#' received, finish receiving segment 
	int state = 0; 
	while(recv(connfd,&c,1,0)>0) {
		if (state == 0) {
				if(c=='!')
				state = 1;
		}
		else if(state == 1) {
			if(c=='&') 
				state = 2;
			else
				state = 0;
		}
		else if(state == 2) {
			if(c=='!') {
				buf[idx]=c;
				idx++;
				state = 3;
			}
			else {
				buf[idx]=c;
				idx++;
			}
		}
		else if(state == 3) {
			if(c=='#') {
				buf[idx]=c;
				idx++;
				state = 0;
				idx = 0;
				memcpy(data ,buf, size);
				return 1;
			}
			else if(c=='!') {
				buf[idx]=c;
				idx++;
			}
			else {
				buf[idx]=c;
				idx++;
				state = 2;
			}
		}
	}
	free(buf);
	return -1;
}
