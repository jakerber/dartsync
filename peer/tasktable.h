typedef struct task_table_entry {
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
} task_t