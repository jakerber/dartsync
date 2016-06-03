/* ========================================================================== */
/* File: peer.c
 *
 * Author(s):
 * Date:
 *
 * Project name:
 * Component name:
 */
/* ========================================================================== */
// ---------------- Local includes
#include "../common/seg.h"
#include "peer.h"
#include "../common/constants.h"
#include "../common/fileMonitor.h"

// ---------------- System includes
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>

// ---------------- Private prototypes

// ---------------- Global variables
int connfd;
file_table_t *filetable;
Monitor *mon;
peerEntry_t peerentry;
char dir[50];
int active = 0;			// 1 for connected, 0 for not connected

// ---------------- Functions

int connectToTracker() {
	struct sockaddr_in servaddr;
	char hostname[50];
	struct hostent *hostInfo;

	printf("Enter server name to connect:");
	scanf("%s",hostname);
	hostInfo = gethostbyname(hostname);
	if (hostInfo == NULL) {
		return -1;
	}

	servaddr.sin_family = AF_INET;
	memcpy(&(servaddr.sin_addr), hostInfo->h_addr_list[0], hostInfo->h_length);
	servaddr.sin_port = htons(HANDSHAKE_PORT);

	int connfd = socket(AF_INET,SOCK_STREAM,0);

	if(connfd<0) {
		return -1;
	}

	if(connect(connfd, (struct sockaddr*)&servaddr, sizeof(servaddr))!=0) {
		return -1;
	}

	//succefully connected
	active = 1;
	return connfd;
}

