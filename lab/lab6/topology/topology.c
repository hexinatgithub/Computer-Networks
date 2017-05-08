//FILE: topology/topology.c
//
//Description: this file implements some helper functions used to parse
//the topology file
//
//Date: May 3,2010

#include "topology.h"
#include "../common/constants.h"

// parse file TOPOLOGY_FILE_NAME
// f is a function, return 1 break the parse loop, return 1 to continue
// data is a struct pass into f function's data.
void parse_topology(int (*f)(char *, char *, int, void *data), void *data);

//
int topology_getNbrNum_parse(char *firstHost, char *secondHost, int cost, void *data);

//
int find(struct allocated_array *a, int ID);

//
int topology_getNodeArray_Parse(char *firstHost, char *secondHost, int cost, void *data);

//
int topology_getNbrArray_Parse(char *firstHost, char *secondHost, int cost, void *data);

//
int topology_getNbrIPArray_Parse(char *firstHost, char *secondHost, int cost, void *data);

//this function returns node ID of the given hostname
//the node ID is an integer of the last 8 digit of the node's IP address
//for example, a node with IP address 202.120.92.3 will have node ID 3
//if the node ID can't be retrieved, return -1
int topology_getNodeIDfromname(char *hostname)
{
  if (strcmp(hostname, "localhost") == 0)
    return topology_getMyNodeID();

  struct hostent *hostInfo;
  struct in_addr address;
  int ip_address;

  hostInfo = gethostbyname(hostname);

  if (hostInfo == NULL)
  {
    printf("gethostbyname %s failed!\n", hostname);
    return -1;
  }

  memcpy((char *)&address.s_addr, hostInfo->h_addr_list[0], hostInfo->h_length);
  ip_address = ntohl(address.s_addr);

  return ip_address & 0x000000FF;
}

void topology_getIPfromname(char *hostname, in_addr_t *ip)
{
  if (strcmp(hostname, "localhost") == 0)
  {
    topology_getMyNodeIP(ip);
    return;
  }

  struct hostent *hostInfo;
  hostInfo = gethostbyname(hostname);

  if (hostInfo == NULL)
  {
    printf("gethostbyname %s failed!\n", hostname);
    return;
  }

  memcpy((char *)ip, hostInfo->h_addr_list[0], hostInfo->h_length);
}

//this function returns node ID from the given IP address
//if the node ID can't be retrieved, return -1
int topology_getNodeIDfromip(struct in_addr *addr)
{
  int ipAddr = ntohl(addr->s_addr);
  return ipAddr & 0x000000FF;
}

//this function returns my node ID
//if my node ID can't be retrieved, return -1
int topology_getMyNodeID()
{
  struct in_addr ip;
  topology_getMyNodeIP(&ip.s_addr);

  return topology_getNodeIDfromname(inet_ntoa(ip));
}

void topology_getMyNodeIP(in_addr_t *ip)
{
  struct ifaddrs *ifap, *ifa;
  struct sockaddr_in *sa;
  char *addr;

  getifaddrs(&ifap);
  for (ifa = ifap; ifa; ifa = ifa->ifa_next)
  {
    if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET)
    {
      sa = (struct sockaddr_in *)ifa->ifa_addr;
      addr = inet_ntoa(sa->sin_addr);

      if (strcmp(addr, "127.0.0.1") == 0)
        continue;

      *ip = sa->sin_addr.s_addr;
      break;
    }
  }

  freeifaddrs(ifap);
}

//this functions parses the topology information stored in topology.dat
//returns the number of neighbors
int topology_getNbrNum()
{
  int number = 0;

  parse_topology(topology_getNbrNum_parse, (void *)&number);

  return number;
}

//this functions parses the topology information stored in topology.dat
//returns the number of total nodes in the overlay
int topology_getNodeNum()
{
  struct allocated_array a;

  a.array = NULL;
  a.size = 0;
  parse_topology(topology_getNodeArray_Parse, (void *)&a);
  free(a.array);

  return a.size;
}

//this functions parses the topology information stored in topology.dat
//returns a dynamically allocated array which contains all the nodes' IDs in the overlay network
int *topology_getNodeArray()
{
  struct allocated_array a;

  a.array = NULL;
  a.size = 0;
  parse_topology(topology_getNodeArray_Parse, (void *)&a);

  return a.array;
}

//this functions parses the topology information stored in topology.dat
//returns a dynamically allocated array which contains all the neighbors'IDs
int *topology_getNbrArray()
{
  struct allocated_array a;

  a.array = NULL;
  a.size = 0;
  parse_topology(topology_getNbrArray_Parse, (void *)&a);

  return a.array;
}

//this functions parses the topology information stored in topology.dat
//returns a dynamically allocated struct allocated_IP_array which contains all the neighbors'IDs and IPs
void *topology_getNbrIPArray()
{
  struct allocated_IP_array *a;

  a = malloc(sizeof(struct allocated_IP_array));
  a->arrayIP = NULL;
  a->arrayID = NULL;
  a->size = 0;
  parse_topology(topology_getNbrIPArray_Parse, (void *)a);

  return a;
}

