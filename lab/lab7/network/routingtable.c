

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "routingtable.h"

//This is the hash function used the by the routing table
//It takes the hash key - destination node ID as input,
//and returns the hash value - slot number for this destination node ID.
int makehash(int node)
{
  return node % MAX_ROUTINGTABLE_SLOTS;
}

//This function creates a routing table dynamically.
//All the entries in the table are initialized to NULL pointers.
//Then for all the neighbors with a direct link, create a routing entry using the neighbor itself as the next hop node, and insert this routing entry into the routing table.
//The dynamically created routing table structure is returned.
routingtable_t *routingtable_create()
{
  routingtable_t *rt_table;
  int *nb_array;

  rt_table = malloc(sizeof(routingtable_t));
  memset(rt_table, 0, sizeof(routingtable_t));
  nb_array = topology_getNbrArray();

  for (int i = 0; i < topology_getNbrNum(); i++)
  {
    int nb_id = nb_array[i];
    routingtable_setnextnode(rt_table, nb_id, nb_id);
  }

  free(nb_array);
  return rt_table;
}

//This funtion destroys a routing table.
//All dynamically allocated data structures for this routing table are freed.
void routingtable_destroy(routingtable_t *routingtable)
{
  routingtable_entry_t *entry, *entry_tmp;

  for (int i = 0; i < MAX_ROUTINGTABLE_SLOTS; i++)
  {
    entry = routingtable->hash[i];
    while (entry)
    {
      entry_tmp = entry;
      entry = entry->next;
      free(entry_tmp);
    }
  }

  free(routingtable);
}

//This function updates the routing table using the given destination node ID and next hop's node ID.
//If the routing entry for the given destination already exists, update the existing routing entry.
//If the routing entry of the given destination is not there, add one with the given next node ID.
//Each slot in routing table contains a linked list of routing entries due to conflicting hash keys (differnt hash keys (destination node ID here) may have same hash values (slot entry number here)).
//To add an routing entry to the hash table:
//First use the hash function makehash() to get the slot number in which this routing entry should be stored.
//Then append the routing entry to the linked list in that slot.
void routingtable_setnextnode(routingtable_t *routingtable, int destNodeID, int nextNodeID)
{
  int i = makehash(destNodeID);
  routingtable_entry_t *entry = routingtable->hash[i];

  if (entry == NULL)
  {
    entry = malloc(sizeof(routingtable_entry_t));
    routingtable->hash[i] = entry;
    memset(entry, 0, sizeof(routingtable_entry_t));
  }
  else
  {
    while (entry)
    {
      if (entry->destNodeID == destNodeID)
        break;
      else if (entry->next == NULL)
      {
        entry->next = malloc(sizeof(routingtable_entry_t));
        entry = entry->next;
        memset(entry, 0, sizeof(routingtable_entry_t));
        break;
      }

      entry = entry->next;
    }
  }
  entry->destNodeID = destNodeID;
  entry->nextNodeID = nextNodeID;
}

//This function looks up the destNodeID in the routing table.
//Since routing table is a hash table, this opeartion has O(1) time complexity.
//To find a routing entry for a destination node, you should first use the hash function makehash() to get the slot number and then go through the linked list in that slot to search the routing entry.
//If the destNodeID is found, return the nextNodeID for this destination node.
//If the destNodeID is not found, return -1.
int routingtable_getnextnode(routingtable_t *routingtable, int destNodeID)
{
  int i = makehash(destNodeID);
  routingtable_entry_t *entry = routingtable->hash[i];

  while (entry)
  {
    if (entry->destNodeID == destNodeID)
      return entry->nextNodeID;

    entry = entry->next;
  }

  return -1;
}

//This function prints out the contents of the routing table
void routingtable_print(routingtable_t *routingtable)
{
  routingtable_entry_t *entry;

  for (int i = 0; i < MAX_ROUTINGTABLE_SLOTS; i++)
  {
    entry = routingtable->hash[i];
    while (entry)
    {
      printf("routing table: %d --- %d", entry->destNodeID, entry->nextNodeID);
      entry = entry->next;
    }
  }
}
