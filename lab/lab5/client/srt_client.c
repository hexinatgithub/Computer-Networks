//
// FILE: srt_client.c
//
// Description: this file contains client states' definition, some important data structures
// and the client SRT socket interface definitions. You need to implement all these interfaces
//
// Date: April 18, 2008
//       April 21, 2008 **Added more detailed description of prototypes fixed ambiguities** ATC
//       April 26, 2008 ** Added GBN and send buffer function descriptions **
//

#include "srt_client.h"
#include <errno.h>

int overlay_socket = 0;
client_tcb_t *tcb_table[MAX_TRANSPORT_CONNECTIONS];
pthread_mutex_t mutex;
pthread_cond_t cond;

// Send the seg SYN_MAX_RETRY times. If receive ack seg before timeout, then timer stop;
// Return 1 if success otherwise return -1;
int send_and_wait_ack(seg_t *seg, client_tcb_t *tcb, unsigned int expect_state, unsigned int timeout);
// Use the client port and server port number to identify a tcb in tcb_table.
client_tcb_t *gettcb(int client_port, int server_port);
// Check socket invalid.
int check_sock(int sock_id);
// Sent starting from the first unsent segment until the number of sent-but-not-Acked
// segments reaches GBN_WINDOW.
void send_buff(client_tcb_t *tcb);

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
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

// This function initializes the TCB table marking all entries NULL. It also initializes
// a global variable for the overlay TCP socket descriptor ``conn'' used as input parameter
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

  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&cond, NULL);
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
  client_tcb_t *tcb;

  for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)
  {
    tcb = tcb_table[i];
    if (tcb == NULL)
    {
      tcb = tcb_table[i] = malloc(sizeof(client_tcb_t));
      tcb->state = CLOSED;
      tcb->client_portNum = client_port;
      tcb->bufMutex = malloc(sizeof(pthread_mutex_t));
      pthread_mutex_init(tcb->bufMutex, NULL);
      tcb->next_seqNum = 0;
      tcb->unAck_segNum = 0;
      tcb->sendBufHead = NULL;
      tcb->sendBufunSent = NULL;
      tcb->sendBufTail = NULL;
      return i;
    }
  }

  return -1;
}

// This function is used to connect to the server. It takes the socket ID and the
// server's port number as input parameters. The socket ID is used to find the TCB entry.
// This function sets up the TCB's server port number and a SYN segment to send to
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
  seg.header.seq_num = tcb->next_seqNum;
  seg.header.type = SYN;
  seg.header.length = 0;

  i = send_and_wait_ack(&seg, tcb, CONNECTED, SYN_TIMEOUT);

  if (i == -1)
    tcb->state = CLOSED;

  return i;
}

// Send data to a srt server. This function should use the socket ID to find the TCP entry.
// Then It should create segBufs using the given data and append them to send buffer linked list.
// If the send buffer was empty before insertion, a thread called sendbuf_timer
// should be started to poll the send buffer every SENDBUF_POLLING_INTERVAL time
// to check if a timeout event should occur. If the function completes successfully,
// it returns 1. Otherwise, it returns -1.
//
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int srt_client_send(int sockfd, void *data, unsigned int length)
{
  if (check_sock(sockfd) == -1 || length == 0)
  {
    printf("srt_client_send params invalid. \n");
    return -1;
  }

  client_tcb_t *tcb;
  segBuf_t *buf;

  tcb = tcb_table[sockfd];

  pthread_mutex_lock(tcb->bufMutex);
  do
  {
    buf = malloc(sizeof(segBuf_t));
    buf->seg.header.src_port = tcb->client_portNum;
    buf->seg.header.dest_port = tcb->svr_portNum;
    buf->seg.header.type = DATA;
    buf->seg.header.seq_num = tcb->next_seqNum;
    buf->seg.header.checksum = 0;
    buf->next = NULL;

    if (length > MAX_SEG_LEN)
    {
      mempcpy(buf->seg.data, data, MAX_SEG_LEN);
      buf->seg.header.length = MAX_SEG_LEN;
      tcb->next_seqNum += MAX_SEG_LEN;
      data += MAX_SEG_LEN;
      length -= MAX_SEG_LEN;
    }
    else
    {
      mempcpy(buf->seg.data, data, length);
      buf->seg.header.length = length;
      tcb->next_seqNum += length;
      length = 0;
    }
    buf->seg.header.checksum = checksum(&buf->seg);

    if (tcb->sendBufHead == NULL)
    {
      tcb->sendBufHead = tcb->sendBufTail = tcb->sendBufunSent = buf;
    }
    else
    {
      if (tcb->sendBufunSent == NULL)
        tcb->sendBufunSent = buf;

      tcb->sendBufTail->next = buf;
      tcb->sendBufTail = buf;
    }

  } while (length > 0);
  pthread_mutex_unlock(tcb->bufMutex);

  send_buff(tcb);

  return 1;
}

