
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

#include <icelib.h>
#include <stunclient.h>

#include "signaling.h"
#include "sockethelper.h"
#include "candidate_harvester.h"
#include "sdp.h"
#include "media.h"

#define SERVERPORT "5061" /* the port client will be connecting to */
#define MAXBUFLEN 4096
#define MAXDATASIZE 200 /* max number of bytes we can get at once */

#define STUNPORT "3478"    /* the port users will be connecting to */


/* static const uint32_t TEST_THREAD_CTX = 1; */


struct prg_data {
  client_state        state;
  ICELIB_INSTANCE*    icelib;
  STUN_CLIENT_DATA*   stunInstance;
  struct mediaConfig* mediaCnf;
  char                user[MAX_USER_LEN];
};


struct sigListenConfig {
  struct prg_data* prg;
  /* Signaling socket.. */
  int sigsock;
  /* "Media" socket */
  /* TURN_INSTANCE_DATA* turnInst; */
  /* int                 msock; */

  /*Handles normal data like RTP etc */
  void (* signal_path_handler)(client_state*    state,
                               ICELIB_INSTANCE* pInstance,
                               int              sockfd,
                               char*,
                               int              buflen);
};

void
ICELIB_printLog(void*           pUserData,
                ICELIB_logLevel logLevel,
                const char*     str)
{
  (void)pUserData;
  (void)logLevel;
  (void)str;
  printf("%s\n", str);
}

void*
sigSocketListen(void* ptr)
{
  struct pollfd           ufds[1];
  struct sockaddr_storage their_addr;
  unsigned char           buf[MAXBUFLEN];
  /* int sockfd =*(int *)ptr; */
  struct sigListenConfig* lconfig = (struct sigListenConfig*) ptr;
  socklen_t               addr_len;
  int                     rv;
  int                     numbytes;
/*  int                  i; */
/*  StunMessage          stunMsg; */
  ufds[0].fd     = lconfig->sigsock;
  ufds[0].events = POLLIN;

  /* ufds[1].fd     = lconfig->msock; */
  /* ufds[1].events = POLLIN; */

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
      /* for (i = 0; i < 2; i++) */
      /* { */
      if (ufds[0].revents & POLLIN)
      {
        if ( ( numbytes =
                 recvfrom(ufds[0].fd, buf, MAXBUFLEN, 0,
                          (struct sockaddr*)&their_addr,
                          &addr_len) ) == -1 )
        {
          perror("recvfrom");
          exit(1);
        }
        /* Signal path */
        /* if (i == 0) */
        /* { */
        lconfig->signal_path_handler(&lconfig->prg->state,
                                     lconfig->prg->icelib,
                                     ufds[0].fd,
                                     (char*)buf,
                                     numbytes);
        /* } */
        /* if (i == 1) */
        /* { */
        /*  if ( stunlib_isStunMsg(buf, numbytes) ) */
        /*  { */
        /*    stunlib_DecodeMessage(buf, numbytes, &stunMsg, NULL, NULL); */
        /* TurnClient_HandleIncResp(lconfig->turnInst, */
        /*                         &stunMsg, */
        /*                         buf); */
        /*  } */
        /* } */
        /* } */
      }
    }
  }
}


/* static void* */
/* tickTurn(void* ptr) */
/* { */
/*  struct timespec     timer; */
/*  struct timespec     remaining; */
/*  TURN_INSTANCE_DATA* instData = (TURN_INSTANCE_DATA*)ptr; */
/*  timer.tv_sec  = 0; */
/*  timer.tv_nsec = 50000000; */
/*  */
/*  for (;; ) */
/*  { */
/*    nanosleep(&timer, &remaining); */
/*  */
/*    if (instData != NULL) */
/*    { */
/*      TurnClient_HandleTick(instData); */
/*    } */
/*  } */
/*  */
/* } */



