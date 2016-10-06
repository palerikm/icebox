#include "media.h"

#include <stdlib.h>

#include "defines.h"

void*
mSocketListen(void* ptr)
{
  struct mediaConfig* mconfig = (struct mediaConfig*) ptr;

  struct pollfd           ufds[ICE_MAX_CANDIDATES];
  struct sockaddr_storage their_addr;
  unsigned char           buf[MAXBUFLEN];
  /* int sockfd =*(int *)ptr; */

  socklen_t addr_len;
  int       rv;
  int       numbytes;
/*  int                  i; */
  StunMessage stunMsg;
/* Populate teh sockets here.. */
  const ICE_MEDIA_STREAM* media =
    ICELIB_getLocalMediaStream(mconfig->icelib,
                               0);
  for (size_t i = 0; i < media->numberOfCandidates; i++)
  {
    ufds[i].fd     = media->candidate[i].socket;
    ufds[i].events = POLLIN;
  }
  /* ufds[0].fd     = mconfig->sigsock; */
  /* ufds[0].events = POLLIN; */

  /* ufds[1].fd     = lconfig->msock; */
  /* ufds[1].events = POLLIN; */
  (void)mconfig;
  addr_len = sizeof their_addr;

  while (1)
  {
    rv = poll(ufds, media->numberOfCandidates, -1);
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
      for (size_t i = 0; i < media->numberOfCandidates; i++)
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
          /* if (i == 0) */
          /* { */
          /*
           *  mconfig->signal_path_handler(&lconfig->prg->state,
           *                            lconfig->mconf->icelib,
           *                            ufds[0].fd,
           *                            (char*)buf,
           *                            numbytes);
           */
          /* } */

          if ( stunlib_isStunMsg(buf, numbytes) )
          {
            stunlib_DecodeMessage(buf, numbytes, &stunMsg, NULL, NULL);
            if ( stunlib_isRequest(&stunMsg) )
            {
              /* Get socket proto. */
              int       proto = IPPROTO_UDP;
              int       type;
              socklen_t length = sizeof(int);
              if ( getsockopt(ufds[i].fd, SOL_SOCKET, SO_TYPE, &type, &length) )
              {
                perror("getsockopt (Trying to get type)");
                exit(1);
              }
              if (type == SOCK_STREAM)
              {
                proto = IPPROTO_TCP;
              }
              /* get the local address of the socket. */
              struct sockaddr_storage saddr;
              length = sizeof(saddr);
              getsockname(ufds[i].fd,
                          (struct sockaddr*)&saddr,
                          &length);

              /* Do some sanity checks on the STUN msg??? */
              /* Naaah, hopefully ICELib takes care of that.. */
              if (stunMsg.hasUsername &&
                  stunMsg.hasPriority)
              {
                ICELIB_incomingBindingRequest(mconfig->icelib,
                                              0,
                                              0,
                                              stunMsg.username.value,
                                              stunMsg.priority.value,
                                              stunMsg.hasUseCandidate,
                                              stunMsg.hasControlling,
                                              stunMsg.hasControlled,
                                              stunMsg.hasControlling ? stunMsg.controlling.value : stunMsg.controlled.value,
                                              stunMsg.msgHdr.id,
                                              ufds[i].fd,
                                              proto,
                                              (struct sockaddr*)&their_addr,
                                              (struct sockaddr*)&saddr,
                                              false,
                                              NULL,
                                              1);
              }


            }
            if ( stunlib_isResponse(&stunMsg) )
            {
              StunClient_HandleIncResp(mconfig->stunInstance,
                                       &stunMsg,
                                       (struct sockaddr*)&their_addr);

            }
          }
        }
        /* } */
      }
    }
  }
}
