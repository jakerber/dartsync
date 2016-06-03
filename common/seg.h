/* ========================================================================== */
/* File: seg.h
 *
 * Author:
 * Date:
 *
 * Project name:
 * Component name:
 */
/* ========================================================================== */
#ifndef SEG_H
#define SEG_H

// ---------------- Prerequisites e.g., Requires "math.h"

#include "constants.h"
#include "filetable.h"

// ---------------- Constants
#define REGISTER 0
#define KEEP_ALIVE 1
#define FILE_UPDATE 2

// ---------------- Structures/Types

//The packet data structure sending from peer to tracker.
typedef struct segment_peer {
    // protocol length
    int protocol_len;
    // protocol name
    char protocol_name[PROTOCOL_LEN + 1];
    // packet type : 0 register, 1 keep alive, 2 update file table
    int type;
    // reserved space, you could use this space for your convenient, 8 bytes by default
    char reserved[RESERVED_LEN];
    // the peer ip address sending this packet
    char peer_ip[IP_LEN];
    // listening port number in p2p
    int port;
    // the number of files in the local file table -- optional
    int file_table_size;
    // file table of the client -- your own design
    char file_table_array[MAX_TBLARRAY_LEN];
} ptp_peer_t;

//The packet data structure sending from tracker to peer.
typedef struct segment_tracker {
    // time interval that the peer should sending alive message periodically 
    int interval;
    // piece length
    int piece_len;
    // file number in the file table -- optional
    int file_table_size;
    // file table of the tracker -- your own design
    char file_table_array[MAX_TBLARRAY_LEN];

    char password[RESERVED_LEN];
} ptp_tracker_t;

// ---------------- Public Variables

// ---------------- Prototypes/Macros

//Send peer segment. Return 1 if success, -1 if fail.
int sendSeg_peer(int connfd, ptp_peer_t *segPtr);

//Send artificial peer segments to test file table update
int recvSeg_peerTest(int connfd, ptp_peer_t *segPtr);

//Recieve peer segment. Return 1 if success, -1 if fail.
int recvSeg_peer(int connfd, ptp_peer_t *segPtr);

//Send tracker segment. Return 1 if success, -1 if fail.
int sendSeg_tracker(int connfd, ptp_tracker_t *segPtr);

//Recieve tracker segment. Return 1 if success, -1 if fail.
int recvSeg_tracker(int connfd, ptp_tracker_t *segPtr);
int recvSeg_data(int connfd, char *data, int size);
int sendSeg_data(int connfd, char *data, int size);

#endif // SEG_H
