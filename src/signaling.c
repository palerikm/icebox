#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#ifdef linux
#include <bsd/string.h>
#endif
#include <sys/socket.h>
#include <ctype.h>
#include <stdlib.h>

#include "sdp.h"
#include "json.h"

#include "sockethelper.h"
#include "signaling.h"

int
registerUser(client_state* state,
             int           sockfd,
             char*         user_reg)
{
  int  numbytes;
  char str[255];

  printf("Registering: %s\n", user_reg);
  *state = REGISTERING;

  strncpy(str,"REGISTER ", 255);
  strlcat(str, user_reg, 245);
  strlcat(str, "\n",     245);
  if ( ( numbytes = send(sockfd, str,strlen(str), 0) ) == -1 )
  {
    perror("registerUser: send");
    return -1;
  }
  return 1;
}


int
NumDigits(int x)
{
  /* x = abs(x); */
  return ( x < 10 ? 1 :
           ( x < 100 ? 2 :
             ( x < 1000 ? 3 :
               ( x < 10000 ? 4 :
                 ( x < 100000 ? 5 :
                   ( x < 1000000 ? 6 :
                     ( x < 10000000 ? 7 :
                       ( x < 100000000 ? 8 :
                         (x < 1000000000 ? 9 :
                          10) ) ) ) ) ) ) ) );
}

int
inviteUser(client_state* state,
           int           sockfd,
           char*         to,
           char*         from,
           char*         sdp)
{
  /* int  numbytes; */
  char str[8000];
  memset(str, 0, sizeof str);

  if (*state != REGISTERED)
  {
    return -1;
  }

  *state = INVITING;

  strlcat( str, "INVITE ",                         sizeof(str) );
  strlcat( str, to,                                sizeof(str) );
  strlcat( str, "\n",                              sizeof(str) );
  strlcat( str, "To: ",                            sizeof(str) );
  strlcat( str, to,                                sizeof(str) );
  strlcat( str, "\nFrom: ",                        sizeof(str) );
  strlcat( str, from,                              sizeof(str) );
  strlcat( str, "\n",                              sizeof(str) );
  strlcat( str, "Content-Type: application/sdp\n", sizeof(str) );
  strlcat( str, "Content-Length: ",                sizeof(str) );
  int  len = strlen(sdp);
  char len_str[NumDigits(len)];
  sprintf(len_str, "%d", len);

  strlcat( str, len_str, sizeof(str) );
  strlcat( str, "\n",    sizeof(str) );
  strlcat( str, "\r\n",  sizeof(str) );

  strlcat( str, sdp,     sizeof(str) );
  /* strlcat( str, "\n",    sizeof(str) ); */
  printf("Sending invite\n");
  int msglen = strlen(str);
  if (sendall(sockfd, str, &msglen) == -1)
  {
    perror("sendall");
    printf("We only sent %d bytes because of the error!\n", msglen);
  }
  return 1;
}

void
harvest(ICELIB_INSTANCE* icelib)
{
  /* Harvest candidates.. */
  struct hcand* udp_cand     = NULL;
  int32_t       udp_cand_len = harvest_host(&udp_cand, SOCK_DGRAM);

  struct hcand* tcp_cand     = NULL;
  int32_t       tcp_cand_len = harvest_host(&tcp_cand, SOCK_STREAM);

  /* Add the candidates to ICELIB */
  uint32_t mediaidx = ICELIB_addLocalMediaStream(icelib,
                                                 42,
                                                 42,
                                                 ICE_CAND_TYPE_HOST);

  uint16_t local_pri = 0xffff;
  for (int i = 0; i < udp_cand_len; i++)
  {
    uint16_t penalty = 0;
    if (udp_cand[i].ice.connectionAddr.ss_family == AF_INET)
    {
      penalty = 1000;
    }
    /* TODO: set local pref based on iface */
    ICELIB_addLocalCandidate(icelib,
                             mediaidx,
                             1,
                             udp_cand[i].sockfd,
                             (struct sockaddr*)&udp_cand[i].ice.connectionAddr,
                             NULL,
                             udp_cand[i].ice.transport,
                             udp_cand[i].ice.type,
                             local_pri - penalty - i);
  }

  for (int i = 0; i < tcp_cand_len; i++)
  {
    uint16_t penalty = 0;
    if (tcp_cand[i].ice.connectionAddr.ss_family == AF_INET)
    {
      penalty = 1000;
    }
    /* TODO: set local pref based on iface? */
    ICELIB_addLocalCandidate(icelib,
                             mediaidx,
                             1,
                             tcp_cand[i].sockfd,
                             (struct sockaddr*)&tcp_cand[i].ice.connectionAddr,
                             NULL,
                             tcp_cand[i].ice.transport,
                             tcp_cand[i].ice.type,
                             local_pri - penalty - i);
  }
  free(udp_cand);
  free(tcp_cand);
}

