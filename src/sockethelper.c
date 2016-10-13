#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sockaddr_util.h>




int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}

/* get sockaddr, IPv4 or IPv6: */
void*
get_in_addr(struct sockaddr* sa)
{
  if (sa->sa_family == AF_INET)
  {
    return &( ( (struct sockaddr_in*)sa )->sin_addr );
  }
  return &( ( (struct sockaddr_in6*)sa )->sin6_addr );
}

int
createUdpSocket(char              host[],
                char              port[],
                char              outprintName[],
                int               ai_flags,
                struct addrinfo*  servinfo,
                struct addrinfo** p)
{
  struct addrinfo hints;
  int             rv, sockfd;
  memset(&hints, 0, sizeof hints);
  hints.ai_family   = AF_UNSPEC; /* set to AF_INET to force IPv4 */
  hints.ai_family   = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags    = ai_flags; /* use my IP if not 0 */

  if ( ( rv = getaddrinfo(host, port, &hints, &servinfo) ) != 0 )
  {
    fprintf( stderr, "getaddrinfo: %s\n", gai_strerror(rv) );
    return -1;
  }

  /* loop through all the results and bind to the first we can */
  for ( (*p) = servinfo; (*p) != NULL; (*p) = (*p)->ai_next )
  {
    if ( ( sockfd = socket( (*p)->ai_family, (*p)->ai_socktype,
                            (*p)->ai_protocol ) ) == -1 )
    {
      perror("createSocket: socket");
      continue;
    }

    if ( (ai_flags != 0) &&
         (bind(sockfd, (*p)->ai_addr, (*p)->ai_addrlen) == -1) )
    {
      close(sockfd);
      perror("createSocket: bind");
      continue;
    }

    break;
  }

  if ( (*p) == NULL )
  {
    fprintf(stderr, "%s: failed to bind socket\n", outprintName);
    return -2;
  }
  else
  {
    fprintf(stderr, "%s: created socket\n", outprintName);
  }

  return sockfd;
}

int
createTcpSocket(char* dst,
                char* port)
{
  struct addrinfo hints, * servinfo, * p;
  int             rv;
  int             sockfd;
  char            s[INET6_ADDRSTRLEN];

  memset(&hints, 0, sizeof hints);
  hints.ai_family   = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ( ( rv = getaddrinfo(dst, port, &hints, &servinfo) ) != 0 )
  {
    fprintf( stderr, "getaddrinfo: %s\n", gai_strerror(rv) );
    return -1;
  }

  /* loop through all the results and connect to the first we can */
  for (p = servinfo; p != NULL; p = p->ai_next)
  {
    if ( ( sockfd = socket(p->ai_family, p->ai_socktype,
                           p->ai_protocol) ) == -1 )
    {
      perror("client: socket");
      continue;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
    {
      close(sockfd);
      perror("client: connect");
      continue;
    }
    break;
  }

  if (p == NULL)
  {
    fprintf(stderr, "client: failed to connect\n");
    return -2;
  }

  inet_ntop(p->ai_family, get_in_addr( (struct sockaddr*)p->ai_addr ),
            s, sizeof s);
  printf("client: connecting to %s\n", s);

  freeaddrinfo(servinfo);   /* all done with this structure */

  return sockfd;
}

int
getSockaddrFromFqdn(struct sockaddr* addr_out,
                    const char*      fqdn)
{
  struct addrinfo hints, * res, * p;
  int             status;

  memset(&hints, 0, sizeof hints);
  hints.ai_family   = AF_UNSPEC; /* AF_INET or AF_INET6 to force version */
  hints.ai_socktype = SOCK_STREAM;

  if ( ( status = getaddrinfo(fqdn, NULL, &hints, &res) ) != 0 )
  {
    fprintf( stderr, "getaddrinfo: %s\n", gai_strerror(status) );
    return 2;
  }

  for (p = res; p != NULL; p = p->ai_next)
  {
    /* get the pointer to the address itself, */
    /* different fields in IPv4 and IPv6: */
    if (p->ai_family == AF_INET)       /* IPv4 */
    {
      struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
      if (ipv4->sin_port == 0)
      {
        sockaddr_initFromIPv4Int( (struct sockaddr_in*)addr_out,
                                  ipv4->sin_addr.s_addr,
                                  htons(3478) );
      }
    }
    else         /* IPv6 */
    {
      struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)p->ai_addr;
      if (ipv6->sin6_port == 0)
      {
        sockaddr_initFromIPv6Int( (struct sockaddr_in6*)addr_out,
                                  ipv6->sin6_addr.s6_addr,
                                  htons(3478) );
      }
    }
  }

  freeaddrinfo(res);   /* free the linked list */
  return 0;
}
