
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "dvtable.h"

//This function creates a dvtable(distance vector table) dynamically.
//A distance vector table contains the n+1 entries, where n is the number of the neighbors of this node, and the rest one is for this node itself.
//Each entry in distance vector table is a dv_t structure which contains a source node ID and an array of N dv_entry_t structures where N is the number of all the nodes in the overlay.
//Each dv_entry_t contains a destination node address the the cost from the source node to this destination node.
//The dvtable is initialized in this function.
//The link costs from this node to its neighbors are initialized using direct link cost retrived from topology.dat.
//Other link costs are initialized to INFINITE_COST.
//The dynamically created dvtable is returned.
dv_t *dvtable_create()
{
  int nb_num, node_num, *nb_id_array, *node_id_array;
  dv_t *dvtable;
  nb_num = topology_getNbrNum();
  node_num = topology_getNodeNum();
  nb_id_array = topology_getNbrArray();
  node_id_array = topology_getNodeArray();
  dvtable = malloc(sizeof(dv_t) * (nb_num + 1));

  for (int i = 0; i <= nb_num; i++)
  {
    int source_id = i == nb_num ? topology_getMyNodeID() : nb_id_array[i];
    dv_t *entry = &dvtable[i];
    entry->nodeID = source_id;
    entry->dvEntry = malloc(sizeof(dv_entry_t) * node_num);

    for (int j = 0; i < node_num; j++)
    {
      int dest_id = node_id_array[i];
      entry->dvEntry[j].nodeID = dest_id;
      entry->dvEntry[j].cost = i == nb_num ? topology_getCost(source_id, dest_id) : INFINITE_COST;
    }
  }

  free(nb_id_array);
  free(node_id_array);

  return dvtable;
}

//This function destroys a dvtable.
//It frees all the dynamically allocated memory for the dvtable.
void dvtable_destroy(dv_t *dvtable)
{
  for (int i = 0; i < topology_getNbrNum(); i++)
    free(dvtable[i].dvEntry);

  free(dvtable);
  dvtable = NULL;
}

//This function sets the link cost between two nodes in dvtable.
//If those two nodes are found in the table and the link cost is set, return 1.
//Otherwise, return -1.
int dvtable_setcost(dv_t *dvtable, int fromNodeID, int toNodeID, unsigned int cost)
{
  int nb_num, node_num;
  nb_num = topology_getNbrNum();
  node_num = topology_getNodeNum();

  for (int i = 0; i <= nb_num; i++)
  {
    dv_t *entry = &dvtable[i];

    for (int j = 0; i < node_num; j++)
    {
      if (entry->nodeID == fromNodeID && entry->dvEntry[j].nodeID == toNodeID)
      {
        entry->dvEntry[j].cost = cost;
        return 1;
      }
    }
  }

  return -1;
}

//This function returns the link cost between two nodes in dvtable
//If those two nodes are found in dvtable, return the link cost.
//otherwise, return INFINITE_COST.
unsigned int dvtable_getcost(dv_t *dvtable, int fromNodeID, int toNodeID)
{
  int nb_num, node_num;
  nb_num = topology_getNbrNum();
  node_num = topology_getNodeNum();

  for (int i = 0; i <= nb_num; i++)
  {
    dv_t *entry = &dvtable[i];

    for (int j = 0; i < node_num; j++)
      if (entry->nodeID == fromNodeID && entry->dvEntry[j].nodeID == toNodeID)
        return entry->dvEntry[j].cost;
  }

  return INFINITE_COST;
}

//This function prints out the contents of a dvtable.
void dvtable_print(dv_t *dvtable)
{
  int nb_num, node_num;
  nb_num = topology_getNbrNum();
  node_num = topology_getNodeNum();

  for (int i = 0; i <= nb_num; i++)
  {
    dv_t *entry = &dvtable[i];

    for (int j = 0; i < node_num; j++)
      printf("distance vector table: %d --- %d : %u\n", entry->nodeID, entry->dvEntry[j].nodeID, entry->dvEntry[j].cost);
  }
}
