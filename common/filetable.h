#ifndef FILETABLE_H
#define FILETABLE_H

#include "constants.h"

#include <pthread.h>
#include <stdint.h>

enum fileType{
    REGULAR,
    DIRECTORY,
};

typedef struct peerListEntry{
    char                 ip[IP_LEN]; 
    uint16_t             port;
}peerEntry_t;

typedef struct fileEntry{
    char                 filename[MAX_NAME_LEN];
    time_t               timestamp;
    unsigned int         size; //size of the file
    enum fileType        type;
    int                  peerArraySize;  //how many peers have this file;
    struct peerListEntry owners[MAX_PEER_ENTRIES];
    struct fileEntry*    next;
}file_entry_t;

typedef struct fileEntrySend{
    char                 filename[MAX_NAME_LEN];
    time_t               timestamp;
    unsigned int         size;
    enum fileType        type;
    int                  peerArraySize;
    struct peerListEntry owners[MAX_PEER_ENTRIES];
}file_entry_send_t;

typedef struct fileTable{
    char                 dirPath[MAX_NAME_LEN]; //name of target directory
    int                  nfiles; //number of files in the table
    pthread_mutex_t      fileTableMutex;  // mutex of the table
    struct fileEntry*    head; // head of the file list
    struct fileEntry*    tail; // tail of the file list
}file_table_t;

int init_fileTable(struct fileTable* filetable, char* dir); //initialize the filetable

int fileTableFromDir(struct fileTable* filetable, char* dir); // fill a filetable

int addFile(struct fileTable* filetable, struct fileEntry* newFile);

int appendFileToTail(struct fileTable* filetable, struct fileEntry* newFile);

int deleteFile(struct fileTable* filetable,struct fileEntry* file);

int modifyFile(struct fileTable* filetable, struct fileEntry* file);

int printFileTable(struct fileTable* filetable);

int destroyFileTable(struct fileTable* filetable);

int deletePeerFromFileTable(file_table_t* filetable, peerEntry_t* curPeerEntry);

int addPeerToFileTable(file_table_t *filetable, peerEntry_t* peerentry);

int deletePeerFromFileEntry(file_entry_t* curFileEntry, peerEntry_t* curPeerEntry);

int addPeerToFileEntry(file_entry_t* curFileEntry, peerEntry_t* curPeerEntry);

char* fileTableToArray(struct fileTable* filetable);

file_table_t* arrayToFileTable(char* buf, int n);
#endif 
