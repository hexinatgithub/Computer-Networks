/*
 * FILE: file_browser.c 
 *
 * Description: A simple, iterative HTTP/1.0 Web server that uses the
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
#include <unistd.h>
#include <string.h>

#define LISTENQ  1024  // second argument to listen()
#define MAXLINE 1024   // max length of a line
#define RIO_BUFSIZE 1024
#define true 1
#define false 0
#define ERROR  -1
#define OK 1
 #define MAX_CLIENTS 9999

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
    int browser_index;
    off_t offset;              // for support Range
    size_t end;
} http_request;

typedef struct {
    const char *extension;
    const char *mime_type;
} mime_map;

mime_map meme_types [] = {
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

// declaring variables for server socket information
int server_port_number;
struct sockaddr_in serverAddress;
int serverSocketFd;

// working directory
char *workingDirectory;

void showErrorAndExit(char *msg){
    perror(msg);
    exit(0);
}
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

    while (nleft > 0){
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
void handle_directory_request(int out_fd, int dir_fd, char *filename){
    
    // send response headers to client e.g., "HTTP/1.1 200 OK\r\n"
    
    // get file directory
    
    // read directory
    
    // send the file buffers to the client
    
    // send recent browser data to the client
}

// utility function to get the MIME (Multipurpose Internet Mail Extensions) type
static const char* get_mime_type(char *filename){
    char *dot = strrchr(filename, '.');
    if(dot){ // strrchar Locate last occurrence of character in string
        mime_map *map = meme_types;
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
    server_port_number = port;
    serverSocketFd = socket(AF_INET, SOCK_STREAM, 0);
    bzero((char *) &serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(server_port_number);
    if (bind(serverSocketFd, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0){
        char buffer[256];
        sprintf(buffer,"Error in binding to port %d \n",port); 
        showErrorAndExit(buffer);
    }
    listen(serverSocketFd,MAX_CLIENTS);
    printf("Started listening at port %d for http requests \n",server_port_number);

    // create a socket descriptor

    // eliminate "Address already in use" error from bind.

    // 6 is TCP's protocol number
    // enable this, much faster : 4000 req/s -> 17000 req/s

    // Listenfd will be an endpoint for all requests to port
    // on any IP address for this host

    // make it a listening socket ready to accept connection requests
}

// decode url
void url_decode(char* src, char* dest, int max) {

}

// parse request to get url
void parse_request(int fd, http_request *req){

    // Rio (Robust I/O) Buffered Input Functions
    
    
    // read all
    
    // update recent browser data
    
    // decode url
}

// log files
void log_access(int status, struct sockaddr_in *c_addr, http_request *req){

}

// echo client error e.g. 404
void client_error(int fd, int status, char *msg, char *longmsg){

}

// serve static content
void serve_static(int out_fd, int in_fd, http_request *req,
                  size_t total_size){
    
    // send response headers to client e.g., "HTTP/1.1 200 OK\r\n"
    
    // send response body to client
}

// handle one HTTP request/response transaction
void process(int fd, struct sockaddr_in *clientaddr){
    printf("accept request, fd is %d, pid is %d\n", fd, getpid());
    http_request req;
    parse_request(fd, &req);

    struct stat sbuf;
    int status = 200; //server status init as 200
    int ffd = open(req.filename, O_RDONLY, 0);
    if(ffd <= 0){
        // detect 404 error and print error log
        
    } else {
        // get descriptor status
        fstat(ffd, &sbuf);
        if(S_ISREG(sbuf.st_mode)){
            // server serves static content
            
        } else if(S_ISDIR(sbuf.st_mode)){
            // server handle directory request

        } else {
            // detect 400 error and print error log

        }
        close(ffd);
    }
    
    // print log/status on the terminal
    log_access(status, clientaddr, &req);
}

int checkAndUpdateUserInput(int argc, char** argv){
    int result = ERROR;
    int default_port = 9999;
    if(argc == 1){
        showErrorAndExit("Working directory is required and port number is optional\n");
        return result;
    }        
    if(argc == 2){
        printf("Setting default port number to %d\n",default_port);
        workingDirectory = argv[1];
        server_port_number = default_port;
        result = OK;
        return result;
    }
    if(argc == 3){
        workingDirectory = argv[1];
        server_port_number = atoi(argv[2]);
        result = OK;
        return result;
    }
}
// main function:
// get the user input for the file directory and port number
int main(int argc, char** argv){
    struct sockaddr_in clientAddress;
    int client = sizeof(clientAddress);
    int clientSocketFd;
    int clientPID;
    int listenfd, connfd;
    char buf[256];
    checkAndUpdateUserInput(argc,argv);
    printf("Server working directory is %s\n", workingDirectory );
    printf("Server listening port is %d\n", server_port_number );
    // get the name of the current working directory
    // user input checking

    
    // ignore SIGPIPE signal, so if browser cancels the request, it
    // won't kill the whole process.
    signal(SIGPIPE, SIG_IGN);
    open_listenfd(server_port_number);
    while(1){
        // permit an incoming connection attempt on a socket.
        clientSocketFd = accept(serverSocketFd, (struct sockaddr *) &clientAddress, &client);
        if(clientSocketFd >= 0){
            // fork children to handle parallel clients
            // handle one HTTP request/response transaction
            clientPID = fork();
            if (clientPID < 0){
                char buffer[256];
                sprintf(buffer,"Error in binding to port %d \n",clientSocketFd); 
                showErrorAndExit(buffer);
            }
            if (clientPID == 0)  {
                close(serverSocketFd);
                process(clientSocketFd,&clientAddress);
                exit(0);
            }
            else {
               close(clientSocketFd);
            }
        }else{
            printf("Error in accepting client \n");
        }
  
    }

    return 0;
}