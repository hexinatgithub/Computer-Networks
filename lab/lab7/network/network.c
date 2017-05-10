//FILE: network/network.c
//
//Description: this file implements network layer process
//
//Date: April 29,2008

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../common/seg.h"
#include "../topology/topology.h"
#include "network.h"
#include "nbrcosttable.h"
#include "dvtable.h"
#include "routingtable.h"

//network layer waits this time for establishing the routing paths
#define NETWORK_WAITTIME 60

/**************************************************************/
//delare global variables
/**************************************************************/
int overlay_conn;                    //connection to the overlay
int transport_conn;                  //connection to the transport
nbr_cost_entry_t *nct;               //neighbor cost table
dv_t *dv;                            //distance vector table
pthread_mutex_t *dv_mutex;           //dvtable mutex
routingtable_t *routingtable;        //routing table
pthread_mutex_t *routingtable_mutex; //routingtable mutex

/**************************************************************/
//implementation network layer functions
/**************************************************************/

//This function is used to for the SNP process to connect to the local ON process on port OVERLAY_PORT.
//TCP descriptor is returned if success, otherwise return -1.
int connectToOverlay()
{
  int sfd;
  struct sockaddr_in addr;
  sfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sfd == -1)
  {
    printf("network layer: create socket failed!\n");
    return -1;
  }

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  topology_getMyNodeIP(&addr.sin_addr.s_addr);
  addr.sin_port = htons(OVERLAY_PORT);

  if (connect(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == 0)
  {
    return sfd;
  }

  return -1;
}

//This thread sends out route update packets every ROUTEUPDATE_INTERVAL time
//The route update packet contains this node's distance vector.
//Broadcasting is done by set the dest_nodeID in packet header as BROADCAST_NODEID
//and use overlay_sendpkt() to send the packet out using BROADCAST_NODEID address.
void *routeupdate_daemon(void *arg)
{
  snp_pkt_t pkt;
  pkt_routeupdate_t route_update;
  int *node_id_array;

  pkt.header.src_nodeID = topology_getMyNodeID();
  pkt.header.dest_nodeID = BROADCAST_NODEID;
  pkt.header.length = sizeof(pkt_routeupdate_t);
  pkt.header.type = ROUTE_UPDATE;
  route_update.entryNum = topology_getNodeNum();
  memset(route_update.entry, 0, MAX_NODE_NUM * sizeof(routeupdate_entry_t));
  node_id_array = topology_getNodeArray();

  do
  {
    // set pkt_routeupdate_t, the distance vector is retrieved from the source node’s distance vector table.
    pthread_mutex_lock(dv_mutex);
    for (int i = 0; i < route_update.entryNum; i++)
    {
      routeupdate_entry_t *rt_entry = &route_update.entry[i];
      rt_entry->nodeID = node_id_array[i];
      rt_entry->cost = dvtable_getcost(dv, pkt.header.src_nodeID, rt_entry->nodeID);
    }
    memcpy(pkt.data, &route_update, sizeof(pkt_routeupdate_t));
    pthread_mutex_unlock(dv_mutex);

    if (overlay_sendpkt(BROADCAST_NODEID, &pkt, overlay_conn) == -1)
      break;
  } while (sleep(ROUTEUPDATE_INTERVAL) == 0);

  free(node_id_array);
  close(overlay_conn);
  overlay_conn = -1;
  pthread_exit(NULL);
}

