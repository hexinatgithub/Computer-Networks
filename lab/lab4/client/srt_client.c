#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "srt_client.h"
#include <errno.h>

int overlay_socket = 0;
client_tcb_t *tcb_table[MAX_TRANSPORT_CONNECTIONS];
pthread_mutex_t mutex;
pthread_cond_t cond;

// Send the seg SYN_MAX_RETRY times. If receive ack seg before timeout, then timer stop;
// Return 1 if success otherwise return -1;
int send_and_wait_ack(seg_t *seg, client_tcb_t *tcb, unsigned int expect_state);
// Use the client port and server port number to identify a tcb in tcb_table.
client_tcb_t *gettcb(int client_port, int server_port);
// Check socket invalid.
int check_sock(int sock_id);

/*interfaces to application layer*/

//
//
//  SRT socket API for the client side application.
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
//  GOAL: Your goal for this assignment is to design and implement the
//  protoypes below - fill the code.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// srt client initialization
//
// This function initializes the TCB table marking all entries NULL. It also initializes
// a global variable for the overlay TCP socket descriptor ‘‘conn’’ used as input parameter
// for snp_sendseg and snp_recvseg. Finally, the function starts the seghandler thread to
// handle the incoming segments. There is only one seghandler for the client side which
// handles call connections for the client.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void srt_client_init(int conn)
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

// Create a client tcb, return the sock
//
// This function looks up the client TCB table to find the first NULL entry, and creates
// a new TCB entry using malloc() for that entry. All fields in the TCB are initialized
// e.g., TCB state is set to CLOSED and the client port set to the function call parameter
// client port.  The TCB table entry index should be returned as the new socket ID to the client
// and be used to identify the connection on the client side. If no entry in the TC table
// is available the function returns -1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int srt_client_sock(unsigned int client_port)
{
  int i;

  for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)
  {
    if (tcb_table[i] == NULL)
    {
      tcb_table[i] = malloc(sizeof(client_tcb_t));
      tcb_table[i]->state = CLOSED;
      tcb_table[i]->client_portNum = client_port;
      return i;
    }
  }

  return -1;
}

// Connect to a srt server
//
// This function is used to connect to the server. It takes the socket ID and the
// server’s port number as input parameters. The socket ID is used to find the TCB entry.
// This function sets up the TCB’s server port number and a SYN segment to send to
// the server using snp_sendseg(). After the SYN segment is sent, a timer is started.
// If no SYNACK is received after SYNSEG_TIMEOUT timeout, then the SYN is
// retransmitted. If SYNACK is received, return 1. Otherwise, if the number of SYNs
// sent > SYN_MAX_RETRY,  transition to CLOSED state and return -1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int srt_client_connect(int sockfd, unsigned int server_port)
{
  if (check_sock(sockfd) == -1)
  {
    printf("Sockfd invalid\n");
    return -1;
  }

  client_tcb_t *tcb;
  seg_t seg;
  int i;

  tcb = tcb_table[sockfd];
  tcb->svr_portNum = server_port;
  tcb->state = SYNSENT;
  seg.header.src_port = tcb->client_portNum;
  seg.header.dest_port = tcb->svr_portNum;
  seg.header.type = SYN;
  seg.header.length = 0;

  i = send_and_wait_ack(&seg, tcb, CONNECTED);

  if (i == -1)
    tcb->state = CLOSED;

  return i;
}

// Send data to a srt server
//
// Send data to a srt server. You do not need to implement this for Lab4.
// We will use this in Lab5 when we implement a Go-Back-N sliding window.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int srt_client_send(int sockfd, void *data, unsigned int length)
{
  return 1;
}

// Disconnect from a srt server
//
// This function is used to disconnect from the server. It takes the socket ID as
// an input parameter. The socket ID is used to find the TCB entry in the TCB table.
// This function sends a FIN segment to the server. After the FIN segment is sent
// the state should transition to FINWAIT and a timer started. If the
// state == CLOSED after the timeout the FINACK was successfully received. Else,
// if after a number of retries FIN_MAX_RETRY the state is still FINWAIT then
// the state transitions to CLOSED and -1 is returned.

