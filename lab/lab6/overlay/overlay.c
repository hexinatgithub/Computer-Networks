//FILE: overlay/overlay.c
//
//Description: this file implements a ON process
//A ON process first connects to all the neighbors and then starts listen_to_neighbor threads each of which keeps receiving the incoming packets from a neighbor and forwarding the received packets to the SNP process. Then ON process waits for the connection from SNP process. After a SNP process is connected, the ON process keeps receiving sendpkt_arg_t structures from the SNP process and sending the received packets out to the overlay network.
//
//Date: April 28,2008

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <assert.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "overlay.h"
#include "../topology/topology.h"
#include "neighbortable.h"

//you should start the ON processes on all the overlay hosts within this period of time
#define OVERLAY_START_DELAY 60

/**************************************************************/
//declare global variables
/**************************************************************/

//declare the neighbor table as global variable
nbr_entry_t *nt;
int size;
//declare the TCP connection to SNP process as global variable
int network_conn;

/**************************************************************/
//implementation overlay functions
/**************************************************************/

// This thread opens a TCP port on CONNECTION_PORT and waits for the incoming connection from all the neighbors that have a larger node ID than my nodeID,
// After all the incoming connections are established, this thread terminates
void *waitNbrs(void *arg)
{
  int myID, ID, sfd, new_sfd;
  nbr_entry_t *entry;
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(struct sockaddr_in);

  if ((myID = topology_getMyNodeID()) == -1)
  {
    printf("topology_getMyNodeID failed!\n");
    return NULL;
  }

  if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    printf("create waiting socket failed!\n");
    return NULL;
  }

  memset(&addr, 0, sizeof(struct sockaddr_in));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(CONNECTION_PORT);

  if (bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1)
  {
    printf("bind address to socket failed!\n");
    close(sfd);
    return NULL;
  }

  if (listen(sfd, 10) < 0)
  {
    printf("Failed to listen on server socket.\n");
    close(sfd);
    return NULL;
  }

  for (int i = 0; i < size; i++)
  {
    entry = &nt[i];

    if (entry->nodeID > myID)
    {
      new_sfd = accept(sfd, (struct sockaddr *)&addr, &addrlen);
      ID = topology_getNodeIDfromip(&addr.sin_addr);

      printf("waitNbrs accept success %d %s\n", ID, inet_ntoa(addr.sin_addr));

      if (ID < myID)
      {
        printf("neighbor has ID less than my ID connect to me!\n");
        close(new_sfd);
        break;
      }

      nt_addconn(nt, ID, new_sfd);
    }
  }

  printf("waitNbrs thread exit!\n");
  close(sfd);
  return NULL;
}

// This function connects to all the neighbors that have a smaller node ID than my nodeID
// After all the outgoing connections are established, return 1, otherwise return -1
int connectNbrs()
{
  int sock, myID;
  nbr_entry_t *entry;
  struct sockaddr_in node_addr;

  if ((myID = topology_getMyNodeID()) == -1)
  {
    return -1;
  }

  for (int i = 0; i < size; i++)
  {
    entry = &nt[i];
    if (entry->nodeID < myID)
    {
      sock = socket(AF_INET, SOCK_STREAM, 0);

      if (sock == -1)
      {
        printf("create socket %d failed!\n", entry->nodeID);
        return -1;
      }

      entry->conn = sock;
      memset(&node_addr, 0, sizeof(node_addr));
      node_addr.sin_addr.s_addr = entry->nodeIP;
      node_addr.sin_family = AF_INET;
      node_addr.sin_port = htons(CONNECTION_PORT);

      if (connect(sock, (struct sockaddr *)&node_addr, sizeof(node_addr)) == -1)
      {
        printf("established connect to %d failed!\n", entry->nodeID);
        close(sock);
        return -1;
      }
    }
  }
  return 1;
}