//this functions parses the topology information stored in topology.dat
//returns the cost of the direct link between the two given nodes
//if no direct link between the two given nodes, INFINITE_COST is returned
unsigned int topology_getCost(int fromNodeID, int toNodeID)
{
  FILE *fd;
  char *lineptr = NULL;
  size_t n = 0;
  char *firstHost, *secondHost;
  int cost = INFINITE_COST;

  if ((fd = fopen(TOPOLOGY_FILE_NAME, "r")) == NULL)
  {
    printf("open file %s failed!\n", TOPOLOGY_FILE_NAME);
    return cost;
  }

  while (getline(&lineptr, &n, fd) != -1)
  {
    firstHost = strtok(lineptr, " ");
    secondHost = strtok(NULL, " ");

    if (topology_getNodeIDfromname(firstHost) == fromNodeID &&
        topology_getNodeIDfromname(secondHost) == toNodeID)
    {
      cost = atoi(strtok(NULL, " "));
      break;
    }
  }

  free(lineptr);
  fclose(fd);

  return cost;
}

// ===========================================================================
// Helper function

void parse_topology(int (*f)(char *, char *, int, void *), void *data)
{
  FILE *fd;
  char *lineptr = NULL;
  size_t n = 0;
  char *firstHost, *secondHost;
  int cost = 0, i;

  if ((fd = fopen(TOPOLOGY_FILE_NAME, "r")) == NULL)
  {
    printf("open file %s failed!\n", TOPOLOGY_FILE_NAME);
    return;
  }

  while (getline(&lineptr, &n, fd) != -1)
  {
    firstHost = strtok(lineptr, " ");
    secondHost = strtok(NULL, " ");
    cost = atoi(strtok(NULL, " "));

    i = f(firstHost, secondHost, cost, data);

    if (i == 1)
      break;
  }

  free(lineptr);
  fclose(fd);
}

int topology_getNbrNum_parse(char *firstHost, char *secondHost, int cost, void *data)
{
  int myID, firstID, secondID;
  int *number = (int *)data;

  myID = topology_getMyNodeID();
  firstID = topology_getNodeIDfromname(firstHost);
  secondID = topology_getNodeIDfromname(secondHost);

  if (firstID == myID || secondID == myID)
  {
    *number += 1;
  }

  return -1;
}

int find(struct allocated_array *a, int ID)
{
  for (int i = 0; i < a->size; i++)
  {
    if (a->array[i] == ID)
    {
      return 1;
    }
  }
  return -1;
}

int topology_getNodeArray_Parse(char *firstHost, char *secondHost, int cost, void *data)
{
  int firstID, secondID;
  struct allocated_array *a = (struct allocated_array *)data;

  firstID = topology_getNodeIDfromname(firstHost);
  secondID = topology_getNodeIDfromname(secondHost);

  if (find(a, firstID) == -1)
  {
    a->array = realloc(a->array, (a->size + 1) * sizeof(int));
    a->size++;
    a->array[a->size] = firstID;
  }

  if (find(a, secondID) == -1)
  {
    a->array = realloc(a->array, (a->size + 1) * sizeof(int));
    a->size++;
    a->array[a->size] = secondID;
  }

  return -1;
}

int topology_getNbrArray_Parse(char *firstHost, char *secondHost, int cost, void *data)
{
  int myID, firstID, secondID, ID = -1;
  struct allocated_array *a = (struct allocated_array *)data;

  myID = topology_getMyNodeID();
  firstID = topology_getNodeIDfromname(firstHost);
  secondID = topology_getNodeIDfromname(secondHost);

  if (myID == firstID || myID == secondID)
  {
    ID = myID == firstID ? secondID : firstID;
    if (find(a, ID) == -1)
    {
      a->array = realloc(a->array, (a->size + 1) * sizeof(int));
      a->size++;
      a->array[a->size] = ID;
    }
  }

  return -1;
}

int topology_getNbrIPArray_Parse(char *firstHost, char *secondHost, int cost, void *data)
{
  int myID, firstID, secondID;
  struct allocated_IP_array *a = (struct allocated_IP_array *)data;
  in_addr_t addr1, addr2;

  myID = topology_getMyNodeID();
  firstID = topology_getNodeIDfromname(firstHost);
  secondID = topology_getNodeIDfromname(secondHost);
  topology_getIPfromname(firstHost, &addr1);
  topology_getIPfromname(secondHost, &addr2);

  if (myID == firstID)
  {
    a->arrayIP = realloc(a->arrayIP, (a->size + 1) * sizeof(in_addr_t));
    a->arrayID = realloc(a->arrayID, (a->size + 1) * sizeof(int));
    a->arrayIP[a->size] = addr2;
    a->arrayID[a->size] = secondID;
    a->size++;
  }
  else if (myID == secondID)
  {
    a->arrayIP = realloc(a->arrayIP, (a->size + 1) * sizeof(in_addr_t));
    a->arrayID = realloc(a->arrayID, (a->size + 1) * sizeof(int));
    a->arrayIP[a->size] = addr1;
    a->arrayID[a->size] = firstID;
    a->size++;
  }

  return -1;
}
