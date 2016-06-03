/* ========================================================================== */
/* File: tracker.c
 *
 * Author(s):
 * Date:
 *
 * Project name:
 * Component name:
 */
/* ========================================================================== */
// ---------------- Local includes

#include "tracker.h"
#include "../common/constants.h"
#include "../common/seg.h"

// ---------------- System includes

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>

// ---------------- Private prototypes

// ---------------- Global variables

pthread_t pthread_Main;                     //main thread
pthread_t pthread_MonitorAlive;             //monitor alive thread
pthread_mutex_t *mutexLock;                 //file mutex
file_table_t *tempTable_tracker;            //holds file table when recieved from peer
tracker_peer_t *peerTable;                  //tracker table of peers
int peerTable_size = 0;                     //size of ^
file_table_t fileTable;                     //tracker global file table
int fileTable_size = 0;                     //size of ^
int xyz = 0;
char password[50];

// ---------------- Functions

//Listen on the handshake port, and create a handshake thread when a new peer joins.
void* thread_Main(void* arg) 
{
	// open socket
    int sockfd;
    if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\nUnable to open socket\n");
        return NULL;
    }
    // initialize socket address
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_port = htons(HANDSHAKE_PORT);		// handshake port
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_family = AF_INET;
    // bind socket
    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        printf("\nUnable to bind with port %i\n", HANDSHAKE_PORT);
        return NULL;
    }
    // make listening socket
    if (listen(sockfd, 1) < 0) {
        printf("\nListening problem with socket %i", sockfd);
        return NULL;
    }
    printf("TRACKER: inititalized\n");
    // infinately listen on handshake port
    socklen_t size;
    struct sockaddr_in clientaddr;
    int peerfd;
    printf("TRACKER: waiting for peers...\n");
    while (1) {
        size = sizeof(clientaddr);
        // accept connection from peer
        peerfd = accept(sockfd, (struct sockaddr *)&clientaddr, &size);

        printf("TRACKER: accepted peer connection\n");    
        // add new peer to tracker-side peer table
        tracker_peer_t *newPeer = peerTable_add(peerfd, inet_ntoa(clientaddr.sin_addr));
        // start handshake thread for specific pear
        pthread_t pthread_Handshake;
        if(pthread_create(&pthread_Handshake, NULL, thread_Handshake, (void*)newPeer) < 0)
            printf("TRACKER: unable to shake hands with peer\n");
    }
}

//Receive messages from a specific peer and respond if needed, by using peer-tracker handshake protocol.
void* thread_Handshake(void* arg) 
{
    tracker_peer_t *peer = (tracker_peer_t*)arg;
    ptp_peer_t recvSeg;
    ptp_tracker_t sendSeg;
    char *fileTable_array;
    //recieve segment
    while(recvSeg_peer(peer->sockfd, &recvSeg) > 0) {
        //segment handling 
        //packet types : 
            //0 register
            //1 keep alive, 
            //2 update file table
        switch(recvSeg.type) {
            case REGISTER://register
                printf("TRACKER: received peer register\n");
                peer->last_time_stamp = (int) time(NULL);
                //recieve it's file table if first peer
                if (xyz == 0)
                    fileTable_init(&recvSeg);
                    //printFileTable(arrayToFileTable(recvSeg.file_table_array, recvSeg.file_table_size));
                //send ACCEPT back with data
                bzero(&sendSeg, sizeof(ptp_tracker_t));
                sendSeg.interval = HEARTBEAT_INTERVAL;
                sendSeg.piece_len = 100;
                sendSeg.file_table_size = fileTable_size;
                strcpy(sendSeg.password, password);
                fileTable_array = fileTableToArray(&fileTable);
                memmove(&sendSeg.file_table_array, fileTable_array, sendSeg.file_table_size * sizeof(file_entry_send_t));
                if (sendSeg_tracker(peer->sockfd, &sendSeg) < 0)
                    killThread(peer, 1);

                //handshake success
                printf("TRACKER: handshake success for %s:%i\n", peer->ip, peer->sockfd);
                xyz++;
                break;
            case KEEP_ALIVE://keep alive
                //sleep(HEARTBEAT_INTERVAL);
                printf("TRACKER: heartbeat\n");
                peer->last_time_stamp = (int) time(NULL);
                break;
            case FILE_UPDATE://update file table
                printf("TRACKER: received file table update\n");
                peer->last_time_stamp = (int) time(NULL);
                update_fileTable(&recvSeg);
                fileTable_broadcast(peer);
                break;
        }
    }
    printf("Exiting handshake thread\n");
    killThread(peer, 0);
    return NULL;
}