//This thread handles incoming packets from the ON process.
//It receives packets from the ON process by calling overlay_recvpkt().
//If the packet is a SNP packet and the destination node is this node, forward the packet to the SRT process.
//If the packet is a SNP packet and the destination node is not this node, forward the packet to the next hop according to the routing table.
//If this packet is an Route Update packet, update the distance vector table and the routing table.
void *pkthandler(void *arg)
{
  snp_pkt_t pkt;
  int myID = topology_getMyNodeID();

  while (overlay_recvpkt(&pkt, overlay_conn) > 0)
  {
    if (pkt.header.type == SNP && pkt.header.dest_nodeID == myID)
    {
      seg_t seg;
      memcpy(&seg, pkt.data, pkt.header.length);
      forwardsegToSRT(transport_conn, pkt.header.src_nodeID, &seg);
    }
    else if (pkt.header.type == SNP && pkt.header.dest_nodeID != myID)
    {
      pthread_mutex_lock(routingtable_mutex);
      int nextID = routingtable_getnextnode(routingtable, pkt.header.dest_nodeID);
      pthread_mutex_unlock(routingtable_mutex);
      overlay_sendpkt(nextID, &pkt, overlay_conn);
    }
    else if (pkt.header.type == ROUTE_UPDATE)
    {
      // update the distance vector table and the routing table.
      pthread_mutex_lock(dv_mutex);
      pthread_mutex_lock(routingtable_mutex);

      pkt_routeupdate_t route_update;
      memcpy(&route_update, pkt.data, pkt.header.length);

      for (int i = 0; i < route_update.entryNum; i++)
      {
        routeupdate_entry_t *rt_update_entry = &route_update.entry[i];
        int my_cost, fw_to_nb_cost;

        dvtable_setcost(dv, pkt.header.src_nodeID, rt_update_entry->nodeID, rt_update_entry->cost);
        my_cost = dvtable_getcost(dv, myID, rt_update_entry->nodeID);
        fw_to_nb_cost = nbrcosttable_getcost(nct, pkt.header.src_nodeID) + rt_update_entry->cost;

        if (my_cost > fw_to_nb_cost)
        {
          dvtable_setcost(dv, myID, rt_update_entry->nodeID, fw_to_nb_cost);
          routingtable_setnextnode(routingtable, rt_update_entry->nodeID, pkt.header.src_nodeID);
        }
      }

      pthread_mutex_unlock(dv_mutex);
      pthread_mutex_unlock(routingtable_mutex);
    }
  }
  close(overlay_conn);
  overlay_conn = -1;
  pthread_exit(NULL);
}

//This function stops the SNP process.
//It closes all the connections and frees all the dynamically allocated memory.
//It is called when the SNP process receives a signal SIGINT.
void network_stop()
{
  close(transport_conn);
  close(overlay_conn);
  nbrcosttable_destroy(nct);
  dvtable_destroy(dv);
  pthread_mutex_destroy(dv_mutex);
  free(dv_mutex);
  routingtable_destroy(routingtable);
  pthread_mutex_destroy(routingtable_mutex);
  free(routingtable_mutex);
  exit(0);
}

//This function opens a port on NETWORK_PORT and waits for the TCP connection from local SRT process.
//After the local SRT process is connected, this function keeps receiving sendseg_arg_ts which contains the segments and their destination node addresses from the SRT process. The received segments are then encapsulated into packets (one segment in one packet), and sent to the next hop using overlay_sendpkt. The next hop is retrieved from routing table.
//When a local SRT process is disconnected, this function waits for the next SRT process to connect.
void waitTransport()
{
  struct sockaddr_in addr;
  int sfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sfd == -1)
  {
    printf("waitNetwork create socket failed!\n");
    exit(-1);
  }

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(NETWORK_PORT);

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

  while ((transport_conn = accept(sfd, NULL, NULL)) > 0)
  {
    snp_pkt_t pkt;
    int nextID;

    pkt.header.src_nodeID = topology_getMyNodeID();
    pkt.header.length = sizeof(seg_t);
    pkt.header.type = SNP;

    while (getsegToSend(transport_conn, &pkt.header.dest_nodeID, (seg_t *)&pkt.data))
    {
      pthread_mutex_lock(routingtable_mutex);
      nextID = routingtable_getnextnode(routingtable, pkt.header.dest_nodeID);
      pthread_mutex_unlock(routingtable_mutex);
      overlay_sendpkt(nextID, &pkt, overlay_conn);
    }
    close(transport_conn);
    transport_conn = -1;
  }

  close(sfd);
}

int main(int argc, char *argv[])
{
  printf("network layer is starting, pls wait...\n");

  //initialize global variables
  nct = nbrcosttable_create();
  dv = dvtable_create();
  dv_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(dv_mutex, NULL);
  routingtable = routingtable_create();
  routingtable_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(routingtable_mutex, NULL);
  overlay_conn = -1;
  transport_conn = -1;

  nbrcosttable_print(nct);
  dvtable_print(dv);
  routingtable_print(routingtable);

  //register a signal handler which is used to terminate the process
  signal(SIGINT, network_stop);

  //connect to local ON process
  overlay_conn = connectToOverlay();
  if (overlay_conn < 0)
  {
    printf("can't connect to overlay process\n");
    exit(1);
  }

  //start a thread that handles incoming packets from ON process
  pthread_t pkt_handler_thread;
  pthread_create(&pkt_handler_thread, NULL, pkthandler, (void *)0);

  //start a route update thread
  pthread_t routeupdate_thread;
  pthread_create(&routeupdate_thread, NULL, routeupdate_daemon, (void *)0);

  printf("network layer is started...\n");
  printf("waiting for routes to be established\n");
  sleep(NETWORK_WAITTIME);
  routingtable_print(routingtable);

  //wait connection from SRT process
  printf("waiting for connection from SRT process\n");
  waitTransport();
}
