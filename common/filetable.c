//
//  filetable.c
//  project
//
//  Created by LITIANQI on 5/18/16.
//  Copyright © 2016 LITIANQI. All rights reserved.
//

#include "filetable.h"
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



// initialize the filetable.
// owners[MAX_PEER_ENTRIES] should be set at the tracker's side

int init_fileTable(struct fileTable* filetable, char* dir){
    filetable -> nfiles = 0;
    pthread_mutex_init(&filetable -> fileTableMutex,NULL);
    filetable -> head = calloc(1, sizeof(struct fileEntry));
    filetable -> tail = filetable -> head;
    filetable -> head -> next = NULL;
    if(dir != NULL){
        strcpy(filetable -> dirPath, dir);
        char fullPath[FULL_PATH_MAX_LENGTH];
        strcpy(fullPath,dir);
        struct stat statbuf;
        if(stat(fullPath, &statbuf) < 0){
            perror("#init_fileTable: stat");
        }else{
            struct fileEntry* newFile = malloc(sizeof(file_entry_t));
            memset(newFile,0,sizeof(*newFile));
            strcpy(newFile -> filename, "/");
            newFile -> timestamp = statbuf.st_mtime;
            newFile -> size = statbuf.st_size;
            newFile -> type = DIRECTORY;
            newFile -> next = NULL;
            newFile -> peerArraySize = 0;
            addFile(filetable,newFile);
        }
    }else{
        strcpy(filetable -> dirPath,"");
    }
    return 0;
}

int fileTableFromDir(struct fileTable* filetable, char* dir){
    struct dirent* ent = NULL;
    DIR *dirPointer;
    dirPointer = opendir(dir);
    while(NULL != (ent = readdir(dirPointer))){
        char cmpTmp[MAX_NAME_LEN];
        strcpy(cmpTmp, ent -> d_name);
        cmpTmp[1] = '\0';
        if(strcmp(cmpTmp,".") != 0){
            char fullPath[1024];
            strcpy(fullPath,dir);
            strcat(fullPath, ent -> d_name);
            struct stat statbuf;
            if(stat(fullPath, &statbuf) < 0){
                perror("#stat");
            }else{
                file_entry_t* newFile = malloc(sizeof(file_entry_t));
                memset(newFile, 0, sizeof(*newFile));
                int offset = strlen(filetable -> dirPath);
                strcpy(newFile -> filename, fullPath + offset -1);
                newFile -> timestamp = statbuf.st_mtime;
                newFile -> size = statbuf.st_size;
                newFile -> type = ent -> d_type == 4 ? DIRECTORY : REGULAR;
                newFile -> next = NULL;
                newFile -> peerArraySize = 0;
                addFile(filetable, newFile);
            }
            if(ent -> d_type == 4){
                strcat(fullPath,"/");
                fileTableFromDir(filetable, fullPath);
            }
        }
    }
    free(dirPointer);
    free(ent);
    return 0;
}// fill a filetable

int addFile(struct fileTable* filetable, struct fileEntry* newFile){
    if(filetable == NULL){
        perror("file table is empty!");
        return -1;
    }
    file_entry_t* curFile = filetable -> head;
    while(curFile -> next != NULL){
        if(strcmp(curFile -> next -> filename, newFile -> filename) < 0){
            curFile = curFile -> next;
        }else{
            break;
        }
    }
    file_entry_t* tmp = curFile -> next;
    curFile -> next = newFile;
    newFile -> next = tmp;
    filetable -> nfiles++;
    return 0;
}

int appendFileToTail(struct fileTable* filetable, struct fileEntry* newFile){
    pthread_mutex_lock(&filetable -> fileTableMutex);
    filetable -> tail -> next = newFile;
    filetable -> tail = newFile;
    filetable -> nfiles++;
    pthread_mutex_unlock(&filetable -> fileTableMutex);
    return 0;
}

int deleteFile(struct fileTable* filetable,struct fileEntry* newFile){
    if(filetable == NULL){
        perror("File table is empty!");
        return -1;
    }
    file_entry_t* curFile = filetable -> head;
    while(curFile -> next != NULL){
        if(strcmp(curFile -> next -> filename, newFile -> filename) == 0){
            file_entry_t* tmp = curFile -> next;
            curFile -> next = tmp -> next;
            free(tmp);
            filetable -> nfiles--;
            return 0;
        }
        curFile = curFile -> next;
    }
    perror("Cannot delete, file is not found");
    return -1;
}

//not implemented for now
int modifyFile(struct fileTable* filetable, struct fileEntry* newFile){
//    if(filetable == NULL){
//        perror("filetable is empty");
//        return -1;
//    }
//    file_entry_t* curFile = filetable -> head;
//    while(curFile -> next != NULL){
//        if(strcmp(curFile -> next -> filename, newFile -> filename) == 0){
//            file_entry_t* tmp = curFile -> next;
//            curFile -> next = newFile;
//            newFile -> next = tmp -> next;
//            if(newFile -> timestamp == tmp -> timestamp){
//                int i;
//                for(i = 0; i < curFile -> next -> peerArraySize; i++){
//                    addPeerTo
//                }
//            }
//            
//        }
//    }
    return -1;
}

int printFileTable(struct fileTable* filetable){
    if(filetable == NULL || filetable -> head == NULL){
        perror("file table is empty!");
        return -1;
    }
    printf("==================Current File Table====================\n");
    if(filetable -> head != NULL){
        file_entry_t* curFile = filetable -> head -> next;
        while(curFile != NULL){
            // printf("filename: %s \t filesize: %u \t type: %d \t timestamp: %ld \n", 
            // 	curFile -> filename, curFile -> size, curFile -> type, curFile ->
            // 	timestamp);
            printf("type=%d\t size=%u \t timestamp=%ld \t filename: %s\n",
					curFile->type,curFile->size,curFile->timestamp,curFile->filename);
            printf("Owners: ");
            int i;
            for(i = 0; i < curFile -> peerArraySize; i++){
            	printf("| %s |",curFile -> owners[i].ip);
            }
            printf("\n");
            curFile = curFile -> next;
        }
    }
    return 0;
}