//Send file table to all alive peers
void fileTable_broadcast(tracker_peer_t *peer)
{
    int success = 0;
    tracker_peer_t *peerPtr = peerTable;
    char *fileTable_array;
    ptp_tracker_t peerSeg;
    for (int i = 0; i < peerTable_size; i++) {
        //initialize new segment
        bzero(&peerSeg, sizeof(ptp_tracker_t));
        peerSeg.interval = HEARTBEAT_INTERVAL;
        peerSeg.piece_len = 100;
        peerSeg.file_table_size = fileTable_size;
        fileTable_array = fileTableToArray(&fileTable);
        memmove(&peerSeg.file_table_array, fileTable_array, fileTable_size * sizeof(file_entry_send_t));
        //send it
        if(sendSeg_tracker(peerPtr->sockfd, &peerSeg) < 0)
            success = 1;
        peerPtr = peerPtr->next;
    }
    if (success != 0)
        killThread(peer, 1);
}

//Update global tracker file table based on peer file table
void update_fileTable(ptp_peer_t *peerRecv)
{
    //free all memory in current file table
    file_entry_t *entryPtr_tracker = fileTable.head;
    fileTable.head = NULL;    
    fileTable.tail = NULL;    
    file_entry_t *entryPtr_next = entryPtr_tracker;    
    for (int i = 0; i < fileTable_size; i++) {
        //free(entryPtr_tracker);
        entryPtr_next = entryPtr_next->next;
        entryPtr_tracker = entryPtr_next;
    }
    //change global to file table from peer    
    //fileTable_init(&peerRecv);
    free(tempTable_tracker);
    //re-attach pointers    
    tempTable_tracker = arrayToFileTable(peerRecv->file_table_array, peerRecv->file_table_size);
    fileTable.head = tempTable_tracker->head;
    fileTable.tail = tempTable_tracker->tail;
    fileTable.nfiles = peerRecv->file_table_size;
    fileTable_size = peerRecv->file_table_size;
    printf("TRACKER: file table updated to:\n");
    printFileTable(&fileTable);
}

//Initialize global file table on first peer acceptance
void fileTable_init(ptp_peer_t *peerRecv)
{
    tempTable_tracker = arrayToFileTable(peerRecv->file_table_array, peerRecv->file_table_size);
    fileTable.head = tempTable_tracker->head;
    fileTable.tail = tempTable_tracker->tail;
    fileTable.nfiles = peerRecv->file_table_size;
    fileTable_size = peerRecv->file_table_size;
    //print initial file table
    printf("File table initialized to (size %i):\n", fileTable_size);
    printFileTable(&fileTable);
}

//Terminate handshake thread thus terminating peer communications
void killThread(tracker_peer_t *peer, int err_code)
{
    //ERROR CODES
        //0 heartbeat lost
        //1 unable to send handshake
        //2 unable to initialize pthread_MonitorAlive
    //alert user
    if (err_code == 0)
        printf("TRACKER: heartbeat lost -- closing connection %s:%i\n", peer->ip, peer->sockfd);
    else if (err_code == 1)
        printf("TRACKER: communication lost -- closing connection %s:%i\n", peer->ip, peer->sockfd);
    else
        printf("TRACKER: unable to initialize monitoring thread -- closing connection %s:%i\n", peer->ip, peer->sockfd);
    //liquidate peer
    close(peer->sockfd);
    peerTable_remove(peer->sockfd, peer->ip);
    //exit thread
    pthread_exit(NULL);
}

