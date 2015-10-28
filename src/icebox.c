
/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <turnclient.h>
#include <sockaddr_util.h>

#include "signaling.h"
#include "sockethelper.h"

#define SERVERPORT "5061" /* the port client will be connecting to */
#define MAXBUFLEN 1024
#define MAXDATASIZE 200 /* max number of bytes we can get at once */

#define STUNPORT "3478"    /* the port users will be connecting to */

/* static const uint32_t TEST_THREAD_CTX = 1; */

struct listenConfig {
  struct prg_data* prg;
  /* Signaling socket.. */
  int sigsock;
  /* "Media" socket */
  TURN_INSTANCE_DATA* turnInst;
  int                 msock;

  /*Handles normal data like RTP etc */
  void (* signal_path_handler)(client_state* state,
                               int           sockfd,
                               unsigned char *);
};



void*
socketListen(void* ptr)
{
  struct pollfd           ufds[2];
  struct sockaddr_storage their_addr;
  unsigned char           buf[MAXBUFLEN];
  /* int sockfd =*(int *)ptr; */
  struct listenConfig* lconfig = (struct listenConfig*) ptr;
  socklen_t            addr_len;
  int                  rv;
  int                  numbytes;
  int                  i;
  StunMessage          stunMsg;
  ufds[0].fd     = lconfig->sigsock;
  ufds[0].events = POLLIN;

  ufds[1].fd     = lconfig->msock;
  ufds[1].events = POLLIN;

  addr_len = sizeof their_addr;

  while (1)
  {
    rv = poll(ufds, 2, -1);
    if (rv == -1)
    {
      perror("poll");       /* error occurred in poll() */
    }
    else if (rv == 0)
    {
      printf("Timeout occurred! (Should not happen)\n");
    }
    else
    {
      /* check for events on s1: */
      for (i = 0; i < 2; i++)
      {
        if (ufds[i].revents & POLLIN)
        {
          if ( ( numbytes =
                   recvfrom(ufds[i].fd, buf, MAXBUFLEN, 0,
                            (struct sockaddr*)&their_addr,
                            &addr_len) ) == -1 )
          {
            perror("recvfrom");
            exit(1);
          }
          /* Signal path */
          if (i == 0)
          {
            lconfig->signal_path_handler(&lconfig->prg->state,
                                         ufds[i].fd,
                                         buf);
          }
          if (i == 1)
          {
            if ( stunlib_isStunMsg(buf, numbytes) )
            {
              stunlib_DecodeMessage(buf, numbytes, &stunMsg, NULL, NULL);
              TurnClient_HandleIncResp(lconfig->turnInst,
                                       &stunMsg,
                                       buf);
            }
          }
        }
      }
    }
  }
}


static void*
tickTurn(void* ptr)
{
  struct timespec     timer;
  struct timespec     remaining;
  TURN_INSTANCE_DATA* instData = (TURN_INSTANCE_DATA*)ptr;
  timer.tv_sec  = 0;
  timer.tv_nsec = 50000000;

  for (;; )
  {
    nanosleep(&timer, &remaining);

    if (instData != NULL)
    {
      TurnClient_HandleTick(instData);
    }
  }

}

void
sendRawStun(int                    sockHandle,
            const uint8_t*         buf,
            int                    bufLen,
            const struct sockaddr* dstAddr)
{
  int  numbytes;
  char addrStr[SOCKADDR_MAX_STRLEN];

  if ( ( numbytes =
           sendto( sockHandle, buf, bufLen, 0, dstAddr,
                   sizeof(*dstAddr) ) ) == -1 )
  {
    perror("sendRawStun: sendto");
    exit(1);
  }

  sockaddr_toString(dstAddr, addrStr, SOCKADDR_MAX_STRLEN, true);
  /* printf("Sending Raw (To: '%s'(%i), Bytes:%i/%i  )\n", addrStr, sockHandle,
   * numbytes, bufLen); */

  /* return numbytes; */
}

static void
turnSendFunc(const uint8_t*         buffer,
             size_t                 bufLen,
             const struct sockaddr* dstAddr,
             void*                  userCtx)
{

  int sockfd = *(int*)userCtx;
  sendRawStun(sockfd, buffer, bufLen, dstAddr);
}