// This function is used to disconnect from the server. It takes the socket ID as
// an input parameter. The socket ID is used to find the TCB entry in the TCB table.
// This function sends a FIN segment to the server. After the FIN segment is sent
// the state should transition to FINWAIT and a timer started. If the
// state == CLOSED after the timeout the FINACK was successfully received. Else,
// if after a number of retries FIN_MAX_RETRY the state is still FINWAIT then
// the state transitions to CLOSED and -1 is returned.

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int srt_client_disconnect(int sockfd)
{
  if (check_sock(sockfd) == -1)
  {
    printf("Sockfd invalid\n");
    return -1;
  }

  client_tcb_t *tcb;
  seg_t seg;
  segBuf_t *buf;
  int i;

  tcb = tcb_table[sockfd];
  tcb->state = FINWAIT;
  seg.header.src_port = tcb->client_portNum;
  seg.header.dest_port = tcb->svr_portNum;
  seg.header.seq_num = tcb->next_seqNum;
  seg.header.type = FIN;
  seg.header.length = 0;

  i = send_and_wait_ack(&seg, tcb, CLOSED, FIN_TIMEOUT);

  pthread_mutex_lock(tcb->bufMutex);
  buf = tcb->sendBufHead;
  // Free send buff
  while (buf != NULL)
  {
    tcb->sendBufHead = tcb->sendBufHead->next;
    free(buf);
    buf = tcb->sendBufHead;
  }
  tcb->sendBufHead = tcb->sendBufunSent = tcb->sendBufTail = NULL;
  tcb->unAck_segNum = 0;
  tcb->state = CLOSED;
  pthread_mutex_unlock(tcb->bufMutex);

  return i;
}

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
    pthread_mutex_destroy(tcb->bufMutex);
    free(tcb->bufMutex);
    free(tcb);
    tcb_table[sockfd] = NULL;
    return 1;
  }

  return -1;
}

// This is a thread  started by srt_client_init(). It handles all the incoming
// segments from the server. The design of seghanlder is an infinite loop that calls snp_recvseg(). If
// snp_recvseg() fails then the overlay connection is closed and the thread is terminated. Depending
// on the state of the connection when a segment is received  (based on the incoming segment) various
// actions are taken. See the client FSM for more details.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void *seghandler(void *arg)
{
  int i;
  seg_t seg;
  client_tcb_t *tcb;
  segBuf_t *buf;

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
    case DATAACK:
      if (tcb->state == CONNECTED)
      {
        pthread_mutex_lock(tcb->bufMutex);
        buf = tcb->sendBufHead;
        // All send buffer segments that the seq_num less than receive segment's ack_num
        // will be free.
        while (buf && tcb->sendBufHead != tcb->sendBufunSent && buf->seg.header.seq_num < seg.header.ack_num)
        {
          tcb->sendBufHead = tcb->sendBufHead->next;
          free(buf);
          tcb->unAck_segNum -= 1;

          // Send buffer have only one element.
          if (buf == tcb->sendBufTail)
            tcb->sendBufTail = tcb->sendBufHead;

          buf = tcb->sendBufHead;
        }
        pthread_mutex_unlock(tcb->bufMutex);

        send_buff(tcb);
      }
      break;
    default:
      break;
    }
  };

  return 0;
}