//Monitor and accept alive message from online peers periodically, and remove dead peers if timeout occurs.
void* thread_MonitorAlive(void* arg)
{
    tracker_peer_t *trackerPeer_ptr;
    while (1) {
        sleep(HEARTBEAT_INTERVAL + HEARTBEAT_CUSHION);
        trackerPeer_ptr = peerTable;
        //check all peer time stamps
        for (int i = 0; i < peerTable_size && trackerPeer_ptr != NULL; i++) {
            //remove dead peer
            if ((int) time(NULL) - trackerPeer_ptr->last_time_stamp > HEARTBEAT_INTERVAL) {
                printf("MONITOR ALIVE: removing dead peer %s:%i\n", trackerPeer_ptr->ip, trackerPeer_ptr->sockfd);
                peerTable_remove(trackerPeer_ptr->sockfd, trackerPeer_ptr->ip);
            }
            trackerPeer_ptr = trackerPeer_ptr->next;
        }
    }
	return NULL;
}

//Remove a peer from the peer table return removed peer entry
void peerTable_remove(int socketfd, char ip[])
{
    tracker_peer_t *peerPtr;
    tracker_peer_t *peerPrevPtr;
    int count = 0;

    //empty table
    if (peerTable_size == 0) {
        peerTable_print();
        return;
    } else {
        //find peer in linked list
        peerPtr = peerTable;
        peerPrevPtr = peerPtr;
        while (peerPtr != NULL) {
            count++;
            //peer found
            if (strcmp(peerPtr->ip, ip) == 0 && peerPtr->sockfd == socketfd) {
                //removing first item
                if (count == 1)
                    peerTable = peerPtr->next;
                else
                    peerPrevPtr->next = peerPtr->next;
                close(peerPtr->sockfd);             // close connection
                free(peerPtr);                      // free memory
                peerTable_size--;
                peerTable_print();
                return;
            }
            peerPrevPtr = peerPtr;
            peerPtr = peerPtr->next;
        }
    }
    peerTable_print();
    return;
}

//Add peer to the tracker side peer table return new peer entry
tracker_peer_t* peerTable_add(int socketfd, char ip[])
{
    //dynamically allocate for peer in table
    tracker_peer_t *peerPtr = malloc(sizeof(tracker_peer_t));;

    //add to first slot in table
    if (peerTable_size == 0) {
        peerPtr->sockfd = socketfd;
        peerPtr->next = NULL;
        strcpy(peerPtr->ip, ip);
        peerPtr->last_time_stamp = (int) time(NULL);
        peerTable = peerPtr;
    } else {
        //loop to end of linked list
        peerPtr = peerTable;
        while (peerPtr->next != NULL)
            peerPtr = peerPtr->next;
        peerPtr->next = malloc(sizeof(tracker_peer_t));
        peerPtr = peerPtr->next;
        peerPtr->sockfd = socketfd;
        peerPtr->next = NULL;
        strcpy(peerPtr->ip, ip);
        peerPtr->last_time_stamp = (int) time(NULL);
    }
    peerTable_size++;
    peerTable_print();
    return peerPtr;
}

//Display table of peers
void peerTable_print()
{
    tracker_peer_t *peerPtr = peerTable;
    printf("TRACKER: peer table (size %i):\n", peerTable_size);
    int i = 0;
    while (peerPtr != NULL) {
        i++;
        printf("\tpeer %i: %s, %i, %lu\n", i, peerPtr->ip, peerPtr->sockfd, peerPtr->last_time_stamp);
        peerPtr = peerPtr->next;
    }
}

int main(int argc, char *argv[]) {
    printf("Enter desired password: ");
    scanf("%s", password);
    //signal for broken pipe
    signal(SIGPIPE, SIG_IGN);

	//initilize thread mutex
	mutexLock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(mutexLock, NULL);

	//start the main thread to listen for handshakes from peers
	if(pthread_create(&pthread_Main, NULL, thread_Main, (void*)0) < 0)
        printf("TRACKER: unable to inititalize pthread_Main\n");

    //start the monitoring thread to remove dead peers
    if(pthread_create(&pthread_MonitorAlive, NULL, thread_MonitorAlive, (void*)0) < 0)
        printf("TRACKER: unable to inititalize pthread_MonitorAlive\n");

	//keep threads alive
	pthread_join(pthread_Main, NULL);
}


