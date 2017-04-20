#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include "srt_server.h"
#include "../common/constants.h"

int overlay_socket = 0;
svr_tcb_t *tcb_table[MAX_TRANSPORT_CONNECTIONS];
pthread_mutex_t mutex;
pthread_cond_t cond;

// Use the client port and server port number to identify a tcb in tcb_table.
svr_tcb_t *gettcb1(int server_port);
svr_tcb_t *gettcb2(int client_port, int server_port);
// Check socket invalid.
int check_sock(int sock_id);
// send connect ack segment.
int send_connect_ack(svr_tcb_t *tcb, unsigned short int seg_type);
// Create a close wait timeout timer.
void close_wait_timeout(svr_tcb_t *tcb);
void *timer_handler(void *args);

/*interfaces to application layer*/

//
//
//  SRT socket API for the server side application.
//  ===================================
//
//  In what follows, we provide the prototype definition for each call and limited pseudo code representation
//  of the function. This is not meant to be comprehensive - more a guideline.
//
//  You are free to design the code as you wish.
//
//  NOTE: When designing all functions you should consider all possible states of the FSM using
//  a switch statement (see the Lab4 assignment for an example). Typically, the FSM has to be
// in a certain state determined by the design of the FSM to carry out a certain action.
//
//  GOAL: Your job is to design and implement the prototypes below - fill in the code.
//

// srt server initialization
//
// This function initializes the TCB table marking all entries NULL. It also initializes
// a global variable for the overlay TCP socket descriptor ‘‘conn’’ used as input parameter
// for snp_sendseg and snp_recvseg. Finally, the function starts the seghandler thread to
// handle the incoming segments. There is only one seghandler for the server side which
// handles call connections for the client.
//

void srt_server_init(int conn)
{
  int i;
  pthread_t thread_id;

  for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)
  {
    tcb_table[i] = NULL;
  }

  overlay_socket = conn;

  i = pthread_create(&thread_id, NULL, seghandler, NULL);

  if (i != 0)
  {
    printf("Fail to create the seghandler thread!\n");
    exit(0);
  }
  return;
}

// Create a server sock
//
// This function looks up the client TCB table to find the first NULL entry, and creates
// a new TCB entry using malloc() for that entry. All fields in the TCB are initialized
// e.g., TCB state is set to CLOSED and the server port set to the function call parameter
// server port.  The TCB table entry index should be returned as the new socket ID to the server
// and be used to identify the connection on the server side. If no entry in the TCB table
// is available the function returns -1.

int srt_server_sock(unsigned int port)
{
  int i;

  for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)
  {
    if (tcb_table[i] == NULL)
    {
      tcb_table[i] = malloc(sizeof(svr_tcb_t));
      tcb_table[i]->state = CLOSED;
      tcb_table[i]->svr_portNum = port;
      return i;
    }
  }

  return -1;
}

// Accept connection from srt client
//
// This function gets the TCB pointer using the sockfd and changes the state of the connetion to
// LISTENING. It then starts a timer to ‘‘busy wait’’ until the TCB’s state changes to CONNECTED
// (seghandler does this when a SYN is received). It waits in an infinite loop for the state
// transition before proceeding and to return 1 when the state change happens, dropping out of
// the busy wait loop. You can implement this blocking wait in different ways, if you wish.
//

int srt_server_accept(int sockfd)
{
  if (check_sock(sockfd) == -1)
  {
    printf("Sockfd invalid\n");
    return -1;
  }

  svr_tcb_t *tcb;
  int error;

  tcb = tcb_table[sockfd];
  tcb->state = LISTENING;

  while (1)
  {
    pthread_mutex_lock(&mutex);
    error = pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);

    if (tcb->state == CONNECTED)
      break;
  };

  return 1;
}

// Receive data from a srt client
//
// Receive data to a srt client. Recall this is a unidirectional transport
// where DATA flows from the client to the server. Signaling/control messages
// such as SYN, SYNACK, etc.flow in both directions. You do not need to implement
// this for Lab4. We will use this in Lab5 when we implement a Go-Back-N sliding window.
//
int srt_server_recv(int sockfd, void *buf, unsigned int length)
{
  return 1;
}

