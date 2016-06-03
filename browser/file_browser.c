/*
 * FILE: file_browser.c 
 *
 * Description: A simple, iterative HTTP/1.1 Web server that uses the
 * GET method to serve static and dynamic content.
 *
 * Date: April 4, 2016
 */

#include <arpa/inet.h>          // inet_ntoa
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

#define LISTENQ  1024  // second argument to listen()
#define MAXLINE 1024   // max length of a line
#define RIO_BUFSIZE 1024

typedef struct {
    int rio_fd;                 // descriptor for this buf
    int rio_cnt;                // unread byte in this buf
    char *rio_bufptr;           // next unread byte in this buf
    char rio_buf[RIO_BUFSIZE];  // internal buffer
} rio_t;

// simplifies calls to bind(), connect(), and accept()
typedef struct sockaddr SA;

typedef struct {
    char filename[512];
    char browser[100];
    off_t offset;              // for support Range
    size_t end;
} http_request;

typedef struct {
    const char *extension;
    const char *mime_type;
} mime_map;

mime_map mime_types [] = {
    {".css", "text/css"},
    {".gif", "image/gif"},
    {".htm", "text/html"},
    {".html", "text/html"},
    {".jpeg", "image/jpeg"},
    {".jpg", "image/jpeg"},
    {".ico", "image/x-icon"},
    {".js", "application/javascript"},
    {".pdf", "application/pdf"},
    {".mp4", "video/mp4"},
    {".png", "image/png"},
    {".svg", "image/svg+xml"},
    {".xml", "text/xml"},
    {NULL, NULL},
};

char *default_mime_type = "text/plain";

// set up an empty read buffer and associates an open file descriptor with that buffer
void rio_readinitb(rio_t *rp, int fd){
    rp->rio_fd = fd;
    rp->rio_cnt = 0;
    rp->rio_bufptr = rp->rio_buf;
}

// utility function for writing user buffer into a file descriptor
ssize_t written(int fd, void *usrbuf, size_t n){
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = usrbuf;

    while (nleft > 0) {
        if ((nwritten = write(fd, bufp, nleft)) <= 0){
            if (errno == EINTR)  // interrupted by sig handler return
                nwritten = 0;    // and call write() again
            else
                return -1;       // errorno set by write()
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
    return n;
}


/*
 *    This is a wrapper for the Unix read() function that
 *    transfers min(n, rio_cnt) bytes from an internal buffer to a user
 *    buffer, where n is the number of bytes requested by the user and
 *    rio_cnt is the number of unread bytes in the internal buffer. On
 *    entry, rio_read() refills the internal buffer via a call to
 *    read() if the internal buffer is empty.
 */
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n){
    int cnt;
    while (rp->rio_cnt <= 0){  // refill if buf is empty
        
        rp->rio_cnt = read(rp->rio_fd, rp->rio_buf,
                           sizeof(rp->rio_buf));
        if (rp->rio_cnt < 0){
            if (errno != EINTR) // interrupted by sig handler return
                return -1;
        }
        else if (rp->rio_cnt == 0)  // EOF
            return 0;
        else
            rp->rio_bufptr = rp->rio_buf; // reset buffer ptr
    }
    
    // copy min(n, rp->rio_cnt) bytes from internal buf to user buf
    cnt = n;
    if (rp->rio_cnt < n)
        cnt = rp->rio_cnt;
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}

// robustly read a text line (buffered)
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen){
    int n, rc;
    char c, *bufp = usrbuf;
    
    for (n = 1; n < maxlen; n++){
        if ((rc = rio_read(rp, &c, 1)) == 1){
            *bufp++ = c;
            if (c == '\n')
                break;
        } else if (rc == 0){
            if (n == 1)
                return 0; // EOF, no data read
            else
                break;    // EOF, some data was read
        } else
            return -1;    // error
    }
    *bufp = 0;
    return n;
}

// utility function to get the format size
void format_size(char* buf, struct stat *stat){
    if(S_ISDIR(stat->st_mode)){
        sprintf(buf, "%s", "[DIR]");
    } else {
        off_t size = stat->st_size;
        if(size < 1024){
            sprintf(buf, "%lu", size);
        } else if (size < 1024 * 1024){
            sprintf(buf, "%.1fK", (double)size / 1024);
        } else if (size < 1024 * 1024 * 1024){
            sprintf(buf, "%.1fM", (double)size / 1024 / 1024);
        } else {
            sprintf(buf, "%.1fG", (double)size / 1024 / 1024 / 1024);
        }
    }
}

