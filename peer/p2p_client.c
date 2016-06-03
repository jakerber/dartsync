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
#include "peer.h"
#include "../common/constants.h"

// ---------------- System includes
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

// ---------------- Private prototypes

// ---------------- Global variables
int connfd;

// ---------------- Functions





//Listen on the P2P port. When receiving a data request from another peer, create a P2PUpload Thread.
void* thread_P2PListening(void* arg)
{
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
    printf("\nhere\n");
    int rc, nbytes;
    P2PInfo *p2p_info = (P2PInfo *) arg;

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

    printf("about to send\n");

    rc = send(p2p_info->sockfd, info_buf, sizeof(P2PInfo), 0);
    if (rc < 0) {
        fprintf(stderr, "Send error inside P2P download\n");
        perror("P2PDownload");
    }

    // file identifier setup
    char filename[200];
    strncat(p2p_info->filename, "-%d", 2);
    sprintf(filename, p2p_info->filename, p2p_info->piece_id);

    FILE *fp = fopen(filename, "w+"); // initialize file, overwriting olds
    fclose(fp);

    char buf[MAX_RECV_BUF] = {0};
    while (1) {
        // now we wait for the upload thread to initiate with us
        nbytes = recv(p2p_info->sockfd, &buf, MAX_RECV_BUF, 0);
        printf("nbytes: %d\n", nbytes);
        printf("recv buf: %s\n", buf);
        // connection closed
        if (nbytes <= 0) {
            break;
        }
        // problem receiving
        if (nbytes == -1) {
            fprintf(stderr, "recv() failed in p2p_download\n");
            perror("recv()");
        }
        // repeatedly append to the file
        fp = fopen(filename, "a");
        //fwrite(buf, 1, strlen(buf), fp);
        fwrite(buf, strlen(buf), 1, fp);
        memset(buf, 0, MAX_RECV_BUF);
    }
    fclose(fp);
    return NULL;
}

//Upload data to the remote peer.
// arg should be a P2PInfo struct
// the sockfd inside arg should be the one to the DL peer
void* thread_P2PUpload(void* arg)
{
    int rc, nbytes;
    char buf[MAX_SEND_BUF] = {0};
    P2PInfo *p2p_info = (P2PInfo *) arg;

    if (access(p2p_info->filename, R_OK) < 0) {
        fprintf(stderr, "Couldn't not open file (permission or can't find) in p2pupload!\n");
    }

    printf("filename:%s\n", p2p_info->filename);
    FILE *fp = fopen(p2p_info->filename, "r");
    if (fp == NULL) {
        printf("BAD!\n");
        return NULL;
    }

    fseek(fp, 0L, SEEK_END);
    int fs = ftell(fp);

    fseek(fp, p2p_info->range_start, SEEK_SET);
    if (feof(fp)) {
        fprintf(stderr, "Range given exceeds file length\n");
        return NULL;
    }
    int c = fgetc(fp); // int to handle EOF
    buf[0] = c;
    int num_chars = p2p_info->range_end - p2p_info->range_start - 1;
    int chars_read = 0;
    int index = p2p_info->range_start + 1;
    while (num_chars > 0) {
        // have we exceeded send buffer?
        if (chars_read == MAX_SEND_BUF) {
            printf("sent here\n");
            rc = send(p2p_info->sockfd, buf, MAX_SEND_BUF, 0);
            if (rc < 0) {
                fprintf(stderr, "send failed in p2pupload\n");
            }
            memset(buf, 0, MAX_SEND_BUF);
            chars_read = 0;
            continue;
        }
        // read a character in
        if ((c = fgetc(fp)) == EOF) {
            buf[index] = c;
            fprintf(stderr, "Warning: did not upload expected number of characters\n");
            fclose(fp);
            return NULL;
        }
        buf[index] = c;
        index++;
        num_chars--;
        chars_read++;
    }
    printf("uplaoding this: %s\n", buf);
    // send whatever's left in the buffer
    sleep(5);
    rc = send(p2p_info->sockfd, buf, strlen(buf), 0);
    printf("%d\n", rc);
    if (rc < 0) {
        perror("send()\n");
    }
    if (rc < 0) {
        fprintf(stderr, "send failed in p2pupload\n");
    }
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
    int out_conn;
    struct sockaddr_in servaddr;
    struct hostent *hostInfo;

    printf("Connect to ");
    // scanf("%s",hostname_buf);

    hostInfo = gethostbyname("moose.cs.dartmouth.edu");
    if(!hostInfo) {
        printf("host name error!\n");
        return -1;
    }

    servaddr.sin_family = hostInfo->h_addrtype;
    memcpy((char *) &servaddr.sin_addr.s_addr, hostInfo->h_addr_list[0], hostInfo->h_length);
    servaddr.sin_port = htons(2100);

    out_conn = socket(AF_INET, SOCK_STREAM, 0);
    if (out_conn < 0) {
        printf("socket error\n");
    }
    if (connect(out_conn, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
        perror("conect");


    pthread_t p2p_thread;

    P2PInfo p2p_info;
    p2p_info.sockfd = out_conn;
    p2p_info.piece_id = 1;
    p2p_info.range_start = 20;
    p2p_info.range_end = 541;
    p2p_info.size = 521;

    char *z = "test_file.txt";
    strncpy(p2p_info.filename, z, strlen(z));


    pthread_create(&p2p_thread, NULL, thread_P2PDownload, &p2p_info);
    pthread_join(p2p_thread);



}
