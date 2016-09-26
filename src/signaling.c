
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <ctype.h>
#include <stdlib.h>

#include "sdp.h"
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
  int  numbytes;
  char str[4096];
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
  printf("Sending invite\n%s\n", str);
  if ( ( numbytes = send(sockfd, str,strlen(str), 0) ) == -1 )
  {
    perror("inviteUser: send");
    return -1;
  }
  printf("Sendt: %i\n", numbytes);
  return 1;
}

void
harvestAndCreateSDP(ICELIB_INSTANCE* icelib,
                    char**           sdp)
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
  for (int i = 0; i < udp_cand_len; i++)
  {
    /* TODO: set local pref based on iface */
    ICELIB_addLocalCandidate(icelib,
                             mediaidx,
                             1,
                             (struct sockaddr*)&udp_cand[i].ice.connectionAddr,
                             NULL,
                             udp_cand->ice.transport,
                             udp_cand->ice.type,
                             0xffff);
  }

  for (int i = 0; i < tcp_cand_len; i++)
  {
    /* TODO: set local pref based on iface? */
    ICELIB_addLocalCandidate(icelib,
                             mediaidx,
                             1,
                             (struct sockaddr*)&tcp_cand[i].ice.connectionAddr,
                             NULL,
                             tcp_cand->ice.transport,
                             tcp_cand->ice.type,
                             0xffff);
  }
  /* Info is now stored in icelib and local struct.. Fix? */

  const ICE_MEDIA_STREAM* media =
    ICELIB_getLocalMediaStream(icelib,
                               mediaidx);

  size_t sdpSize = 4096;
  *sdp = calloc(sizeof(char), sdpSize);

  if (sdpCandCat(*sdp, &sdpSize,
                 media->candidate, media->numberOfCandidates) == 0)
  {
    printf("Failed to write candidates to SDP. Too small buffer?\n");
  }
  /* if (sdpCandCat(sdp, &sdpSize, */
  /*               tcp_cand, tcp_cand_len) == 0) */
  /* { */
  /*  printf("Failed to write TCP candidates to SDP. Too small buffer?\n"); */
  /* } */
  free(udp_cand);
  free(tcp_cand);
  /* printf("SDP:\n%s\n", sdp); */
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
    int  numbytes;
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
          parseSDP(icelib, message + (len - sdp_len), len);
          /* Ok, so now we send our local candidate back in the 200 Ok */
          const ICE_MEDIA_STREAM* media =
            ICELIB_getLocalMediaStream(icelib,
                                       0);

          size_t sdpSize = 4096;
          sdp = calloc(sizeof(char), sdpSize);

          if (sdpCandCat(sdp, &sdpSize,
                         media->candidate, media->numberOfCandidates) == 0)
          {
            printf("Failed to write candidates to SDP. Too small buffer?\n");
          }

        }
      }
    }
    strlcat( str, "200 OK\nTo: ", sizeof(str) );
    strlcat( str, str_to,         sizeof(str) );
    strlcat( str, "\nFrom: ",     sizeof(str) );
    strlcat( str, str_from,       sizeof(str) );
    if (sdp != NULL)
    {
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
    }

    printf("Sending 200OK\n%s\n", str);

    if ( ( numbytes = send(sockfd, str,strlen(str), 0) ) == -1 )
    {
      perror("200Ok: send");
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
  char* sdp   = NULL;
  if (strncmp(tok, "200 OK", 6) == 0)
  {
    int  numbytes;
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
          parseSDP(icelib, message + (len - sdp_len), len);


        }
      }
    }
    strlcat( str, "200 OK\nTo: ", sizeof(str) );
    strlcat( str, str_to,         sizeof(str) );
    strlcat( str, "\nFrom: ",     sizeof(str) );
    strlcat( str, str_from,       sizeof(str) );
    if (sdp != NULL)
    {
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
    }

    printf("Sending 200OK\n%s\n", str);

    if ( ( numbytes = send(sockfd, str,strlen(str), 0) ) == -1 )
    {
      perror("200Ok: send");
    }
  }
}

void
signalPathHandler(client_state*    state,
                  ICELIB_INSTANCE* icelib,
                  int              sockfd,
                  char*            message,
                  int              len)
{

  char* delim = "\n:\\";
  char* tok   = strtok( (char*)message, delim );

  if (*state == REGISTERING)
  {
    if (strncmp(tok, "200 OK", 6) == 0)
    {
      printf("Registered\n");
      *state = REGISTERED;
    }
    else
    {
      printf("Registration failed with %s\n", message);
    }
  }
  if (*state == INVITING)
  {
    if (strncmp(tok, "100 Trying", 6) == 0)
    {
      printf("Invitation recieved at notice server\n");
      return;
    }
    if (strncmp(tok, "200 OK", 6) == 0)
    {
      printf("Got a 200 OK (Session Established..)\n");

    }
    else
    {
      printf("Invitation failed with %s\n", message);
    }
  }
  if (*state == REGISTERED)
  {
    char* sdp = NULL;
    harvestAndCreateSDP(icelib, &sdp);
    handleOffer(icelib, sockfd, message, len, tok);
    if (sdp != NULL)
    {
      free(sdp);
    }
  }
}
