// File pkt.c
// May 03, 2010

#include "pkt.h"

enum PKTSTATE
{
  PKTSTART1,
  PKTSTART2,
  PKTRECV,
  PKTSTOP1
};

// When sending the packet over the TCP connection between the SNP
// process and the ON process, use '!&'  and '!#' as delimiters.
// Send !& packet !# over the TCP connection.
// Return 1 if packet is sent successfully, otherwise return -1.
int send_pkt_with_delimiter(int overlay_conn, void *buff, size_t len);
// To receive the packet, this function uses a simple finite state machine(FSM)
// PKTSTART1 -- starting point
// PKTSTART2 -- '!' received, expecting '&' to receive data
// PKTRECV -- '&' received, start receiving data
// PKTSTOP1 -- '!' received, expecting '#' to finish receiving data
// Return 1 if a packet is received successfully, otherwise return -1.
int recv_pkt_without_delimiter(int overlay_conn, void *buff, size_t len);

// overlay_sendpkt() is called by the SNP process to request the ON
// process to send a packet out to the overlay network. The
// ON process and SNP process are connected with a local TCP connection.
// In overlay_sendpkt(), the packet and its next hop's nodeID are encapsulated
// in a sendpkt_arg_t data structure and sent over this TCP connection to the ON process.
// The parameter overlay_conn is the TCP connection's socket descriptior
// between the SNP process and the ON process.
// When sending the sendpkt_arg_t data structure over the TCP connection between the SNP
// process and the ON process, use '!&'  and '!#' as delimiters.
// Send !& sendpkt_arg_t structure !# over the TCP connection.
// Return 1 if sendpkt_arg_t data structure is sent successfully, otherwise return -1.
int overlay_sendpkt(int nextNodeID, snp_pkt_t *pkt, int overlay_conn)
{
  sendpkt_arg_t pkt_arg;

  pkt_arg.nextNodeID = nextNodeID;
  memcpy(&pkt_arg.pkt, pkt, sizeof(snp_pkt_t));

  return send_pkt_with_delimiter(overlay_conn, &pkt_arg, sizeof(sendpkt_arg_t));
}

// overlay_recvpkt() function is called by the SNP process to receive a packet
// from the ON process. The parameter overlay_conn is the TCP connection's socket
// descriptior between the SNP process and the ON process. The packet is sent over
// the TCP connection between the SNP process and the ON process, and delimiters
// !& and !# are used.
// To receive the packet, this function uses a simple finite state machine(FSM)
// PKTSTART1 -- starting point
// PKTSTART2 -- '!' received, expecting '&' to receive data
// PKTRECV -- '&' received, start receiving data
// PKTSTOP1 -- '!' received, expecting '#' to finish receiving data
// Return 1 if a packet is received successfully, otherwise return -1.
int overlay_recvpkt(snp_pkt_t *pkt, int overlay_conn)
{
  return recv_pkt_without_delimiter(overlay_conn, pkt, sizeof(snp_pkt_t));
}

// This function is called by the ON process to receive a sendpkt_arg_t data structure.
// A packet and the next hop's nodeID is encapsulated  in the sendpkt_arg_t structure.
// The parameter network_conn is the TCP connection's socket descriptior between the
// SNP process and the ON process. The sendpkt_arg_t structure is sent over the TCP
// connection between the SNP process and the ON process, and delimiters !& and !# are used.
// To receive the packet, this function uses a simple finite state machine(FSM)
// PKTSTART1 -- starting point
// PKTSTART2 -- '!' received, expecting '&' to receive data
// PKTRECV -- '&' received, start receiving data
// PKTSTOP1 -- '!' received, expecting '#' to finish receiving data
// Return 1 if a sendpkt_arg_t structure is received successfully, otherwise return -1.
int getpktToSend(snp_pkt_t *pkt, int *nextNode, int network_conn)
{
  sendpkt_arg_t pkt_arg;

  if (recv_pkt_without_delimiter(network_conn, &pkt_arg, sizeof(sendpkt_arg_t)) == 1)
  {
    memcpy(pkt, &pkt_arg.pkt, sizeof(snp_pkt_t));
    *nextNode = pkt_arg.nextNodeID;
    return 1;
  }

  return -1;
}

