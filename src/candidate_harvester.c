


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include <netdb.h>
#include <ifaddrs.h>

#include <sockaddr_util.h>
#include "candidate_harvester.h"


void
printhCandidate(struct hcand* cand)
{
  char addr_str[SOCKADDR_MAX_STRLEN];
  printf( "(%s)\t  a=candidate:%s 1 ", cand->ifname, cand->ice.foundation);
  printf( "%s ",
          ICELIBTYPES_ICE_TRANSPORT_PROTO_toString(cand->ice.transport) );

  /* Priority */
  printf( "12345678 ");


  printf( "%s ",
          sockaddr_toString( (struct sockaddr*)&cand->ice.connectionAddr,
                             addr_str,
                             sizeof(addr_str),
                             false ) );

  printf( "%i ",    sockaddr_ipPort(
            (struct sockaddr*)&cand->ice.connectionAddr) );

  printf( "typ %s", ICELIBTYPES_ICE_CANDIDATE_TYPE_toString(cand->ice.type) );
  if (cand->ice.transport == ICE_TRANS_TCPPASS)
  {
    printf( " tcptype %s",
            ICELIBTYPES_ICE_TRANSPORT_toString(cand->ice.transport) );
  }
  printf("\n");
}

size_t
store_cand(struct hcand** candidates,
           size_t*        numCand,
           size_t*        currCandStoreCnt,
           int            sockfd,
           socklen_t*     addrsize,
           char*          ifname,
           int            proto)
{

  if (*currCandStoreCnt < (*numCand) + 1)
  {
    void* tmp = NULL;
    tmp = realloc( *candidates, sizeof(struct hcand) * ( (*numCand) + 5 ) );
    if (tmp == NULL)
    {
      /* Memory failed. */
      return *numCand;
    }
    else
    {
      *candidates          = tmp;
      (*currCandStoreCnt) += 5;
    }
  }
  struct hcand* cand = *candidates;

  getsockname(sockfd,
              (struct sockaddr*)&cand[*numCand].ice.connectionAddr,
              addrsize);

  if ( sockaddr_isAddrLinkLocal( (struct sockaddr*)&cand[*numCand].ice.
                                 connectionAddr ) )
  {
    close(sockfd);
    return 0;
  }
  else
  {


    strncpy(cand[*numCand].ifname, ifname, IFNAMSIZ);

    cand[*numCand].sockfd = sockfd;
    /* Set sockfd as foundation as well */
    sprintf(cand[*numCand].ice.foundation, "%d", sockfd);

    /* Somebody should set this priority later */
    cand[*numCand].ice.priority = 0;

    if (proto == SOCK_DGRAM)
    {
      cand[*numCand].ice.transport = ICE_TRANS_UDP;
    }
    if (proto == SOCK_STREAM)
    {
      cand[*numCand].ice.transport = ICE_TRANS_TCPPASS;
    }
    cand[*numCand].ice.type = ICE_CAND_TYPE_HOST;
    (*numCand)++;
    return *numCand;
  }
}


int32_t
harvest_host(struct hcand** candidates,
             int            proto)
{
  struct ifaddrs* ifaddr, * ifa;
  int             s;
  char            host[NI_MAXHOST];

  size_t numCand        = 0;
  size_t numCandStorage = 0;
  if (getifaddrs(&ifaddr) == -1)
  {
    perror("getifaddrs");
    exit(EXIT_FAILURE);
  }
  int sockfd;

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
  {
    if (ifa->ifa_addr == NULL)
    {
      continue;
    }
    s = getnameinfo(ifa->ifa_addr,
                    sizeof(struct sockaddr_in),
                    host,
                    NI_MAXHOST,
                    NULL,
                    0,
                    NI_NUMERICHOST);

    if ( (ifa->ifa_addr->sa_family == PF_INET6) ||
         (ifa->ifa_addr->sa_family == PF_INET) )
    {
      int family = ifa->ifa_addr->sa_family;
      if (s != 0)
      {
        printf( "getnameinfo() failed: %s\n", gai_strerror(s) );
        exit(EXIT_FAILURE);
      }
      /* Ignore loopback */
      if ( 0 != strncmp("lo", ifa->ifa_name, 2) )
      {
        sockfd = socket(family, proto, 0);
        if (sockfd == -1)
        {
          perror("createSocket: socket");
          continue;
        }
        socklen_t addrsize =
          (family ==
           AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
        if (bind(sockfd, ifa->ifa_addr, addrsize)  == -1)
        {
          close(sockfd);
          perror("createSocket: bind");
          continue;
        }
        store_cand(candidates,
                   &numCand, &numCandStorage,
                   sockfd,
                   &addrsize,
                   ifa->ifa_name,
                   proto);
      }
    }
  }
  /* Reclaim some memory */
  if (numCandStorage > numCand)
  {
    void* tmp = NULL;
    tmp = realloc(*candidates, sizeof(struct hcand) * numCand);
    if (tmp == NULL)
    {
      /* Memory failed. But how I just trimmed it.. */
    }
    else
    {
      *candidates = tmp;
    }
  }
  freeifaddrs(ifaddr);
  return numCand;
}