void
sendPacket(void*                  ctx,
           int                    sockHandle,
           const uint8_t*         buf,
           int                    bufLen,
           const struct sockaddr* dstAddr,
           int                    proto,
           bool                   useRelay,
           uint8_t                ttl)
{
  int32_t numbytes;
  /* char addrStr[SOCKADDR_MAX_STRLEN]; */
  uint32_t sock_ttl;
  uint32_t addr_len;
  (void) ctx;
  (void) proto; /* Todo: Sanity check? */
  (void) useRelay;

  if (dstAddr->sa_family == AF_INET)
  {
    addr_len = sizeof(struct sockaddr_in);
  }
  else
  {
    addr_len = sizeof(struct sockaddr_in6);
  }

  if (ttl > 0)
  {
    /*Special TTL, set it send packet and set it back*/
    int          old_ttl;
    unsigned int optlen;
    if (dstAddr->sa_family == AF_INET)
    {
      getsockopt(sockHandle, IPPROTO_IP, IP_TTL, &old_ttl, &optlen);
    }
    else
    {
      getsockopt(sockHandle, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &old_ttl,
                 &optlen);
    }

    sock_ttl = ttl;

    /* sockaddr_toString(dstAddr, addrStr, SOCKADDR_MAX_STRLEN, true); */
    /* printf("Sending Raw (To: '%s'(%i), Bytes:%i/%i  (Addr size: %u)\n",
     * addrStr, sockHandle, numbytes, bufLen,addr_len); */

    if (dstAddr->sa_family == AF_INET)
    {
      setsockopt( sockHandle, IPPROTO_IP, IP_TTL, &sock_ttl, sizeof(sock_ttl) );
    }
    else
    {
      setsockopt( sockHandle, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &sock_ttl,
                  sizeof(sock_ttl) );
    }

    if ( ( numbytes =
             sendto(sockHandle, buf, bufLen, 0, dstAddr, addr_len) ) == -1 )
    {
      perror("Stun sendto");
      exit(1);
    }
    if (dstAddr->sa_family == AF_INET)
    {
      setsockopt(sockHandle, IPPROTO_IP, IP_TTL, &old_ttl, optlen);
    }
    else
    {
      setsockopt(sockHandle, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &old_ttl, optlen);
    }


  }
  else
  {
    /*Nothing special, just send the packet*/
    if ( ( numbytes =
             sendto(sockHandle, buf, bufLen, 0, dstAddr, addr_len) ) == -1 )
    {
      perror("Stun sendto");
      char addr[SOCKADDR_MAX_STRLEN];
      sockaddr_toString(dstAddr,
                        addr,
                        sizeof(addr),
                        true);
      printf("Trying to send to: %s\n", addr);
      exit(1);
    }
  }
}

static void
stunStatusCallBack(void*               ctx,
                   StunCallBackData_T* retData)
{
  (void)ctx;
  (void)retData;
  /* printf("Got STUN status callback\n");// (Result (%i)\n",
   * retData->stunResult); */
}


/* static void */
/* turnSendFunc(const uint8_t*         buffer, */
/*             size_t                 bufLen, */
/*             const struct sockaddr* dstAddr, */
/*             void*                  userCtx) */
/* { */
/*  */
/*  int sockfd = *(int*)userCtx; */
/*  sendRawStun(sockfd, buffer, bufLen, dstAddr); */
/* } */

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

ICELIB_Result
sendConnectivityCheck(void*                  pUserData,
                      int                    proto,
                      int                    socket,
                      const struct sockaddr* destination,
                      const struct sockaddr* source,
                      uint32_t               userValue1,
                      uint32_t               userValue2,
                      uint32_t               componentId,
                      bool                   useRelay,
                      const char*            pUfrag,
                      const char*            pPasswd,
                      uint32_t               peerPriority,
                      bool                   useCandidate,
                      bool                   iceControlling,
                      bool                   iceControlled,
                      uint64_t               tieBreaker,
                      StunMsgId              transactionId)
{

  (void)userValue1;
  (void)userValue2;
  (void)componentId;
  (void)iceControlled;

  struct prg_data*      prg = pUserData;
  TransactionAttributes transAttr;

  transAttr.transactionId = transactionId;
  transAttr.sockhandle    = socket;
  strncpy(transAttr.username, pUfrag,  STUN_MSG_MAX_USERNAME_LENGTH);
  strncpy(transAttr.password, pPasswd, STUN_MSG_MAX_PASSWORD_LENGTH);
  transAttr.peerPriority   = peerPriority;
  transAttr.useCandidate   = useCandidate;
  transAttr.iceControlling = iceControlling;
  transAttr.tieBreaker     = tieBreaker;

  /* kick off stun */
  int32_t ret =  StunClient_startBindTransaction(prg->stunInstance,
                                                 NULL,
                                                 destination,
                                                 source,
                                                 proto,
                                                 useRelay,
                                                 &transAttr,
                                                 sendPacket,
                                                 stunStatusCallBack);
  return ret;
}

static void*
tickStun(void* ptr)
{
  struct timespec   timer;
  struct timespec   remaining;
  STUN_CLIENT_DATA* clientData = (STUN_CLIENT_DATA*)ptr;

  timer.tv_sec  = 0;
  timer.tv_nsec = 50000000;

  for (;; )
  {
    nanosleep(&timer, &remaining);
    StunClient_HandleTick(clientData, 50);
  }
  return NULL;
}

static void*
tickIce(void* ptr)
{
  struct timespec  timer;
  struct timespec  remaining;
  ICELIB_INSTANCE* clientData = (ICELIB_INSTANCE*)ptr;

  timer.tv_sec  = 0;
  timer.tv_nsec = 20000000;

  for (;; )
  {
    nanosleep(&timer, &remaining);
    ICELIB_Tick(clientData);
  }
  return NULL;
}