void
handleOffer(ICELIB_INSTANCE* icelib,
            int              sockfd,
            char*            message,
            int              len,
            char*            tok)
{
  char* delim = "\n:\\";
  char* sdp   = NULL;
  if (strncmp(tok, "INVITE", 6) == 0)
  {
    /* int  numbytes; */
    char str[8100];
    char str_to[128];
    char str_from[128];
    int  i;

    /* Worst parser ever, works by accident */
    for (i = 0; i < 5; i++)
    {
      tok = strtok(NULL, delim);
      if (strcmp(tok, "To") == 0)
      {
        tok = strtok(NULL, delim);
        while ( isspace(*tok) )
        {
          tok++;
        }
        strncpy( str_from, tok, strlen(tok) );
      }
      if (strcmp(tok, "From") == 0)
      {
        tok = strtok(NULL, delim);
        while ( isspace(*tok) )
        {
          tok++;
        }
        strncpy( str_to, tok, strlen(tok) );
      }
      if (strcmp(tok, "Content-Length") == 0)
      {
        tok = strtok(NULL, delim);
        while ( isspace(*tok) )
        {
          tok++;
          int sdp_len = atoi(tok);
          /* Maybe some more sanyty cheking? */
          /* This does not work with partial messages and so on.. */
          /* parseSDP(icelib, message + (len - sdp_len), sdp_len); */
          parseCandidateJson(icelib,message + (len - sdp_len), sdp_len);

          /* Ok, so now we send our local candidate back in the 200 Ok */
          size_t sdpSize = 8000;
          sdp = calloc(sizeof(char), sdpSize);

          harvest(icelib);
          printf("Completed harvesting\n");
          crateCandidateJson(icelib, &sdp);
        }
      }
    }
    strlcat( str, "200 OK\r\nTo: ", sizeof(str) );
    strlcat( str, str_to,           sizeof(str) );
    strlcat( str, "\r\nFrom: ",     sizeof(str) );
    strlcat( str, str_from,         sizeof(str) );
    if (sdp != NULL)
    {
      strlcat( str, "\r\n",                            sizeof(str) );
      strlcat( str, "Content-Type: application/sdp\n", sizeof(str) );
      strlcat( str, "Content-Length: ",                sizeof(str) );
      int  len = strlen(sdp);
      char len_str[NumDigits(len)];
      sprintf(len_str, "%d", len);
      strlcat( str, len_str, sizeof(str) );
      strlcat( str, "\r\n",  sizeof(str) );
      strlcat( str, "\r\n",  sizeof(str) );
      strlcat( str, sdp,     sizeof(str) );
    }

    printf("Sending 200OK\n%s\n", str);
    int len = strlen(str);
    if (sendall(sockfd, str, &len) == -1)
    {
      perror("sendall");
      printf("We only sent %d bytes because of the error!\n", len);
    }
  }
}

void
handleAnswer(ICELIB_INSTANCE* icelib,
             int              sockfd,
             char*            message,
             int              len,
             char*            tok)
{
  char* delim = "\n:\\";
  /* char* sdp   = NULL; */
  if (strncmp(tok, "200 OK", 6) == 0)
  {
    /* int  numbytes; */
    char str[4096];
    char str_to[128];
    char str_from[128];
    int  i;

    /* Worst parser ever, works by accident */
    for (i = 0; i < 5; i++)
    {
      tok = strtok(NULL, delim);
      if (strcmp(tok, "To") == 0)
      {
        tok = strtok(NULL, delim);
        while ( isspace(*tok) )
        {
          tok++;
        }
        strncpy( str_from, tok, strlen(tok) );
      }
      if (strcmp(tok, "From") == 0)
      {
        tok = strtok(NULL, delim);
        while ( isspace(*tok) )
        {
          tok++;
        }
        strncpy( str_to, tok, strlen(tok) );
      }
      if (strcmp(tok, "Content-Length") == 0)
      {
        tok = strtok(NULL, delim);
        while ( isspace(*tok) )
        {
          tok++;
          int sdp_len = atoi(tok);
          /* Maybe some more sanyty cheking? */
          /* This does not work with partial messages and so on.. */
          /* parseSDP(icelib, message + (len - sdp_len), sdp_len); */
          parseCandidateJson(icelib,message + (len - sdp_len), sdp_len);
        }
      }
    }
    strlcat( str, "ACK\r\n",    sizeof(str) );
    strlcat( str, str_to,       sizeof(str) );
    strlcat( str, "\r\nFrom: ", sizeof(str) );
    strlcat( str, str_from,     sizeof(str) );


    printf("Sending ACK\n%s\n", str);
    int len = strlen(str);
    if (sendall(sockfd, str, &len) == -1)
    {
      perror("sendall");
      printf("We only sent %d bytes because of the error!\n", len);
    }
  }
}

