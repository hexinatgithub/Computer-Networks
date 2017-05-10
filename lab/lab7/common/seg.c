
#include "seg.h"
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>

//SRT process uses this function to send a segment and its destination node ID in a sendseg_arg_t structure to SNP process to send out.
//Parameter network_conn is the TCP descriptor of the connection between the SRT process and the SNP process.
//Return 1 if a sendseg_arg_t is succefully sent, otherwise return -1.
int snp_sendseg(int network_conn, int dest_nodeID, seg_t *segPtr)
{
  sendseg_arg_t seg_arg;
  seg_arg.nodeID = dest_nodeID;
  memcpy(&seg_arg.seg, segPtr, sizeof(seg_t));

  return send(network_conn, &seg_arg, sizeof(sendseg_arg_t), 0) > 0 ? 1 : -1;
}

//SRT process uses this function to receive a  sendseg_arg_t structure which contains a segment and its src node ID from the SNP process.
//Parameter network_conn is the TCP descriptor of the connection between the SRT process and the SNP process.
//When a segment is received, use seglost to determine if the segment should be discarded, also check the checksum.
//Return 1 if a sendseg_arg_t is succefully received, otherwise return -1.
int snp_recvseg(int network_conn, int *src_nodeID, seg_t *segPtr)
{
  sendseg_arg_t seg_arg;
  while (recv(network_conn, &seg_arg, sizeof(sendseg_arg_t), 0) > 0)
  {
    if (seglost(&seg_arg.seg) == 1)
      continue;

    memcpy(segPtr, &seg_arg.seg, sizeof(seg_t));
    *src_nodeID = seg_arg.nodeID;
    return 1;
  }

  return -1;
}

//SNP process uses this function to receive a sendseg_arg_t structure which contains a segment and its destination node ID from the SRT process.
//Parameter tran_conn is the TCP descriptor of the connection between the SRT process and the SNP process.
//Return 1 if a sendseg_arg_t is succefully received, otherwise return -1.
int getsegToSend(int tran_conn, int *dest_nodeID, seg_t *segPtr)
{
  sendseg_arg_t seg_arg;
  if (recv(tran_conn, &seg_arg, sizeof(sendseg_arg_t), 0) == -1)
    return -1;

  *dest_nodeID = seg_arg.nodeID;
  memcpy(segPtr, &seg_arg.seg, sizeof(seg_t));
  return 1;
}

//SNP process uses this function to send a sendseg_arg_t structure which contains a segment and its src node ID to the SRT process.
//Parameter tran_conn is the TCP descriptor of the connection between the SRT process and the SNP process.
//Return 1 if a sendseg_arg_t is succefully sent, otherwise return -1.
int forwardsegToSRT(int tran_conn, int src_nodeID, seg_t *segPtr)
{
  sendseg_arg_t seg_arg;
  seg_arg.nodeID = src_nodeID;
  memcpy(&seg_arg.seg, segPtr, sizeof(sendseg_arg_t));
  return send(tran_conn, &seg_arg, sizeof(sendseg_arg_t), 0) > 0 ? 1 : -1;
}

// for seglost(seg_t* segment):
// a segment has PKT_LOST_RATE probability to be lost or invalid checksum
// with PKT_LOST_RATE/2 probability, the segment is lost, this function returns 1
// If the segment is not lost, return 0.
// Even the segment is not lost, the packet has PKT_LOST_RATE/2 probability to have invalid checksum
// We flip  a random bit in the segment to create invalid checksum
int seglost(seg_t *segPtr)
{
  int random = rand() % 100;
  if (random < PKT_LOSS_RATE * 100)
  {
    //50% probability of losing a segment
    if (rand() % 2 == 0)
    {
      printf("seg lost!!!\n");
      return 1;
    }
    //50% chance of invalid checksum
    else
    {
      //get data length
      int len = sizeof(srt_hdr_t) + segPtr->header.length;
      //get a random bit that will be flipped
      int errorbit = rand() % (len * 8);
      //flip the bit
      char *temp = (char *)segPtr;
      temp = temp + errorbit / 8;
      *temp = *temp ^ (1 << (errorbit % 8));
      return 0;
    }
  }
  return 0;
}
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//This function calculates checksum over the given segment.
//The checksum is calculated over the segment header and segment data.
//You should first clear the checksum field in segment header to be 0.
//If the data has odd number of octets, add an 0 octets to calculate checksum.
//Use 1s complement for checksum calculation.
unsigned short checksum(seg_t *segment)
{
  register long checksum = 0;
  unsigned int D = 0;

  unsigned short *byteAddress = (unsigned short *)segment;

  if (segment->header.length % 2 == 1)
    segment->data[segment->header.length] = 0;
  D = sizeof(srt_hdr_t) + segment->header.length;
  if (D % 2 == 1)
  {
    D++;
  }
  D = D / 2;

  while (D > 0)
  {
    checksum += *byteAddress++;
    if (checksum & 0x10000)
    {
      checksum = (checksum & 0xFFFF) + 1;
    }
    D--;
  }

  return ~checksum;
}

//Check the checksum in the segment,
//return 1 if the checksum is valid,
//return -1 if the checksum is invalid
int checkchecksum(seg_t *segment)
{
  register long checksum_result = checksum(segment);
  // calculate the return value 1 or -1
  return (~checksum_result) == 0 ? 1 : -1;
}
