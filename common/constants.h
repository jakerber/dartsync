/* ========================================================================== */
/* File: constants.h
 *
 * Author:
 * Date:
 *
 * Project name:
 * Component name:
 */
/* ========================================================================== */
#ifndef CONSTANTS_H
#define CONSTANTS_H

// ---------------- Prerequisites e.g., Requires "math.h"

// ---------------- Constants

#define IP_LEN 16

#define FILE_NAME_LEN 200

#define PROTOCOL_LEN 200

#define RESERVED_LEN 200

#define HANDSHAKE_PORT 9090

#define JOSH_HANDSHAKE_PORT 8288

#define NETWORK_PORT 7107

#define MAX_NAME_LEN 200

#define FULL_PATH_MAX_LENGTH 256

#define MAX_PEER_ENTRIES 10

#define HEARTBEAT_INTERVAL 60		// 10 minutes will likely be the final heartbeat interval

#define RECONNECT_INTERVAL 5		// reconnect to another tracker if current dies

#define MAX_RECV_BUF 1000000  // 1 million characters read at a time
 
#define MAX_SEND_BUF 1000000  // 1 million characters read at a time

#define MAX_PARTITIONS 10000 // partition file into 10000 pieces at most

#define PIECE_NUM_LEN 5 // amount of padding for piece num in filename

#define MAX_PORT_BUFFER_LEN 5

#define MONITOR_REFRESH_INTERVAL 10

#define MAX_TBLARRAY_LEN 10000

// ---------------- Structures/Types


// ---------------- Public Variables

// ---------------- Prototypes/Macros

#endif // CONSTANTS_H