// forwardpktToSNP() function is called after the ON process receives a packet from
// a neighbor in the overlay network. The ON process calls this function
// to forward the packet to SNP process.
// The parameter network_conn is the TCP connection's socket descriptior between the SNP
// process and ON process. The packet is sent over the TCP connection between the SNP process
// and ON process, and delimiters !& and !# are used.
// Send !& packet data !# over the TCP connection.
// Return 1 if the packet is sent successfully, otherwise return -1.
int forwardpktToSNP(snp_pkt_t *pkt, int network_conn)
{
  return send_pkt_with_delimiter(network_conn, pkt, sizeof(snp_pkt_t));
}

// sendpkt() function is called by the ON process to send a packet
// received from the SNP process to the next hop.
// Parameter conn is the TCP connection's socket descritpor to the next hop node.
// The packet is sent over the TCP connection between the ON process and a neighboring node,
// and delimiters !& and !# are used.
// Send !& packet data !# over the TCP connection
// Return 1 if the packet is sent successfully, otherwise return -1.
int sendpkt(snp_pkt_t *pkt, int conn)
{
  return send_pkt_with_delimiter(conn, pkt, sizeof(snp_pkt_t));
}

// recvpkt() function is called by the ON process to receive
// a packet from a neighbor in the overlay network.
// Parameter conn is the TCP connection's socket descritpor to a neighbor.
// The packet is sent over the TCP connection  between the ON process and the neighbor,
// and delimiters !& and !# are used.
// To receive the packet, this function uses a simple finite state machine(FSM)
// PKTSTART1 -- starting point
// PKTSTART2 -- '!' received, expecting '&' to receive data
// PKTRECV -- '&' received, start receiving data
// PKTSTOP1 -- '!' received, expecting '#' to finish receiving data
// Return 1 if the packet is received successfully, otherwise return -1.
int recvpkt(snp_pkt_t *pkt, int conn)
{
  return recv_pkt_without_delimiter(conn, pkt, sizeof(snp_pkt_t));
}

int send_pkt_with_delimiter(int overlay_conn, void *buff, size_t len)
{
  char bufstart[2];
  char bufend[2];

  bufstart[0] = '!';
  bufstart[1] = '&';
  bufend[0] = '!';
  bufend[1] = '#';

  if (send(overlay_conn, bufstart, 2, 0) < 0)
  {
    return -1;
  }

  if (send(overlay_conn, buff, len, 0) < 0)
  {
    return -1;
  }

  if (send(overlay_conn, bufend, 2, 0) < 0)
  {
    return -1;
  }

  return 1;
}

int recv_pkt_without_delimiter(int overlay_conn, void *data, size_t len)
{
  char buf[len + 2];
  char c;
  int idx = 0;
  int state = PKTSTART1;

  while (recv(overlay_conn, &c, 1, 0) > 0)
  {
    if (state == PKTSTART1)
    {
      if (c == '!')
        state = PKTSTART2;
    }
    else if (state == PKTSTART2)
    {
      if (c == '&')
        state = PKTRECV;
      else
        state = PKTSTART1;
    }
    else if (state == PKTRECV)
    {
      if (c == '!')
      {
        buf[idx] = c;
        idx++;
        state = PKTSTOP1;
      }
      else
      {
        buf[idx] = c;
        idx++;
      }
    }
    else if (state == PKTSTOP1)
    {
      if (c == '#')
      {
        memcpy(data, buf, len);
        return 1;
      }
      else if (c == '!')
      {
        buf[idx] = c;
        idx++;
      }
      else
      {
        buf[idx] = c;
        idx++;
        state = PKTRECV;
      }
    }
  }
  return -1;
}
