
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "nbrcosttable.h"
#include "../common/constants.h"
#include "../topology/topology.h"

//This function creates a neighbor cost table dynamically
//and initialize the table with all its neighbors' node IDs and direct link costs.
//The neighbors' node IDs and direct link costs are retrieved from topology.dat file.
nbr_cost_entry_t *nbrcosttable_create()
{
  int nb_num, *nb_id_array, myID;
  nbr_cost_entry_t *nb_cost_table;

  nb_num = topology_getNbrNum();
  nb_id_array = topology_getNbrArray();
  myID = topology_getMyNodeID();
  nb_cost_table = malloc(nb_num * sizeof(nbr_cost_entry_t));

  for (int i = 0; i < nb_num; i++)
  {
    int nb_id = nb_id_array[i];
    nb_cost_table[i].nodeID = nb_id;
    nb_cost_table[i].cost = topology_getCost(myID, nb_id);
  }

  free(nb_id_array);
  return nb_cost_table;
}

//This function destroys a neighbor cost table.
//It frees all the dynamically allocated memory for the neighbor cost table.
void nbrcosttable_destroy(nbr_cost_entry_t *nct)
{
  free(nct);
}

//This function is used to get the direct link cost from neighbor.
//The direct link cost is returned if the neighbor is found in the table.
//INFINITE_COST is returned if the node is not found in the table.
unsigned int nbrcosttable_getcost(nbr_cost_entry_t *nct, int nodeID)
{
  int nb_num;
  nb_num = topology_getNbrNum();

  for (int i = 0; i < nb_num; i++)
  {
    if (nct[i].nodeID == nodeID)
    {
      return nct[i].cost;
    }
  }

  return INFINITE_COST;
}

//This function prints out the contents of a neighbor cost table.
void nbrcosttable_print(nbr_cost_entry_t *nct)
{
  int nb_num, myID;
  nb_num = topology_getNbrNum();
  myID = topology_getMyNodeID();

  for (int i = 0; i < nb_num; i++)
  {
    printf("neighbor cost table: %d -- %d : %d", myID, nct[i].nodeID, nct[i].cost);
  }
}