// Close the srt server
//
// This function calls free() to free the TCB entry. It marks that entry in TCB as NULL
// and returns 1 if succeeded (i.e., was in the right state to complete a close) and -1
// if fails (i.e., in the wrong state).
//

int srt_server_close(int sockfd)
{
  if (check_sock(sockfd) == -1)
  {
    printf("Sockfd invalid\n");
    return -1;
  }

  svr_tcb_t *tcb;

  tcb = tcb_table[sockfd];

  if (tcb->state == CLOSED)
  {
    free(tcb);
    tcb_table[sockfd] = NULL;
    return 1;
  }

  return -1;
}

// Thread handles incoming segments
//
// This is a thread  started by srt_server_init(). It handles all the incoming
// segments from the client. The design of seghanlder is an infinite loop that calls snp_recvseg(). If
// snp_recvseg() fails then the overlay connection is closed and the thread is terminated. Depending
// on the state of the connection when a segment is received  (based on the incoming segment) various
// actions are taken. See the client FSM for more details.
//

void *seghandler(void *arg)
{
  int i;
  seg_t seg;
  svr_tcb_t *tcb;

  while (snp_recvseg(overlay_socket, &seg) != -1)
  {
    // printf("seghandler dest:%d src:%d\n", seg.header.dest_port, seg.header.src_port);
    tcb = gettcb2(seg.header.src_port, seg.header.dest_port);

    switch (seg.header.type)
    {
    case SYN:
      if (tcb == NULL)
        tcb = gettcb1(seg.header.dest_port);

      if (tcb == NULL)
        continue;

      if (tcb->state == LISTENING)
      {
        tcb->client_portNum = seg.header.src_port;
        tcb->state = CONNECTED;
        pthread_cond_broadcast(&cond);
        send_connect_ack(tcb, SYNACK);
      }
      else if (tcb->state == CONNECTED)
      {
        send_connect_ack(tcb, SYNACK);
      }
      break;
    case FIN:
      if (tcb == NULL)
        continue;

      if (tcb->state == CONNECTED)
      {
        tcb->state = CLOSEWAIT;
        send_connect_ack(tcb, FINACK);
        close_wait_timeout(tcb);
      }
      else if (tcb->state == CLOSEWAIT)
      {
        send_connect_ack(tcb, FINACK);
      }
      break;
    default:
      break;
    }
  };

  return 0;
}

int send_connect_ack(svr_tcb_t *tcb, unsigned short int seg_type)
{
  seg_t seg;

  seg.header.src_port = tcb->svr_portNum;
  seg.header.dest_port = tcb->client_portNum;
  seg.header.type = seg_type;
  seg.header.length = 0;

  return snp_sendseg(overlay_socket, &seg);
}

svr_tcb_t *gettcb1(int server_port)
{
  int i;
  svr_tcb_t *tcb;

  for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)
  {
    tcb = tcb_table[i];
    if (tcb == NULL)
      continue;

    if (tcb->svr_portNum == server_port)
    {
      return tcb;
    }
  }

  return NULL;
}

svr_tcb_t *gettcb2(int client_port, int server_port)
{
  int i;
  svr_tcb_t *tcb;

  for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)
  {
    tcb = tcb_table[i];
    if (tcb == NULL)
      continue;

    if (tcb->client_portNum == client_port && tcb->svr_portNum == server_port)
    {
      return tcb;
    }
  }

  return NULL;
}

int check_sock(int sock_id)
{
  svr_tcb_t *tcb;

  if (sock_id > MAX_TRANSPORT_CONNECTIONS || sock_id < 0)
  {
    return -1;
  }

  tcb = tcb_table[sock_id];

  return tcb ? 1 : -1;
}

void *timer_handler(void *args)
{
  sleep(CLOSEWAIT_TIME);
  svr_tcb_t *tcb = (svr_tcb_t *)args;
  tcb->state = CLOSED;

  return NULL;
}

void close_wait_timeout(svr_tcb_t *tcb)
{
  pthread_t pthread_id;
  int erron;

  erron = pthread_create(&pthread_id, NULL, timer_handler, (void *)tcb);

  if (erron != 0)
  {
    printf("close_wait_timeout create pthread failed\n");
    exit(0);
  }
}