//Each listen_to_neighbor thread keeps receiving packets from a neighbor. It handles the received packets by forwarding the packets to the SNP process.
//all listen_to_neighbor threads are started after all the TCP connections to the neighbors are established
void *listen_to_neighbor(void *arg)
{
  int i = *(int *)arg;
  snp_pkt_t pkt;

  while (recvpkt(&pkt, nt[i].conn) == 1)
    forwardpktToSNP(&pkt, network_conn);

  close(nt[i].conn);
  nt[i].conn = -1;
  printf("listen_to_neighbor %d thread exit\n", i);
  return NULL;
}

//This function opens a TCP port on OVERLAY_PORT, and waits for the incoming connection from local SNP process. After the local SNP process is connected, this function keeps getting sendpkt_arg_ts from SNP process, and sends the packets to the next hop in the overlay network. If the next hop's nodeID is BROADCAST_NODEID, the packet should be sent to all the neighboring nodes.
void waitNetwork()
{
  int sfd, nbNodeID;
  snp_pkt_t pkt;
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(struct sockaddr_in);

  sfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sfd == -1)
  {
    printf("waitNetwork create socket failed!\n");
    exit(-1);
  }

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(OVERLAY_PORT);

  if (bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1)
  {
    printf("bind address to socket failed!\n");
    close(sfd);
    return;
  }

  if (listen(sfd, 10) < 0)
  {
    printf("Failed to listen on server socket.\n");
    close(sfd);
    return;
  }

  if ((network_conn = accept(sfd, (struct sockaddr *)&addr, &addrlen)) < 0)
  {
    printf("Overlay: accept failed\n");
    close(sfd);
    return;
  }

  close(sfd);

  printf("Overlay: accept connection from SNP process...\n");

  while (getpktToSend(&pkt, &nbNodeID, network_conn) == 1)
  {
    printf("Overlay: receive packet from SNP to send!\n");
    for (int i = 0; i < size; i++)
    {
      if (nt[i].nodeID == nbNodeID || nbNodeID == BROADCAST_NODEID)
        sendpkt(&pkt, nt[i].conn);
    }
  }
}

//this function stops the overlay
//it closes all the connections and frees all the dynamically allocated memory
//it is called when receiving a signal SIGINT
void overlay_stop()
{
  nt_destroy(nt);
  close(network_conn);
  exit(1);
}

int main()
{
  //start overlay initialization
  printf("Overlay: Node %d initializing...\n", topology_getMyNodeID());

  //create a neighbor table
  nt = nt_create();
  //initialize network_conn to -1, means no SNP process is connected yet
  network_conn = -1;

  //register a signal handler which is sued to terminate the process
  signal(SIGINT, overlay_stop);

  //print out all the neighbors
  int nbrNum = topology_getNbrNum();
  size = nbrNum;
  int i;
  for (i = 0; i < nbrNum; i++)
  {
    printf("Overlay: neighbor %d:%d\n", i + 1, nt[i].nodeID);
  }

  //start the waitNbrs thread to wait for incoming connections from neighbors with larger node IDs
  pthread_t waitNbrs_thread;
  pthread_create(&waitNbrs_thread, NULL, waitNbrs, (void *)0);

  //wait for other nodes to start
  sleep(5);

  //connect to neighbors with smaller node IDs
  connectNbrs();

  //wait for waitNbrs thread to return
  pthread_join(waitNbrs_thread, NULL);

  //at this point, all connections to the neighbors are created

  //create threads listening to all the neighbors
  for (i = 0; i < nbrNum; i++)
  {
    int *idx = (int *)malloc(sizeof(int));
    *idx = i;
    pthread_t nbr_listen_thread;
    pthread_create(&nbr_listen_thread, NULL, listen_to_neighbor, (void *)idx);
  }
  printf("Overlay: node initialized...\n");
  printf("Overlay: waiting for connection from SNP process...\n");

  //waiting for connection from  SNP process
  waitNetwork();
}