void
turnCbFunc(void*               userCtx,
           TurnCallBackData_T* turnCbData)
{
  /* ctx points to whatever you initialized the library with. (Not used in this
   * simple example.) */
  (void)userCtx;
  if (turnCbData->turnResult == TurnResult_AllocOk)
  {
    char addr[SOCKADDR_MAX_STRLEN];

    printf( "Successfull Allocation: \n");

    printf( "   Active TURN server: '%s'\n",
            sockaddr_toString( (struct sockaddr*)&turnCbData->TurnResultData.
                               AllocResp.activeTurnServerAddr,
                               addr,
                               sizeof(addr),
                               true ) );

    printf( "   RFLX addr: '%s'\n",
            sockaddr_toString( (struct sockaddr*)&turnCbData->TurnResultData.
                               AllocResp.srflxAddr,
                               addr,
                               sizeof(addr),
                               true ) );

    printf( "   RELAY addr: '%s'\n",
            sockaddr_toString( (struct sockaddr*)&turnCbData->TurnResultData.
                               AllocResp.relAddrIPv4,
                               addr,
                               sizeof(addr),
                               true ) );


    printf( "   RELAY addr: '%s'\n",
            sockaddr_toString( (struct sockaddr*)&turnCbData->TurnResultData.
                               AllocResp.relAddrIPv6,
                               addr,
                               sizeof(addr),
                               true ) );

  }
  else if (turnCbData->turnResult == TurnResult_AllocUnauthorised)
  {
    printf("Unable to authorize. Wrong user/pass?\n");
  }
}


static void
turnInfoFunc(void*              userCtx,
             TurnInfoCategory_T category,
             char*              ErrStr)
{
  (void)userCtx;
  (void)category;
    printf("TurnInfoCB");
    printf("%s\n", ErrStr);
}

int
main(int   argc,
     char* argv[])
{
  struct prg_data     prg;
  struct listenConfig lconf;
  struct addrinfo     servinfo, * p;


  struct sockaddr_storage taddr;
  lconf.prg = &prg;

  pthread_t socketListenThread;
  pthread_t turnTickThread;

  if (argc < 3)
  {
    fprintf(stderr,"usage: icebox hostname user remoteuser\n");
    exit(1);
  }

  lconf.sigsock             = createTcpSocket(argv[1], SERVERPORT);
  lconf.signal_path_handler = signalPathHandler;

  lconf.msock = createUdpSocket(argv[1],
                                STUNPORT,
                                "turnclient",
                                0,
                                &servinfo,
                                &p);
  if (lconf.msock == -1)
  {
    return 1;
  }
  else if (lconf.msock == -2)
  {
    return 2;
  }
  pthread_create(&socketListenThread, NULL, socketListen, (void*)&lconf);

  strncpy(prg.user, argv[2], MAX_USER_LEN);

  registerUser(&prg.state, lconf.sigsock, prg.user);

  /* Signal path set up, time to gather the candidates */
  /* Turn setup */



  if ( 0 != getSockaddrFromFqdn( (struct sockaddr*)&taddr, argv[1] ) )
  {
    printf("Error getting TURN Server IP\n");
    return 1;
  }

    printf("Starting TURN party!\n");

  TurnClient_StartAllocateTransaction(&lconf.turnInst,
                                      50,
                                      turnInfoFunc,
                                      "icebox",
                                      &lconf.msock, /* *userCtx */
                                      (struct sockaddr*)&taddr,
                                      "test\0", /*argv[2],*/
                                      "pass\0", /*argv[2],*/
                                      AF_INET,
                                      turnSendFunc,
                                      turnCbFunc,
                                      false,
                                      0);


  pthread_create(&turnTickThread, NULL, tickTurn, (void*)lconf.turnInst);


  if (argc == 4)
  {
    /* Ok got nothing better to do. Busywaiting */
    while (prg.state != REGISTERED)
    {
    }
    inviteUser(&prg.state, lconf.sigsock, argv[3], prg.user);
  }

  /* Just wait a bit for now.. */
  printf("About to sleep\n");
  sleep(100);
  close(lconf.sigsock);

  return 0;
}
