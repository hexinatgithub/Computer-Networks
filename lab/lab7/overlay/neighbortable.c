//FILE: overlay/neighbortable.c
//
//Description: this file the API for the neighbor table
//
//Date: May 03, 2010

#include "neighbortable.h"
#include "../topology/topology.h"

//This function first creates a neighbor table dynamically. It then parses the topology/topology.dat file and fill the nodeID and nodeIP fields in all the entries, initialize conn field as -1 .
//return the created neighbor table
nbr_entry_t *nt_create()
{
  int size;
  nbr_entry_t *table;
  struct allocated_IP_array *IParray;

  IParray = topology_getNbrIPArray();
  size = IParray->size;
  table = malloc(size * sizeof(nbr_entry_t));

  for (int i = 0; i < size; i++)
  {
    table[i].nodeID = IParray->arrayID[i];
    table[i].nodeIP = IParray->arrayIP[i];
    table[i].conn = -1;
  }

  free(IParray->arrayIP);
  free(IParray->arrayID);
  free(IParray);

  return table;
}

//This function destroys a neighbortable. It closes all the connections and frees all the dynamically allocated memory.
void nt_destroy(nbr_entry_t *nt)
{
  int size;
  size = topology_getNbrNum();

  for (int i = 0; i < size; i++)
  {
    close(nt[i].conn);
  }
  free(nt);
}

//This function is used to assign a TCP connection to a neighbor table entry for a neighboring node. If the TCP connection is successfully assigned, return 1, otherwise return -1
int nt_addconn(nbr_entry_t *nt, int nodeID, int conn)
{
  int size;

  if (nt)
  {
    size = topology_getNbrNum();
    for (int i = 0; i < size; i++)
    {
      if (nt[i].nodeID == nodeID)
      {
        nt[i].conn = conn;
        return 1;
      }
    }
  }
  return -1;
}