int srt_client_disconnect(int sockfd)
{
  if (check_sock(sockfd) == -1)
  {
    printf("Sockfd invalid\n");
    return -1;
  }

  client_tcb_t *tcb;
  seg_t seg;
  int i;

  tcb = tcb_table[sockfd];
  tcb->state = FINWAIT;
  seg.header.src_port = tcb->client_portNum;
  seg.header.dest_port = tcb->svr_portNum;
  seg.header.type = FIN;
  seg.header.length = 0;

  i = send_and_wait_ack(&seg, tcb, CLOSED);

  tcb->state = CLOSED;
  return i;
}

// Close srt client

// This function calls free() to free the TCB entry. It marks that entry in TCB as NULL
// and returns 1 if succeeded (i.e., was in the right state to complete a close) and -1
// if fails (i.e., in the wrong state).
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int srt_client_close(int sockfd)
{
  if (check_sock(sockfd) == -1)
  {
    printf("Sockfd invalid\n");
    return -1;
  }

  client_tcb_t *tcb;

  tcb = tcb_table[sockfd];

  if (tcb->state == CLOSED)
  {
    free(tcb);
    tcb_table[sockfd] = NULL;
    return 1;
  }

  return -1;
}

// The thread handles incoming segments
//
// This is a thread  started by srt_client_init(). It handles all the incoming
// segments from the server. The design of seghanlder is an infinite loop that calls snp_recvseg(). If
// snp_recvseg() fails then the overlay connection is closed and the thread is terminated. Depending
// on the state of the connection when a segment is received  (based on the incoming segment) various
// actions are taken. See the client FSM for more details.
//

void *seghandler(void *arg)
{
  int i;
  seg_t seg;
  client_tcb_t *tcb;

  while (snp_recvseg(overlay_socket, &seg) != -1)
  {
    tcb = gettcb(seg.header.dest_port, seg.header.src_port);
    if (tcb == NULL)
      continue;

    switch (seg.header.type)
    {
    case SYNACK:
      if (tcb->state == SYNSENT)
      {
        tcb->state = CONNECTED;
        pthread_cond_broadcast(&cond);
      }
      break;
    case FINACK:
      if (tcb->state == FINWAIT)
      {
        tcb->state = CLOSED;
        pthread_cond_broadcast(&cond);
      }
      break;
    default:
      break;
    }
  };

  return 0;
}

int send_and_wait_ack(seg_t *seg, client_tcb_t *tcb, unsigned int expect_state)
{
  int i, error;

  i = 1;
  while (i <= SYN_MAX_RETRY)
  {
    snp_sendseg(overlay_socket, seg);

    pthread_mutex_lock(&mutex);
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (ts.tv_nsec + SYNSEG_TIMEOUT_NS) / 1000000000;
    ts.tv_nsec = (ts.tv_nsec + SYNSEG_TIMEOUT_NS) % 1000000000;
    error = pthread_cond_timedwait(&cond, &mutex, &ts);
    pthread_mutex_unlock(&mutex);

    if (error == ETIMEDOUT)
    {
      i++;
      continue;
    }
    else if (error == 0 && tcb->state == expect_state)
    {
      return 1;
    }
    else
    {
      printf("Failed to call pthread_cond_timedwait(errno: %d)!\n", error);
      exit(0);
    }
  }
  return -1;
}

client_tcb_t *gettcb(int client_port, int server_port)
{
  int i;
  client_tcb_t *tcb;

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
  client_tcb_t *tcb;

  if (sock_id > MAX_TRANSPORT_CONNECTIONS || sock_id < 0)
  {
    return -1;
  }

  tcb = tcb_table[sock_id];

  return tcb ? 1 : -1;
}