/* static void */
/* turnInfoFunc(void*              userCtx, */
/*             TurnInfoCategory_T category, */
/*             char*              ErrStr) */
/* { */
/*  (void)userCtx; */
/*  (void)category; */
/*    printf("TurnInfoCB"); */
/*    printf("%s\n", ErrStr); */
/* } */

int
main(int   argc,
     char* argv[])
{
  struct prg_data        prg;
  struct sigListenConfig lconf;
  /* struct addrinfo     servinfo, * p; */

  /* Intializes random number generator */
  time_t t;
  srand( (unsigned) time(&t) );

  /* struct sockaddr_storage taddr; */
  lconf.prg = &prg;

  pthread_t sigSocketListenThread;
  pthread_t stunTickThread;
  pthread_t iceTickThread;
  /* pthread_t turnTickThread; */

  if (argc < 3)
  {
    fprintf(stderr,"usage: icebox hostname user remoteuser\n");
    exit(1);
  }

  lconf.sigsock             = createTcpSocket(argv[1], SERVERPORT);
  lconf.signal_path_handler = signalPathHandler;

  /* lconf.msock = createUdpSocket(argv[1], */
  /*                              STUNPORT, */
  /*                              "turnclient", */
  /*                              0, */
  /*                              &servinfo, */
  /*                              &p); */
  /* if (lconf.msock == -1) */
  /* { */
  /*  return 1; */
  /* } */
  /* else if (lconf.msock == -2) */
  /* { */
  /*  return 2; */
  /* } */
  pthread_create(&sigSocketListenThread, NULL, sigSocketListen, (void*)&lconf);

  strncpy(prg.user, argv[2], MAX_USER_LEN);

  registerUser(&prg.state, lconf.sigsock, prg.user);

  /* Setup ICE */
  ICELIB_CONFIGURATION iceConfig;
  iceConfig.tickIntervalMS       = 20;
  iceConfig.keepAliveIntervalS   = 15;
  iceConfig.maxCheckListPairs    = ICELIB_MAX_PAIRS;
  iceConfig.aggressiveNomination = false;
  iceConfig.iceLite              = false;
  iceConfig.logLevel             = ICELIB_logDebug;
  /* iceConfig.logLevel = ICELIB_logDisable; */

  prg.icelib = malloc( sizeof(ICELIB_INSTANCE) );

  ICELIB_Constructor(prg.icelib,
                     &iceConfig);

  ICELIB_setCallbackLog(prg.icelib,
                        ICELIB_printLog,
                        NULL,
                        ICELIB_logDebug);

  ICELIB_setCallbackOutgoingBindingRequest(prg.icelib,
                                           sendConnectivityCheck,
                                           &prg);
  /* Setup stun */
  StunClient_Alloc(&prg.stunInstance);

  /* Signal path set up, time to gather the candidates */
  /* Turn setup */



  /* if ( 0 != getSockaddrFromFqdn( (struct sockaddr*)&taddr, argv[1] ) ) */
  /* { */
  /*  printf("Error getting TURN Server IP\n"); */
  /*  return 1; */
  /* } */

  /* TurnClient_StartAllocateTransaction(&lconf.turnInst, */
  /*                                    50, */
  /*                                    turnInfoFunc, */
  /*                                    "icebox", */
  /*                                      &lconf.msock, / * *userCtx * / */
  /*                                    (struct sockaddr*)&taddr, */
  /* /                                  "test\0", / *argv[2],* / */
  /*                                "pass\0", / *argv[2],* / */
  /*                              AF_INET, */
  /*                            turnSendFunc, */
  /*                          turnCbFunc, */
  /*                        false, */
  /*                      0); */


  /* pthread_create(&turnTickThread, NULL, tickTurn, (void*)lconf.turnInst); */
  pthread_create(&stunTickThread, NULL, tickStun, (void*)prg.stunInstance);
  pthread_create(&iceTickThread,  NULL, tickIce,  (void*)prg.icelib);

  if (argc == 4)
  {
    /* Ok got nothing better to do. Busywaiting */
    while (prg.state != REGISTERED)
    {
    }

    char* sdp = NULL;
    harvestAndCreateSDP(prg.icelib, &sdp);
    inviteUser(&prg.state, lconf.sigsock, argv[3], prg.user, sdp);

    if (sdp != NULL)
    {
      free(sdp);
    }

  }


  /* Just wait a bit for now.. */
  printf("About to sleep\n");
  sleep(100);
  close(lconf.sigsock);
  StunClient_free(prg.stunInstance);

  return 0;
}
