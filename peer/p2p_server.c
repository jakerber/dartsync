//FILE: server/app_server.c

//Description: this is the server application code. The server first starts the overlay by creating a direct TCP link between the client and the server. Then it initializes the SRT server by calling srt_svr_init(). It creates 2 sockets and waits for connection from the client by calling srt_svr_sock() and srt_svr_connect() twice. Finally the server closes the socket by calling srt_server_close(). Overlay is stoped by calling overlay_end().

//Date: April 18,2008

//Input: None

//Output: SRT server states

// ---------------- Local includes
#include "peer.h"
#include "../common/constants.h"



#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>




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

printf("size: %d\n", fs);


    // char b[10000];

    // fread(b, 1469, 1, fp);
    // printf("%s\n", b);


    fseek(fp, p2p_info->range_start, SEEK_SET);
    if (feof(fp)) {
        fprintf(stderr, "Range given exceeds file length\n");
        return NULL;
    }


    int c = fgetc(fp); // int to handle EOF
    buf[0] = c;
    int num_chars = p2p_info->range_end - p2p_info->range_start - 1;
    printf("num_chars: %d\n", num_chars);
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
        buf[index] = (char) c;
        index++;
        num_chars--;
        chars_read++;
        printf("char: %d\n", chars_read);
        printf("%c\n", buf[index]);
    }
    printf("buf: %s\n", buf);
    printf("DONE\n");
    // send whatever's left in the buffer
    rc = send(p2p_info->sockfd, buf, strlen(buf), 0);
    printf("%d\n", rc);
    if (rc < 0) {
        perror("send()\n");
    }
    printf("sent!\n");
    if (rc < 0) {
        fprintf(stderr, "send failed in p2pupload\n");
    }
    return NULL;
}


//this function stops the overlay by closing the tcp connection between the server and the clien
void overlay_stop(int connection) {
    close(connection);
}

int main() {
    int rc;
    //start overlay and get the overlay TCP socket descriptor
    int tcpserv_sd;
    struct sockaddr_in tcpserv_addr;
    int connection;
    struct sockaddr_in tcpclient_addr;
    socklen_t tcpclient_addr_len;

    tcpserv_sd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpserv_sd < 0)
        printf("here\n");
    memset(&tcpserv_addr, 0, sizeof(tcpserv_addr));
    tcpserv_addr.sin_family = AF_INET;
    tcpserv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    tcpserv_addr.sin_port = htons(2100);

    // eliminate "Address already in use" error from bind.
    int enable = 1;


    if (setsockopt(tcpserv_sd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
    }
    if (bind(tcpserv_sd, (struct sockaddr *)&tcpserv_addr, sizeof(tcpserv_addr)) < 0) {
        printf("here\n");
        perror("");
        return -1;
    }
    if (listen(tcpserv_sd, 1) < 0) {

        printf("here\n");
        perror("");
        return -1;
    }
    printf("waiting for connection\n");

    connection = accept(tcpserv_sd, (struct sockaddr*)&tcpclient_addr, &tcpclient_addr_len);
    printf("conn: %d\n", connection);
    if (connection < 0) {
        printf("can not start server\n");
        return;
    }
    char *info_buf = calloc(sizeof(P2PInfo), sizeof(char));

    rc = recv(connection, info_buf, sizeof(P2PInfo), 0);
    if (rc == 0) {
        printf("no conn\n");
    }
    if (rc < 0) {
        perror("recv");
        printf("recv err\n");
    }
    P2PInfo *thing = (P2PInfo *) info_buf;
    printf("%d\n", thing->sockfd);
    printf("%d\n", thing->piece_id);
    printf("%d\n", thing->range_start);
    printf("%d\n", thing->range_end);
    printf("%s\n", thing->filename);

    thing->sockfd = connection;
    // prin
    pthread_t p2p_thread;
    pthread_create(&p2p_thread, NULL, thread_P2PUpload, thing);

    pthread_join(p2p_thread);

    printf("done with program\n");

    //stop the overlay
    overlay_stop(connection);
}
