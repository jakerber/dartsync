//----------------------
//
// C-Men
//
// DartSync
//
// ---------------------

/* The packet data structure sending from peer to tracker */
typedef struct segment_peer {
    // protocol length
    int protocol_len;
    // protocol name
    char protocol_name[PROTOCOL_LEN + 1];
    // packet type : register, keep alive, update file table
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
    file_t file_table;
} ptp_peer_t;

/* The packet data structure sending from tracker to peer */
typedef struct segment_tracker {
    // time interval that the peer should sending alive message periodically
    int interval;
    // piece length
    int piece_len;
    // file number in the file table -- optional
    int file_table_size;
    // file table of the tracker -- your own design
    file_t file_table;
} ptp_tracker_t;

/* each file can be represented as a node in file table */
typedef struct node {
    //the size of the file
    int size;
    //the name of the file
    char *name;
    //the timestamp when the file is modified or created
    unsigned long int timestamp;
    //pointer to build the linked list
    struct node *pNext;
    //for the file table on peers, it is the ip address of the peer
    //for the file table on tracker, it records the ip of all peers which has the
    //newest edition of the file
    char *newpeerip;
} Node,*pNode;

/* Peer-side peer table. */
typedef struct _peer_side_peer_t {
    // Remote peer IP address, 16 bytes.
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

/* Tracker-side peer table. */
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

/* Structure to define the file information. */
typedef struct {
    // Path of the file
    char filepath[200];
    // Size of the file
    int size;
    // time stamp of last modification
    unsigned long last_modify_time;
} FileInfo;