// This thread continuously polls send buffer to trigger timeout events
// It should always be running when the send buffer is not empty
// If the current time -  first sent-but-unAcked segment's sent time > DATA_TIMEOUT, a timeout event occurs
// When timeout, resend all sent-but-unAcked segments
// When the send buffer is empty, this thread terminates
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void *sendBuf_timer(void *clienttcb)
{
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  client_tcb_t *tcb;
  segBuf_t *buf;
  int i;
  struct timespec ts;

  tcb = (client_tcb_t *)clienttcb;
  pthread_cond_init(&cond, NULL);
  pthread_mutex_init(&mutex, NULL);

  pthread_mutex_lock(&mutex);
  while (1)
  {
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (ts.tv_nsec + SENDBUF_POLLING_INTERVAL) / 1000000000;
    ts.tv_nsec = (ts.tv_nsec + SENDBUF_POLLING_INTERVAL) % 1000000000;
    if ((i = pthread_cond_timedwait(&cond, &mutex, &ts)) == ETIMEDOUT)
    {
      // The send buf is empty
      pthread_mutex_lock(tcb->bufMutex);
      if (tcb->sendBufHead == NULL || tcb->sendBufHead == tcb->sendBufunSent)
      {
        pthread_mutex_unlock(tcb->bufMutex);
        break;
      }

      i = time(NULL) - tcb->sendBufHead->sentTime;
      if (i > DATA_TIMEOUT)
      {
        // All the sent-but-not-Acked segments in send buffer are re-sent.
        buf = tcb->sendBufHead;
        while (buf != tcb->sendBufunSent)
        {
          snp_sendseg(overlay_socket, &buf->seg);
          buf->sentTime = time(NULL);
          buf = buf->next;
        }
      }
      pthread_mutex_unlock(tcb->bufMutex);
    }
    else
    {
      printf("sendBuf_timer pthread_cond_timedwait error %d\n", i);
      exit(-1);
    }
  }
  pthread_mutex_unlock(&mutex);
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Help functions

int send_and_wait_ack(seg_t *seg, client_tcb_t *tcb, unsigned int expect_state, unsigned int timeout)
{
  int i, error;

  i = 1;
  while (i <= SYN_MAX_RETRY)
  {
    snp_sendseg(overlay_socket, seg);

    pthread_mutex_lock(&mutex);
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (ts.tv_nsec + timeout) / 1000000000;
    ts.tv_nsec = (ts.tv_nsec + timeout) % 1000000000;
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
      printf("send_and_wait_ack pthread_cond_timedwait error %d\n", error);
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

void send_buff(client_tcb_t *tcb)
{
  segBuf_t *buf;
  pthread_t thread_id;
  int i;

  pthread_mutex_lock(tcb->bufMutex);
  i = GBN_WINDOW - tcb->unAck_segNum;
  while (i > 0)
  {
    buf = tcb->sendBufunSent;

    if (buf == NULL)
    {
      break;
    }
    else
    {
      snp_sendseg(overlay_socket, &buf->seg);
      buf->sentTime = time(NULL);
      tcb->unAck_segNum += 1;

      if (tcb->sendBufHead == tcb->sendBufunSent)
      {
        i = pthread_create(&thread_id, NULL, sendBuf_timer, tcb);

        if (i != 0)
        {
          printf("srt_client_send call pthread_create faild, errno: %d\n", i);
          exit(-1);
        }
      }

      tcb->sendBufunSent = buf->next;
    }
    i--;
  }
  pthread_mutex_unlock(tcb->bufMutex);
}