// pre-process files in the "home" directory and send the list to the client
void handle_directory_request(int out_fd, int dir_fd, char *dirname){
    struct dirent *ent;
    struct stat sbuf;
    char buf[MAXLINE], filename[256], file_prefix[256], new_prefix[256];
    char *token, *dirname_copy;
    int in, foundsl, npi, newPut, foundMax;
	DIR *dir;
    npi = 0;
    in = 0;
    foundsl = 0;
    newPut = 0;
    foundMax = 0;

    for (int i = 0; i < 256; i++) {
        filename[i] = 0;
        file_prefix[i] = 0;
        new_prefix[i] = 0;
    }
	
    // send response headers to client e.g., "HTTP/1.1 200 OK\r\n"
    written(out_fd, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n", 100);

    // send html response body to client
    written(out_fd, "<html><head><style>body{font-family: monospace; font-size: 13px;}td {padding: 1.5px 6px;}</style></head><body><table>\n", 117);

    // get file directory name to prepend to filenames
    dirname_copy = calloc(strlen(dirname), sizeof(char));
    strcpy(dirname_copy, dirname);
    token = strtok(dirname_copy, "/");
    token = strtok(NULL, "/");
    while (token != NULL) {
        in = 1;
        strcat(file_prefix, token);
        strcat(file_prefix, "/");
        token = strtok(NULL, "/");
    }
    if (in == 0 && token == NULL)
        strcpy(file_prefix, "");
    //check for double slash in prefix
    for (int i = 0; i < strlen(file_prefix); i++)
        if (file_prefix[i] == '/')
            foundsl++;
    foundMax = foundsl;
    if (foundsl > 1) {
        foundsl = 0;
        for (int i = 0; i < strlen(file_prefix); i++) {
            //start recording prefix
            if (foundsl == foundMax - 1) {
                new_prefix[npi] = file_prefix[i];
                npi++;
            }
            if (file_prefix[i] == '/')
                foundsl++;
        }
        newPut = 1;
    }
    // open directory
    dir = opendir(dirname);
	while ((ent = readdir(dir)) != NULL) {
        // send filenames, dates last modified, and filesizes to client in html
		if ((strcmp(ent->d_name, ".") != 0) && (strcmp(ent->d_name, "..") != 0)) {
			sprintf(filename, "%s/%s", dirname, ent->d_name);
			stat(filename, &sbuf);
			if (newPut == 0) {
                sprintf(buf, "<tr><td><a href=\"%s%s\">%s</a></td><td>%s</td><td>%ld</td></tr>\n", file_prefix, ent->d_name, ent->d_name, ctime(&sbuf.st_mtime), sbuf.st_size);
            } else {
                sprintf(buf, "<tr><td><a href=\"%s%s\">%s</a></td><td>%s</td><td>%ld</td></tr>\n", new_prefix, ent->d_name, ent->d_name, ctime(&sbuf.st_mtime), sbuf.st_size);
            }
            written(out_fd, buf, strlen(buf));
		}
	}
	written(out_fd, "</tr></table>\n", 15);

    free(dirname_copy);
    closedir(dir);
}

// utility function to get the MIME (Multipurpose Internet Mail Extensions) type
static const char* get_mime_type(char *filename){
    char *dot = strrchr(filename, '.');
    if(dot){ // strrchar Locate last occurrence of character in string
        mime_map *map = mime_types;
        while(map->extension){
            if(strcmp(map->extension, dot) == 0){
                return map->mime_type;
            }
            map++;
        }
    }
    return default_mime_type;
}

// open a listening socket descriptor using the specified port number.
int open_listenfd(int port){
    int listenfd;
    struct sockaddr_in servaddr;

    // create a socket descriptor
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 6)) < 0) {
        perror("Socket could not be created");
        exit(1);
    }

    //set up server address
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    // bind address to socket
    bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));

    // make it a listening socket ready to accept connection requests
    listen(listenfd, LISTENQ);

    return listenfd;
}

// decode url in case of spaces in the filename
void url_decode(char* src, char* dest) {
	char *token;
	int i = 0;

	token = strtok(src, "%%20");
	while (token != NULL) {
		if (i > 0) {
			strcat(dest, " ");
		}
		strcat(dest, token);
		token = strtok(NULL, "%%20");
		i++;
	}
}

// parse request to get url, IP address, browser type
void parse_request(int fd, http_request *req, struct sockaddr_in *clientaddr){
    int n;
    rio_t rp;
    char buf[MAXLINE], filename[100];
    char *token;

    rio_readinitb(&rp, fd);
    while ((n = rio_readlineb(&rp, buf, MAXLINE)) != 0) {
        if (strstr(buf, "HTTP") != NULL) {
            // only supports GET method
        	if (strstr(buf, "GET") == NULL) {
        		write(fd, "Invalid method detected. Only GET is supported", 50);
        		close(fd);
                exit(0);
        	}
            // parse filename from url
        	strtok(buf, " ");
        	token = strtok(NULL, " ");
        	//url_decode(token, filename);
            strcpy(filename, token);
        	strcat(req->filename, filename);
        }
        // detect browser being used
        else if (strstr(buf, "User-Agent") != NULL) {
        	if (strstr(buf, "Chrome") != NULL) {
        		strcpy(req->browser, "Chrome");
        	}
        	else if (strstr(buf, "Firefox") != NULL) {
        		strcpy(req->browser, "Firefox");
        	}
        	else if (strstr(buf, "Safari") != NULL) {
        		strcpy(req->browser, "Safari");
        	}
        	else {
        		strcpy(req->browser, "Other");
        	}
        }
        else if (strcmp(buf, "\r\n") == 0) {
        	break;
        }
    }
}

