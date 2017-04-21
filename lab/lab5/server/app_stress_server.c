//FILE: server/app_stress_server.c

//Description: this is the stress server application code. The server first starts the overlay by creating a direct TCP link between the client and the server. Then it initializes the SRT server by calling srt_svr_init(). It creates a sockets and waits for connection from the client by calling srt_svr_sock() and srt_svr_connect(). It then receives the length of the file to be received. After that, it creates a buffer, receives the file data and saves the file data to receivedtext.txt file. Finally the server closes the socket by calling srt_server_close(). Overlay is stopped by calling overlay_end().

//Date: April 26,2008

//Input: None

//Output: SRT server states

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

#include "../common/constants.h"
#include "srt_server.h"

//One SRT connection is created using client port CLIENTPORT1 and server port SVRPORT1.
#define CLIENTPORT1 87
#define SVRPORT1 88
//after the received file data is saved, the server waits WAITTIME seconds, and then closes the connection
#define WAITTIME 10

//this function starts the overlay by creating a direct TCP connection between the client and the server. The TCP socket descriptor is returned. If the TCP connection fails, return -1. The TCP socket desciptor returned will be used by SRT to send segments.
int overlay_start()
{
  int tcpserv_sd;
  struct sockaddr_in tcpserv_addr;
  int connection;
  struct sockaddr_in tcpclient_addr;
  socklen_t tcpclient_addr_len;

  tcpserv_sd = socket(AF_INET, SOCK_STREAM, 0);
  if (tcpserv_sd < 0)
    return -1;
  memset(&tcpserv_addr, 0, sizeof(tcpserv_addr));
  tcpserv_addr.sin_family = AF_INET;
  tcpserv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  tcpserv_addr.sin_port = htons(OVERLAY_PORT);

  if (bind(tcpserv_sd, (struct sockaddr *)&tcpserv_addr, sizeof(tcpserv_addr)) < 0)
    return -1;
  if (listen(tcpserv_sd, 1) < 0)
    return -1;
  printf("waiting for connection\n");
  connection = accept(tcpserv_sd, (struct sockaddr *)&tcpclient_addr, &tcpclient_addr_len);
  return connection;
}

//this function stops the overlay by closing the TCP connection between the server and the client
void overlay_stop(int connection)
{
  close(connection);
}

int main()
{
  //random seed for segment loss
  srand(time(NULL));

  //start overlay and get the overlay TCP socket descriptor
  int overlay_conn = overlay_start();
  if (overlay_conn < 0)
  {
    printf("can not start overlay\n");
  }

  //initialize srt server
  srt_server_init(overlay_conn);

  //create a srt server sock at port SVRPORT1
  int sockfd = srt_server_sock(SVRPORT1);
  if (sockfd < 0)
  {
    printf("can't create srt server\n");
    exit(1);
  }
  //listen and accept connection from a srt client
  srt_server_accept(sockfd);

  //receive the file size first
  //and then receive the file data
  int fileLen;
  srt_server_recv(sockfd, &fileLen, sizeof(int));
  char *buf = (char *)malloc(fileLen);
  srt_server_recv(sockfd, buf, fileLen);

  //save the received file data in receivedtext.txt
  FILE *f;
  f = fopen("receivedtext.txt", "w+");
  fwrite(buf, fileLen, 1, f);
  fclose(f);
  free(buf);

  //wait for a while
  sleep(WAITTIME);

  //close srt server
  if (srt_server_close(sockfd) < 0)
  {
    printf("can't destroy srt server\n");
    exit(1);
  }

  //stop the overlay
  overlay_stop(overlay_conn);
}
