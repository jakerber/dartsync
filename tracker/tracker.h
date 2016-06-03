/* ========================================================================== */
/* File: tracker.h
 *
 * Author:
 * Date:
 *
 * Project name:
 * Component name:
 */
/* ========================================================================== */
#ifndef TRACKER_H
#define TRACKER_H

// ---------------- Prerequisites e.g., Requires "math.h"

#include "../common/constants.h"
#include "../common/seg.h"
#include "../common/filetable.h"

// ---------------- Constants

#define HEARTBEAT_CUSHION 8

// ---------------- Structures/Types

//Tracker-side peer table.
typedef struct _tracker_side_peer_t {
    //Remote peer IP address, 16 bytes.
    char ip[IP_LEN];
    //Last alive timestamp of this peer.
    unsigned long last_time_stamp;
    //TCP connection to this remote peer.
    int sockfd;
    //Pointer to the next peer, linked list.
    struct _tracker_side_peer_t *next;
} tracker_peer_t;

// ---------------- Public Variables

// ---------------- Prototypes/Macros

//Display table of peers
void peerTable_print();

//Send file table to all alive peers
void fileTable_broadcast(tracker_peer_t *peer);

//Initialize global file table on first peer acceptance
void fileTable_init(ptp_peer_t *peerRecv);

//Update global tracker file table based on peer file table
void update_fileTable(ptp_peer_t *peerRecv);

//Terminate handshake thread thus terminating peer communications
void killThread(tracker_peer_t *peer, int err_code);

//Remove a peer from the peer table return removed peer entry
void peerTable_remove(int socketfd, char ip[]);

//Add peer to the tracker side peer table return new peer entry
tracker_peer_t* peerTable_add(int socketfd, char ip[]);

//Listen on the handshake port, and create a Handshake thread when a new peer joins.
void* thread_Main(void* arg);

//Receive messages from a specific peer and respond if needed, by using peer-tracker handshake protocol.
void* thread_Handshake(void* arg);

//Monitor and accept alive message from online peers periodically, and remove dead peers if timeout occurs.
void* thread_MonitorAlive(void* arg);

#endif // TRACKER_H
