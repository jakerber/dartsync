/* ========================================================================== */
/* File: peer.h
 *
 * Author:
 * Date:
 *
 * Project name:
 * Component name:
 */
/* ========================================================================== */
#ifndef PEER_H
#define PEER_H

// ---------------- Prerequisites e.g., Requires "math.h"

// ---------------- Constants

#include "../common/constants.h"

// ---------------- Structures/Types

//Peer-side peer table.
typedef struct _peer_side_peer_t {
    //Remote peer IP address, 16 bytes.
    char ip[IP_LEN];
    //Current downloading file name.
    char file_name[FILE_NAME_LEN];
    //Timestamp of current downloading file.
    unsigned long file_time_stamp;
    //TCP connection to this remote peer.
    int sockfd;
    //Pointer to the next peer, linked list.
    struct _peer_side_peer_t *next;
} peer_peer_t;

//Structure to define the file information for the file monitoring system.
typedef struct {
    // Path of the file
    char filepath[200];
    // Size of the file
    int size;
    // time stamp of last modification
    unsigned long last_modify_time;
} FileInfo;


// struct to provide to individual P2PDownload threads
typedef struct p2p_info_structure {
    //TCP connection to LISTENING thread
    int sockfd;
    // for partition-file case
    int piece_id;
    // for partition-file case
    int range_start;
    int range_end;
    // file information
    char filepath[FULL_PATH_MAX_LENGTH];
    // size of the file
    int size;
    //last timestamp to maintain correct modification times
    int timestamp;
    // file name
    char filename[FILE_NAME_LEN];
} P2PInfo;


// ---------------- Public Variables

// ---------------- Prototypes/Macros

//After connecting to the tracker, receive messages from tracker, and then create P2PDownload Threads if needed.
void* thread_Main(void* arg);

//Listen on the P2P port. When receiving a data request from another peer, create a P2PUpload Thread.
void* thread_P2PListening(void* arg);

//Download data from the remote peer.
void* thread_P2PDownload(void* arg);

//Upload data to the remote peer.
void* thread_P2PUpload(void* arg);

//Monitor a local file directory. Send out updated file table to the tracker if any file changes in the local file directory.
void* thread_FileMonitor(void* arg);

//Send out heartbeat (alive) messages to the tracker to keep its online status.
void* thread_Alive(void* arg);

#endif // PEER_H