int handshake() {
	ptp_peer_t rgstr_pkt;
	ptp_tracker_t response_pkt;
	char *filetable_array;
    file_table_t *temptable;
    file_entry_t *tempentry;
    P2PInfo *p2p_info;
    pthread_t filemonitor_thread;
    struct sockaddr_in servaddr;
    int downloadfd;
    struct stat st = {0};
    char filename_complete[50];
    char password[50];

    pthread_t *p2p_download_threads;

    fflush(stdout);
    printf("Enter password: ");

    scanf("%s", password);

	rgstr_pkt.type = REGISTER;
	if (filetable->nfiles > -1) {
		printFileTable(filetable);
		filetable_array = fileTableToArray(filetable);

		rgstr_pkt.file_table_size = filetable->nfiles;
		memcpy(&rgstr_pkt.file_table_array, filetable_array, filetable->nfiles * sizeof(file_entry_send_t));
	}

	if (sendSeg_peer(connfd, &rgstr_pkt) < 0) {
		return -1;
	}
	if (recvSeg_tracker(connfd, &response_pkt) < 0) {
		return -1;
	}
    if (strcmp(password, response_pkt.password) != 0) {
        printf("Invalid Password -- exiting...\n");
        close(connfd);
        exit(0);
    }
    printf("Password accepted!\n");

    //if filetable is empty then use the one that is received
    if (filetable->nfiles == -1) {
        temptable = arrayToFileTable(response_pkt.file_table_array, response_pkt.file_table_size);
        filetable->head = temptable->head;
        filetable->tail = temptable->tail;
        filetable->nfiles = response_pkt.file_table_size;
        strcpy(filetable->dirPath, "dartsync_folder/");
        printf("Current filetable:\n");
        printFileTable(filetable);
        tempentry = filetable->head->next;



        p2p_download_threads = calloc(filetable->nfiles, sizeof(pthread_t));
        if (p2p_download_threads == NULL) {
            fprintf(stderr, "calloc failed!\n");
            return -1;
        }

        int thread_index = 0;

        while (tempentry != NULL) {
            if (tempentry->type == 0) {
                p2p_info = calloc(1, sizeof(P2PInfo));
                servaddr.sin_family = AF_INET;
                servaddr.sin_addr.s_addr = inet_addr(tempentry->owners[0].ip);
                servaddr.sin_port = htons(tempentry->owners[0].port);

                downloadfd = socket(AF_INET, SOCK_STREAM, 0);

                if (downloadfd < 0) {
                    printf("socket error\n");
                }
                if(connect(downloadfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
                    perror("connect error");
                    return -1;
                }

                p2p_info->sockfd = downloadfd;
                p2p_info->piece_id = 1;
                p2p_info->range_start = 0;
                p2p_info->range_end = tempentry->size;
                p2p_info->size = tempentry->size;
                p2p_info->timestamp = tempentry->timestamp;
                strcpy(p2p_info->filename, tempentry->filename);

                // pthread_create(&p2p_download_thread, NULL, thread_P2PDownload, p2p_info);
                pthread_create(&p2p_download_threads[thread_index], NULL, thread_P2PDownload, p2p_info);
                thread_index += 1;
            }
            else {
                strcpy(filename_complete, dir);
                strcat(filename_complete, tempentry->filename);
                if (stat(filename_complete, &st) == -1) {
                    mkdir(filename_complete, 0700);
                }
            }

            tempentry = tempentry->next;
        }
        // pthread_join(p2p_download_thread, NULL);
        for (int i = 0; i < thread_index; ++i) { // thread_index not nfiles as a safeguard
            pthread_join(p2p_download_threads[i], NULL);
        }
        free(p2p_download_threads);
        printf("Downloads finished\n");
    }
    pthread_create(&filemonitor_thread, NULL, thread_FileMonitor, NULL);


	return 1;
}

//After connecting to the tracker, receive messages from tracker, and then create P2PDownload Threads if needed.
void* thread_Main(void* arg) {
	ptp_tracker_t tracker_pkt;
    file_table_t *temptable;
    file_entry_t *temp_entry_new, *temp_entry_old;
    int i, j;
    char filename_complete[50];
    P2PInfo *p2p_info;
    struct sockaddr_in servaddr;
    int downloadfd;
    struct stat st = {0};


    pthread_t *p2p_download_threads;

	if (handshake() < 0) {
		printf("Handshake Error -- exiting\n");
		exit(1);
	}

	while (recvSeg_tracker(connfd, &tracker_pkt) > 0) {
		printf("Filetable update received\n");

        temptable = arrayToFileTable(tracker_pkt.file_table_array, tracker_pkt.file_table_size);

        int old_array[filetable->nfiles];
        int new_array[temptable->nfiles];
        memset(old_array, 0, (filetable->nfiles)*sizeof(int));
        memset(new_array, 0, (temptable->nfiles)*sizeof(int));

        temp_entry_new = temptable->head->next;
        i = 0;
        while (temp_entry_new != NULL) {
            temp_entry_old = filetable->head->next;
            j = 0;
            while (temp_entry_old != NULL) {
                if (strcmp(temp_entry_old->filename, temp_entry_new->filename) == 0) {
                    old_array[j] = 1;
                    new_array[i] = 1;
                    if (temp_entry_new->timestamp > temp_entry_old->timestamp) {
                        old_array[j] = 2;
                        new_array[j] = 2;
                    }
                    break;
                }
                temp_entry_old = temp_entry_old->next;
                j++;
            }
            temp_entry_new = temp_entry_new->next;
            i++;
        }

        fileMonitor_block(mon);
        //check and handle for modifications and additions

        int thread_index = 0;
        p2p_download_threads = calloc(temptable->nfiles, sizeof(pthread_t));
        if (p2p_download_threads == NULL) {
            fprintf(stderr, "calloc failed!\n");
            return NULL;
        }
        for (i=0; i < (temptable->nfiles); i++) {
            if ((new_array[i] == 2) || (new_array[i] == 0)) {
                temp_entry_new = temptable->head->next;
                j=0;
                for (j=0; j<i; j++) {
                    temp_entry_new = temp_entry_new->next;
                }
                strcpy(filename_complete, dir);
                strcat(filename_complete, temp_entry_new->filename);
                if (temp_entry_new->type == 0) {
                    if (new_array[i] == 2) {
                        remove(filename_complete);
                    }
                    p2p_info = calloc(1, sizeof(P2PInfo));
                    servaddr.sin_family = AF_INET;
                    servaddr.sin_addr.s_addr = inet_addr(temp_entry_new->owners[0].ip);
                    servaddr.sin_port = htons(temp_entry_new->owners[0].port);

                    downloadfd = socket(AF_INET, SOCK_STREAM, 0);

                    if (downloadfd < 0) {
                        printf("socket error\n");
                    }
                    if(connect(downloadfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
                        perror("connect error");
                        return NULL;
                    }

                    p2p_info->sockfd = downloadfd;
                    p2p_info->piece_id = 1;
                    p2p_info->range_start = 0;
                    p2p_info->range_end = temp_entry_new->size;
                    p2p_info->size = temp_entry_new->size;
                    p2p_info->timestamp = temp_entry_new->timestamp;
                    strcpy(p2p_info->filename, temp_entry_new->filename);
                    // pthread_create(&p2p_download_thread, NULL, thread_P2PDownload, p2p_info);

                    pthread_create(&p2p_download_threads[thread_index], NULL, thread_P2PDownload, p2p_info);
                    thread_index += 1;
                }
                else {
                    if (stat(filename_complete, &st) == -1) {
                        mkdir("filename_complete", 0700);
                    }
                }
            }
        }
        for (int i = 0; i < thread_index; ++i) { // thread_index not nfiles as a safeguard
            pthread_join(p2p_download_threads[i], NULL);
        }
        free(p2p_download_threads);

        //check and handle for deletions
        for (i=0; i < (filetable->nfiles); i++) {
            if (old_array[i] == 0) {
                temp_entry_old = filetable->head->next;
                j=0;
                for (j=0; j<i; j++) {
                    temp_entry_old = temp_entry_old->next;
                }
                if (temp_entry_old->type == 0) {
                    strcpy(filename_complete, dir);
                    strcat(filename_complete, temp_entry_old->filename);
                    remove(filename_complete);
                }
            }
        }
        fileMonitor_unblock(mon);
	}
	return NULL;
}

//Listen on the P2P port. When receiving a data request from another peer, create a P2PUpload Thread.
void* thread_P2PListening(void* arg) {
    int listenfd, uploadfd;
    struct sockaddr_in tcpserv_addr;
    struct sockaddr_in tcpclient_addr;
    pthread_t P2PUpload_Thread;
    char *p2pinfo_buf;
    P2PInfo *p2pinfo;

    socklen_t tcpclient_addr_len;

    //prepare socket
    tcpclient_addr_len = sizeof(tcpclient_addr);

    listenfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (listenfd < 0){
        printf("socket creation failed\n");
        return NULL; 
    }

    memset(&tcpserv_addr, 0, sizeof(tcpserv_addr));
    tcpserv_addr.sin_family = AF_INET;
    tcpserv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    tcpserv_addr.sin_port = htons(NETWORK_PORT);

    if (bind(listenfd, (struct sockaddr *)&tcpserv_addr, sizeof(tcpserv_addr)) < 0) {
        printf("err: bind failed\n");
        perror("Error \n");
        return NULL; 
    }
    if (listen(listenfd, 1) < 0) {
        printf("listen failed\n");
        return NULL;
    }

    while (1) {
        // printf("Listening for connection.....\n");
        uploadfd = accept(listenfd,(struct sockaddr*)&tcpclient_addr,&tcpclient_addr_len);
        if (uploadfd == -1){
            printf("Connection could not be established with peer\n");
            continue;
        }
        printf("Accepted connection from peer\n");

        p2pinfo_buf = calloc(sizeof(P2PInfo), sizeof(char));

        if (recv(uploadfd, p2pinfo_buf, sizeof(P2PInfo), 0) < 0) {
            printf("Receive Error\n");
        }

        p2pinfo = (P2PInfo *) p2pinfo_buf;

        p2pinfo->sockfd = uploadfd;
        pthread_create(&P2PUpload_Thread, NULL, thread_P2PUpload, p2pinfo);
    }
	return NULL;
}



// Splits a string into substrings based on the delimeter passed in
int split(char *string, char **substrings, int capacity, char *delim) {
    char *substring = strtok(string, delim);
    int i = 0;
    while (substring != NULL) {
        substrings[i] = substring;
        substring = strtok(NULL, delim);
        i += 1;
    }
    return i;
}

/**
 * thread_P2PDownload()
 *
 * Sockfd passed in should be the listening thread
 *
 * Download data from the remote peer.
 * Upon sending a download request to update files, this thread is called to handle downloading the data
 * from the partner peer directly.
 *
 * Steps:
 * 1. Download data via socket connection into a temporary file.
 * 2. Overwrite existing file with this updated file.
 * 3. Discard temporary file on failure
 *
 *
 * Notes:
 * The actual connections are established by the downloader thread (ie. main thread).
 *
 * This main thread starts "downloader_threads" by calling (this current function).
 *
 *
 * Example:
 * pthread_create(&p2p_download_thread, NULL, P2P_Download, (void *)&sockfd);
 *
 * @param arg an already open sockfd passed in as a casted (void *) pointer.
 *
 */
void* thread_P2PDownload(void* arg)
{
    int rc, nbytes;
    P2PInfo *p2p_info = (P2PInfo *) arg;
    struct utimbuf modTime;

    // argument check
    if (p2p_info->sockfd < 0) {
        fprintf(stderr, "Bad argument to thread_P2PDownload: sockfd\n");
    } else if (p2p_info->piece_id < 0) {
        fprintf(stderr, "Bad argument to thread_P2PDownload: piece_id\n");
    } else if (p2p_info->size < 0) {
        fprintf(stderr, "Bad argument to thread_P2PDownload: FileInfo.size\n");
    }

    // send a p2p start connection request to p2p listener
    char *info_buf = calloc(sizeof(P2PInfo), sizeof(char));
    memcpy(info_buf, p2p_info, sizeof(P2PInfo));

    rc = send(p2p_info->sockfd, info_buf, sizeof(P2PInfo), 0);
    if (rc < 0) {
        fprintf(stderr, "Send error inside P2P download\n");
        perror("P2PDownload");
    }

    // file identifier setup
    char filename[200];
    // strncat(p2p_info->filename, "-%d", 2);
    // sprintf(filename, p2p_info->filename, p2p_info->piece_id);
    strcpy(filename, dir);
    strcat(filename, p2p_info->filename);

    printf("Downloading file: %s\n", p2p_info->filename);
    FILE *fp = fopen(filename, "w+"); // initialize file, overwriting olds
    fclose(fp);

    // char buf[MAX_RECV_BUF] = {0};
    char *buf = calloc(p2p_info->size + 10, sizeof(char));
    // printf("p2p_info size: %d\n", p2p_info->size);
    printf("****File size is: %d\n", p2p_info->size);
    // while ((nbytes = recvSeg_data(p2p_info->sockfd, buf, p2p_info->size + 10)) > 0) {
    nbytes = recvSeg_data(p2p_info->sockfd, buf, p2p_info->size + 10);
        // now we wait for the upload thread to initiate with us
        // connection closed
        if (nbytes == 0) {
            return NULL;
        }
        // problem receiving
        if (nbytes == -1) {
            fprintf(stderr, "recv() failed in p2p_download\n");
            perror("recv()");
        }
        // repeatedly append to the file
        // printf("appending nbytes: %d\n", nbytes);
        fp = fopen(filename, "a");
        fwrite(buf, p2p_info->size, 1, fp);
        memset(buf, 0, p2p_info->size + 10);
        fclose(fp);
    // }

    modTime.actime = time(0);
    modTime.modtime = p2p_info->timestamp;
    if (utime(filename, &modTime) < 0) {
         perror("Cannot modify time");
    }
    return NULL;
}

//Upload data to the remote peer.
// arg should be a P2PInfo struct
// the sockfd inside arg should be the one to the DL peer
void* thread_P2PUpload(void* arg)
{
    int rc;
    // char buf[MAX_SEND_BUF] = {0};
    P2PInfo *p2p_info = (P2PInfo *) arg;
    char filename_complete[50];

    strcpy(filename_complete, dir);
    strcat(filename_complete, p2p_info->filename);


    if (access(filename_complete, R_OK) < 0) {
        fprintf(stderr, "Couldn't not open file (permission or can't find) in p2pupload!\n");
    }

    FILE *fp = fopen(filename_complete, "r");
    fseek(fp, p2p_info->range_start, SEEK_SET);
    if (feof(fp)) {
        fprintf(stderr, "Range given exceeds file length\n");
        return NULL;
    }

    int num_chars = p2p_info->range_end - p2p_info->range_start - 1;
    int lim = num_chars;
    printf("Num chars: %d, limit of the file: %d\n", num_chars, lim);

    char *buf = calloc(lim + 10, sizeof(char));
    int c = fgetc(fp); // int to handle EOF
    buf[p2p_info->range_start] = c;
    int chars_read = 0;
    int index = p2p_info->range_start + 1;


    // while (num_chars > 0 && chars_read < MAX_SEND_BUF) {
    while (num_chars > 0) {        
        // have we exceeded send buffer?
        if (chars_read == lim + 1) {
            printf("exceeded limit! %d\n", lim);
            rc = sendSeg_data(p2p_info->sockfd, buf, lim);
            if (rc < 0) {
                fprintf(stderr, "send failed in p2pupload\n");
            }
            memset(buf, 0, lim);
            chars_read = 0;
            continue;
        }
        // read a character in
        if ((c = fgetc(fp)) == EOF) {
            buf[index] = c;
            fprintf(stderr, "Warning: did not upload expected number of characters\n");
            break;
        }
        buf[index] = (char) c;
        index++;
        num_chars--;
        chars_read++;
    }
    // send whatever's left in the buffer
    printf("here's the strlen of buf: %lu\n", strlen(buf));
    rc = sendSeg_data(p2p_info->sockfd, buf, lim);
    if (rc < 0) {
        fprintf(stderr, "send failed in p2pupload\n");
    }
    close(p2p_info->sockfd);
    return NULL;
}

//Monitor a local file directory; send out updated file table to the tracker if any file changes in the local file directory.
void* thread_FileMonitor(void* arg) {
	mon = calloc(1, sizeof(Monitor));
	fileMonitor_initialize(mon, filetable);
	fileMonitor_watch(mon, connfd, filetable);
	fileMonitor_destroy(mon);
	pthread_exit(NULL);
	return NULL;
}

//Send out heartbeat (alive) messages to the tracker to keep its online status.
void* thread_Alive(void* arg) {
	ptp_peer_t heartbeat_pkt;

	heartbeat_pkt.type = KEEP_ALIVE;

	while (sendSeg_peer(connfd, &heartbeat_pkt) > 0) {
		// printf("Heartbeat sent\n");
		sleep(HEARTBEAT_INTERVAL);
	}
    perror("Error: ");
	printf("Lost connection with tracker -- exiting thread\n");
	active = 0;	// inactive
	return NULL;
}

/**
 * (charlie) made these assumptions about the main thread
 *
 * Steps related to P2P_Download threads:
 * 1. Search the node argument for peers that have the latest file.
 * 2. Check the peer table for these peers.
 * 3. If not, establish a client connection with the peer uploader. Otherwise communicate on already open socket.
 * 4. Update the peer_peer_t table to include this new peer connection.
 * 5. Pass in a P2PInfo struct (refer to peer.h -- note: actual downloading done by P2P_Download thread)
 * 6. After P2P_Download thread returns, destroy the corresponding peer table slot
 *
 * @return 0 on success
 */


int main(int argc, char* argv[]) {
	pthread_t main_thread, p2p_listen_thread, heartbeat_thread;
    char hostname[50];
    struct hostent *hostInfo;
    struct stat st = {0};

	//signal for broken pipe
    signal(SIGPIPE, SIG_IGN);

    filetable = calloc(1, sizeof(file_table_t));
	connfd = -1;
	filetable->nfiles = -1;

    gethostname(hostname, 50);
    hostInfo = gethostbyname(hostname);
    strcpy(peerentry.ip, inet_ntoa(*(struct in_addr *) hostInfo->h_addr_list[0]));
    peerentry.port = NETWORK_PORT;
    strcpy(dir, "dartsync_folder/");
    if (stat("dartsync_folder", &st) == -1) {
        printf("Making new test dir\n");
        mkdir("dartsync_folder", 0700);
    }

	if ((argc > 1) && (strcmp(argv[1], "-n") == 0)) {
		
		// filetable = calloc(1, sizeof(struct fileTable));
		memset(filetable,0, sizeof(struct fileTable));

		init_fileTable(filetable, dir);
		fileTableFromDir(filetable,dir);
        addPeerToFileTable(filetable, &peerentry);
	}

    printf("waiting for tracker...\n");
    connfd = connectToTracker();
    if(connfd<0) {
        printf("fail to connect to the tracker process\n");
        exit(1);
    }

    pthread_create(&heartbeat_thread, NULL, thread_Alive, NULL);
    printf("Starting main thread\n");
    pthread_create(&main_thread, NULL, thread_Main, NULL);

    ///// NOTE: these threads stay alive for ever and aren't included in loop below
    pthread_create(&p2p_listen_thread, NULL, thread_P2PListening, NULL);
    //pthread_create(&filemonitor_thread, NULL, thread_FileMonitor, NULL);

    while (1) {
        while (active == 1) {
            sleep(RECONNECT_INTERVAL);
        }
        printf("waiting for tracker...\n");
        connfd = connectToTracker();
        if(connfd<0) {
            printf("fail to connect to the tracker process\n");
            exit(1);
        }
        pthread_create(&heartbeat_thread, NULL, thread_Alive, NULL);
        printf("Re-starting main thread\n");
        pthread_create(&main_thread, NULL, thread_Main, NULL);
    }
}