// log files
void log_access(int status, struct sockaddr_in *c_addr, http_request *req) {
	char *ip_addr;
	FILE *fp;
	FILE *tempfp;
	char buf[256];

	ip_addr = inet_ntoa(c_addr->sin_addr);
	printf("BROWSER: Client IP: %s\n", ip_addr);

    // maintain a log file with latest access on top and oldest on bottom
	if ((fp = fopen("log", "r")) != NULL) {
       /* if there is a log file existing, then make a new temp file, add the latest access to it, 
        append the existing log file to it, and then make temp the log file*/
		tempfp = fopen("temp", "a");
		fprintf(tempfp, "%s %s\n", ip_addr, req->browser);
		while (!feof(fp)) {
			if (fgets(buf, 100, fp) != NULL) {
				fprintf(tempfp, buf);
			}
		}
		remove("log");
		rename("temp", "log");
		fclose(fp);
		fclose(tempfp);
	}
	else {
        // if there is no log file existing, then make a new one
		fp = fopen("log", "a");
		fprintf(fp, "%s %s\n", ip_addr, req->browser);
		fclose(fp);
	}
}

// echo client error e.g. 404
void client_error(int fd, int status, char *msg, char *longmsg) {
	char response[100];

	sprintf(response, "HTTP/1.1 %d %s\r\n\r\n%s", status, msg, longmsg);
	written(fd, response, strlen(response));
}

// serve static content
void serve_static(int out_fd, int in_fd, http_request *req, size_t total_size) {
	char mime_type[50], header[100];
	strcpy(mime_type, get_mime_type(req->filename));
    sprintf(header, "HTTP/1.1 200 OK\r\nAccept-Ranges: bytes\r\nCache-Control: no-cache\r\nContent-type: %s\r\nContent-length: %zu\r\n\r\n", 
        mime_type, total_size);
    // send response headers to client e.g., "HTTP/1.1 200 OK\r\n"
    written(out_fd, header, strlen(header));
    // send response body to client
    sendfile(out_fd, in_fd, NULL, total_size);
}

// handle one HTTP request/response transaction
void process(int fd, struct sockaddr_in *clientaddr, char *dir) {
	http_request req;
    //printf("accept request, fd is %d, pid is %d\n", fd, getpid());
    strcpy(req.filename, dir);
    parse_request(fd, &req, clientaddr);
    struct stat sbuf;
    int status = 200; //server status init as 200
    int ffd = open(req.filename, O_RDONLY, 0);
    if(ffd <= 0){
    	// detect 404 error and print error log
        status = 404;
    	client_error(fd, status, "Not found", "File not found");
    } else {
        // get descriptor status
        fstat(ffd, &sbuf);
        if(S_ISREG(sbuf.st_mode)){
            // server serves static content
            serve_static(fd, ffd, &req, sbuf.st_size);
            
        } else if(S_ISDIR(sbuf.st_mode)){
            // server handle directory request
        	handle_directory_request(fd, ffd, req.filename);

        } else {
            status = 400;
            client_error(fd, status, "Error", "There was an error");
        }
        close(ffd);
    }
    
    // print log/status on the terminal
    log_access(status, clientaddr, &req);
}

// main function:
// get the user input for the file directory and port number
int main(int argc, char** argv){
    struct sockaddr_in clientaddr;
    int port,
        listenfd,
        connfd;
    socklen_t clientlen;
    pid_t pid;
    struct stat s;
    
    // user input checking 
    if (argc != 3) {
        perror("Invalid number of inputs, please enter file directory and port number");
        exit(1);
    }

    stat(argv[1], &s);
    if (!S_ISDIR(s.st_mode)) {
    	printf("Invalid input: No such directory '%s'\n", argv[1]);
    	exit(1);
    }

    port = atoi(argv[2]);
    if (port > 65535) {
        perror("Port number too high");
        exit(1);
    }

    printf("Please visit\n");
    system("hostname -I");
    printf("%i\n", port);

    // ignore SIGPIPE signal, so if browser cancels the request, it
    // won't kill the whole process.
    signal(SIGPIPE, SIG_IGN);
    listenfd = open_listenfd(port);


    //reap child zombie processes
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
        perror(0);
        exit(1);
    }

    while(1){
    	clientlen = sizeof(clientaddr);
    	connfd = accept(listenfd, (struct sockaddr *) &clientaddr, &clientlen);

    	if ((pid = fork()) == 0) {
    		close(listenfd);
    		process(connfd, &clientaddr, argv[1]);
    		exit(0);
    	}
    	close(connfd);
    }

    return 0;
}