int destroyFileTable(struct fileTable* filetable){
    if(filetable == NULL){
    	perror("file table is empty!");
    	return -1;
    }else{
    	pthread_mutex_lock(&filetable -> fileTableMutex);
    	file_entry_t * curFile = filetable -> head;
    	while(curFile != NULL){
    		file_entry_t* tmp = curFile;
    		curFile = curFile -> next;
    		free(tmp);
    	}
    	pthread_mutex_unlock(&filetable -> fileTableMutex);
    	pthread_mutex_destroy(&filetable -> fileTableMutex);
    	free(filetable);
    	return 0;
    }
}

char* fileTableToArray(struct fileTable* filetable){
    char* buf = (char*)calloc(filetable -> nfiles, sizeof(file_entry_send_t));
    file_entry_t* curFile = filetable -> head -> next;
    file_entry_send_t tmp;
    int i;
    for(i = 0; i < filetable -> nfiles; i++){
    	memset(&tmp, 0, sizeof(file_entry_send_t));
    	strcpy(tmp.filename,curFile -> filename);
    	memcpy(&tmp.size,&curFile -> size, sizeof(unsigned int));
    	memcpy(&tmp.type,&curFile -> type, sizeof(enum fileType));
    	memcpy(&tmp.timestamp, &curFile -> timestamp, sizeof(time_t));
    	memcpy(&tmp.peerArraySize, &curFile -> peerArraySize, sizeof(int));
    	int j;
    	for(j = 0; j < curFile -> peerArraySize;j++){
    		strcpy(tmp.owners[j].ip,curFile -> owners[j].ip);
    		tmp.owners[j].port = curFile -> owners[j].port;
    	}
    	memcpy(buf + i * sizeof(file_entry_send_t), &tmp, sizeof(file_entry_send_t));
    	curFile = curFile -> next;
    }
    return buf;
}

file_table_t* arrayToFileTable(char* buf, int n){
    file_table_t* ft = (file_table_t*)calloc(1, sizeof(file_table_t));
    ft -> head = (file_entry_t*)calloc(1, sizeof(file_entry_t));
    ft -> head -> next = NULL;
    file_entry_t* curFile = ft -> head;
    file_entry_send_t tmp;
    int i;
    for(i = 0; i < n; i++){
        memset(&tmp,0, sizeof(file_entry_send_t));
        file_entry_t* newFile = (file_entry_t*)calloc(1, sizeof(file_entry_t));
        memcpy(&tmp,buf + i * sizeof(file_entry_send_t), sizeof(file_entry_send_t));
        strcpy(newFile -> filename,tmp.filename);
        newFile -> size = tmp.size;
        newFile -> peerArraySize = tmp.peerArraySize;
        newFile -> timestamp = tmp.timestamp;
        newFile -> type = tmp.type;

        int j;
        for(j = 0; j < tmp.peerArraySize; j++){
            strcpy(newFile -> owners[j].ip, tmp.owners[j].ip);
            newFile -> owners[j].port = tmp.owners[j].port;
        }
        newFile  -> next = NULL;
        curFile -> next = newFile;
        curFile = newFile;
    }
    ft -> tail = curFile;
    ft->nfiles = n;
    
    return ft;
}

int deletePeerFromFileTable(file_table_t* filetable, peerEntry_t* curPeerEntry){
    file_entry_t* curFile = filetable -> head -> next;
    while(curFile != NULL){
        deletePeerFromFileEntry(curFile, curPeerEntry);
        curFile = curFile -> next;
    }
    return 0;
}

int addPeerToFileTable(file_table_t* filetable, peerEntry_t* peerentry) {
    file_entry_t* curFile = filetable -> head -> next;
    while(curFile != NULL){
        addPeerToFileEntry(curFile, peerentry);
        curFile = curFile -> next;
    }
    return 0;
}

int deletePeerFromFileEntry(file_entry_t* curFile, peerEntry_t* curPeerEntry){
    int i;
    for(i = 0; i < curFile -> peerArraySize; i++){
        if(strcmp(curFile -> owners[i].ip, curPeerEntry -> ip)){
            curFile -> peerArraySize--;
            curFile -> owners[i].port = curFile -> owners[curFile -> peerArraySize].port;
            strcpy(curFile -> owners[i].ip,curFile -> owners[curFile -> peerArraySize].ip);
            return 0;
        }
    }
    return -1;
}
//if the peerArraySize reach MAX_PEER_ENTRIES, return -1;
//if the peer already exit in the filetable, return 0;
//if the peer is successfully added to the file table, return 1;
int addPeerToFileEntry(file_entry_t* curFile, peerEntry_t* curPeerEntry){
    if(curFile -> peerArraySize >= MAX_PEER_ENTRIES){
        return -1;
    }else{
        int i;
        for(i = 0; i < curFile -> peerArraySize; i++){
            if(strcmp(curFile -> owners[i].ip, curPeerEntry -> ip) == 0){
                return 0;
            }
        }
        strcpy(curFile -> owners[curFile -> peerArraySize].ip, curPeerEntry -> ip);
        curFile -> owners[curFile -> peerArraySize].port = curPeerEntry -> port;
        curFile -> peerArraySize++;

    }
    return 1;
}