bool
completeMessage(const char* msg,
                size_t      len)
{
  /* Is there A content length after /r/n? */
  /* If so can we hold the entire message or do we need to wait? */
  (void)len;
  char* data = NULL;
  data = strstr(msg, "\r\n");
  if (data != NULL)
  {
    char* content = NULL;
    content = strstr(msg, "Content-Length:");
    if (content != NULL)
    {
      int value = atoi(content + 15);
      int real  = strlen(data + 2);
      //printf("Content-Length: %i  (%i)\n", value, real);
      if (real < value)
      {
        return false;
      }
      return true;
    }
    return true;
  }
  printf("No data.. \n%s\n", msg);
  return false;
}

void
signalPathHandler(struct sig_data*    sData,
                  struct mediaConfig* mconf,
                  int                 sockfd,
                  char*               msg,
                  int                 len)
{
  if (sData->msgBufSize > 0)
  {
    char* tmp = reallocf(sData->msgBuf, len + sData->msgBufSize);

    if (!tmp)
    {
      printf("Can not allocate memory for incomming message\n");
      exit(1);
    }
    /* printf("A\n"); */
    sData->msgBuf = tmp;
    /* sData->msgBufSize+=len; */

  }
  else
  {
    //printf("About to malloc(%p): %i\n", (void*)sData->msgBuf,len);
    sData->msgBuf = malloc(len);
  }
  memcpy(sData->msgBuf + sData->msgBufSize, msg, len);
  sData->msgBufSize += len;

  /* Do we have a complete message? */
  if ( !completeMessage(sData->msgBuf, sData->msgBufSize) )
  {
    return;
  }

  char* delim = "\n:\\";
  char* tok   = strtok(sData->msgBuf, delim);

  if (sData->state == REGISTERING)
  {
    if (strncmp(tok, "200 OK", 6) == 0)
    {
      printf("Registered\n");
      sData->state = REGISTERED;
    }
    else
    {
      printf("Registration failed with %s\n", sData->msgBuf);
    }
  }
  if (sData->state == INVITING)
  {
    if (strncmp(tok, "100 Trying", 10) == 0)
    {
      printf("Invitation recieved at notice server\n");
      /* return; */
    }
    else
    if (strncmp(tok, "200 OK", 6) == 0)
    {
      printf("Got a 200 OK (Session Established..)\n");
      handleAnswer(mconf->icelib, sockfd, sData->msgBuf, sData->msgBufSize,
                   tok);
      ICELIB_Start(mconf->icelib, false);
    }
    else
    {
      printf("Invitation failed with %s\n", sData->msgBuf);
    }
  }
  if (sData->state == REGISTERED)
  {
    if (strncmp(tok, "INVITE", 6) == 0)
    {
      /* char* sdp = NULL; */
      harvest(mconf->icelib);
      /* Start to listen here... */
      pthread_create(&mconf->mSocketListenThread,
                     NULL,
                     mSocketListen,
                     (void*)mconf);
      printf("Handling Offer\n");
      handleOffer(mconf->icelib, sockfd, sData->msgBuf, sData->msgBufSize, tok);
      printf("Starting ICE\n");
      ICELIB_Start(mconf->icelib, true);
    }
  }

  if (!sData->msgBuf)
  {
    printf("Freeing msgbuf\n");
    fflush(stdout);
    free(sData->msgBuf);
    sData->msgBuf = NULL;
  }
  sData->msgBufSize = 0;
}
