//
// FILE: seg.h

// Description: This file contains segment definitions and interfaces to send and receive segments.
// The prototypes support snp_sendseg() and snp_rcvseg() for sending to the network layer.
//
// Date: April 18, 2008
//       April 21, 2008 **Added more detailed description of prototypes fixed ambiguities** ATC
//       April 26, 2008 **Added checksum descriptions**
//

#include "seg.h"
#include "stdio.h"

//
//
//  SNP API for the client and server sides
//  =======================================
//
//  In what follows, we provide the prototype definition for each call and limited pseudo code representation
//  of the function. This is not meant to be comprehensive - more a guideline.
//
//  You are free to design the code as you wish.
//
//  NOTE: snp_sendseg() and snp_recvseg() are services provided by the networking layer
//  i.e., simple network protocol to the transport layer.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

// Send a SRT segment over the overlay network (this is simply a single TCP connection in the
// case of Lab4). TCP sends data as a byte stream. In order to send segments over the overlay TCP connection,
// delimiters for the start and end of the packet must be added to the transmission.
// That is, first send the characters ``!&'' to indicate the start of a  segment; then
// send the segment seg_t; and finally, send end of packet markers ``!#'' to indicate the end of a segment.
// Return 1 in case of success, and -1 in case of failure. snp_sendseg() uses
// send() to first send two chars, then send() again but for the seg_t, and, then
// send() two chars for the end of packet.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int snp_sendseg(int connection, seg_t *segPtr)
{
  /**
	* calculate the checksum and set it to 
	*/
  //printf("snp_sendseg connection = %d, client_port = %d, server_port = %d \n", connection, segPtr->header.src_port, segPtr->header.dest_port);
  char bufstart[2];
  char bufend[2];
  bufstart[0] = '!';
  bufstart[1] = '&';
  bufend[0] = '!';
  bufend[1] = '#';
  if (send(connection, bufstart, 2, 0) < 0)
  {
    return -1;
  }
  //reset the checksum variable and set it to 0;
  segPtr->header.checksum = 0;
  segPtr->header.checksum = checksum(segPtr);
  if (send(connection, segPtr, sizeof(seg_t), 0) < 0)
  {
    return -1;
  }
  if (send(connection, bufend, 2, 0) < 0)
  {
    return -1;
  }
  return 1;
}

// Receive a segment over overlay network (this is a single TCP connection in the case of
// Lab4). We recommend that you receive one byte at a time using recv(). Here you are looking for
// ``!&'' characters then seg_t and then ``!#''. This is a FSM of sorts and you
// should code it that way. Make sure that you cover cases such as ``#&bbb!b!bn#bbb!#''
// The assumption here (fairly limiting but simplistic) is that !& and !# will not
// be seen in the data in the segment. You should read in one byte as a char at
// a time and copy the data part into a buffer to be returned to the caller.
//
// IMPORTANT: once you have parsed a segment you should call seglost(). The code
// for seglost(seg_t* segment) is provided for you below snp_recvseg().
//
// a segment has PKT_LOST_RATE probability to be lost or invalid checksum
// with PKT_LOST_RATE/2 probability, the segment is lost, this function returns 1
// if the segment is not lost, return 0
// Even the segment is not lost, the packet has PKT_LOST_RATE/2 probability to have invalid checksum
//  We flip  a random bit in the segment to create invalid checksum
int snp_recvseg(int connection, seg_t *segPtr)
{
  char buf[sizeof(seg_t) + 2];
  char c;
  int idx = 0;
  // state can be 0,1,2,3;
  // 0 starting point
  // 1 '!' received
  // 2 '&' received, start receiving segment
  // 3 '!' received,
  // 4 '#' received, finish receiving segment
  int state = 0;
  while (recv(connection, &c, 1, 0) > 0)
  {
    if (state == 0)
    {
      if (c == '!')
        state = 1;
    }
    else if (state == 1)
    {
      if (c == '&')
        state = 2;
      else
        state = 0;
    }
    else if (state == 2)
    {
      if (c == '!')
      {
        buf[idx] = c;
        idx++;
        state = 3;
      }
      else
      {
        buf[idx] = c;
        idx++;
      }
    }
    else if (state == 3)
    {
      if (c == '#')
      {
        buf[idx] = c;
        idx++;
        state = 0;
        idx = 0;
        // seg lost commented for now
        if (seglost(segPtr) > 0)
        {
          printf("seg lost!!!\n");
          continue;
        }
        int checksum = checkchecksum(segPtr);
        if (checkchecksum < 0)
        {
          continue;
        }
        memcpy(segPtr, buf, sizeof(seg_t));
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
        state = 2;
      }
    }
  }
  return -1;
}

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

//this function calculates checksum over the given segment
//the checksum is calculated over the segment header and segment data
//you should first clear the checksum field in segment header to be 0
//if the data has odd number of octets, add an 0 octets to calculate checksum
//use 1s complement for checksum calculation
unsigned short checksum(seg_t *segment)
{
  register long checksum = 0;
  unsigned int D = 0;

  // convert into 16 bits format "divides D into 16-bits-long values, and adds all these 16-bits-long values"
  unsigned short *byteAddress = (unsigned short *)segment;

  // add a byte of 0 if odd number
  if (segment->header.length % 2 == 1)
    segment->data[segment->header.length] = 0;
  // Denote the data from which the checksum is calculated as D. D = segment header + segment data.
  D = sizeof(srt_hdr_t) + segment->header.length;
  if (D % 2 == 1)
  {
    D++; //If size of D (in bytes) is an odd number, append a byte with all bits set as 0 to D
  }
  // http://www.rfc-editor.org/rfc/rfc1071.txt following the checksum logic mentioned in RFC
  D = D / 2;
  // Compute Internet Checksum for "count" bytes
  // beginning at location "addr".
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

//check the checksum in the segment,
//return 1 if the checksum is valid,
//return -1 if the checksum is invalid
int checkchecksum(seg_t *segment)
{
  register long checksum_result = checksum(segment);
  // calculate the return value 1 or -1
  return (~checksum_result) == 0 ? 1 : -1;
}
