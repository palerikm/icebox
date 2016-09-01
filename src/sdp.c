

#include <sockaddr_util.h>
#include <stdlib.h>
#include <string.h>

#include <icelibtypes.h>

#include "sdp.h"


char*
sdp_candidate_toString(const ICE_CANDIDATE* cand)
{
  char* x = NULL;
  char  addr_str[SOCKADDR_MAX_STRLEN];
  asprintf(&x, "a=candidate:%s 1 %s %i %s %i %s",
           cand->foundation,
           ICELIBTYPES_ICE_TRANSPORT_PROTO_toString(cand->transport),
           cand->priority,
           sockaddr_toString( (struct sockaddr*)&cand->connectionAddr,
                              addr_str,
                              sizeof(addr_str),
                              false ),
           sockaddr_ipPort( (struct sockaddr*)&cand->connectionAddr ),
           ICELIBTYPES_ICE_CANDIDATE_TYPE_toString(cand->type)
           );
  return x;
}

char*
sdp_cline_toString(const ICE_CANDIDATE* cand)
{
  char* x = NULL;
  char  addr_str[SOCKADDR_MAX_STRLEN];
  char  addr_type[4];
  switch ( ( (struct sockaddr*)&cand->connectionAddr )->sa_family )
  {
  case AF_INET:
    strncpy(addr_type, "IP4", 3);
    break;
  case AF_INET6:
    strncpy(addr_type, "IP6", 3);
    break;
  }
  asprintf(&x, "c=IN %s %s",
           addr_type,
           sockaddr_toString( (struct sockaddr*)&cand->connectionAddr,
                              addr_str,
                              sizeof(addr_str),
                              false )
           );
  return x;
}

size_t
sdpCandCat(char*                sdp,
           size_t*              sdpSize,
           const ICE_CANDIDATE* cand,
           size_t               numcand)
{

  size_t sdpEnd = strlen(sdp) + 1;
  size_t num    = 0;

  /* Add C line */
  char* c_line = sdp_cline_toString(&cand[0]);
  if (c_line != NULL)
  {
    size_t cline_len = strlen(c_line);
    if (sdpEnd + cline_len > *sdpSize)
    {
      return 0;
    }
    /* memcpy(sdp[sdpEnd], sdp_cand, candLen +1); */
    strcat(sdp, c_line);
    strcat(sdp, "\n");
    free(c_line);
    num += cline_len;
  }

  for (size_t i = 0; i < numcand; i++)
  {

    /* printhCandidate(&udp_cand[i]); */
    char* sdp_cand = sdp_candidate_toString(&cand[i]);
    if (sdp_cand != NULL)
    {
      size_t cand_len = strlen(sdp_cand);
      if (sdpEnd + cand_len > *sdpSize)
      {
        return 0;
      }
      /* memcpy(sdp[sdpEnd], sdp_cand, candLen +1); */
      strcat(sdp, sdp_cand);
      strcat(sdp, "\n");
      free(sdp_cand);
      num += cand_len;
    }
  }
  return num;
}


void
parseAttr(ICELIB_INSTANCE* icelib,
          char*            attr,
          int              mediaidx)
{
  ICE_CANDIDATE ice_cand;
  int           i = 0;
  while (attr != NULL)
  {
    /* Break up the candidate line */
    /* first is the foundation..Should be a string after ":"" */
    switch (i)
    {
    case fond:
      strncpy(ice_cand.foundation, strchr(attr, ':') + 1, 32);
      break;
    case compid:
      ice_cand.componentid = atoi(attr);
      break;
    case trnsp:
      if (strcmp(attr, "UDP") == 0)
      {
        ice_cand.transport = ICE_TRANS_UDP;
      }
      if (strcmp(attr, "TCP") == 0)
      {
        ice_cand.transport = ICE_TRANS_TCPACT;
      }
      break;
    case priority:
      ice_cand.priority = atoi(attr);
      break;
    case addr:
      sockaddr_initFromString( (struct sockaddr*)&ice_cand.connectionAddr,
                               attr );
      break;
    case port:
      sockaddr_setPort( (struct sockaddr*)&ice_cand.connectionAddr,
                        atoi(attr) );
      break;
    case type:
      if (strcmp(attr, "host") == 0)
      {
        ice_cand.type = ICE_CAND_TYPE_HOST;

      }
      break;
    default:
      printf(" Attr Rest (%i): %s\n", i, attr);
    }
    attr = strtok(NULL, " ");
    i++;
  }

  char ipaddr_str[SOCKADDR_MAX_STRLEN];
  sockaddr_toString( (struct sockaddr*)&ice_cand.connectionAddr,
                     ipaddr_str,
                     SOCKADDR_MAX_STRLEN,
                     false );

  int32_t res = ICELIB_addRemoteCandidate(icelib,
                                          mediaidx,
                                          ice_cand.foundation,
                                          strlen(ice_cand.foundation),
                                          ice_cand.componentid,
                                          ice_cand.priority,
                                          ipaddr_str,
                                          sockaddr_ipPort( (struct sockaddr*)&
                                                           ice_cand.
                                                           connectionAddr ),
                                          ice_cand.transport,
                                          ice_cand.type);
  if (res == -1)
  {
    printf("Failed to add Remote candidates..\n");
  }

}


void
parseCline(ICELIB_INSTANCE* icelib,
           char*            cline,
           int32_t*         mediaidx)
{
  /* ICE_CANDIDATE ice_cand; */
  struct sockaddr_storage ss;
  int                     i = 0;
  while (cline != NULL)
  {
    switch (i)
    {
    case nettype:
      /* Ignore */
      break;
    case addrtype:
      break;
    case clineaddr:
      sockaddr_initFromString( (struct sockaddr*)&ss,
                               cline );
      break;
    default:
      printf(" Cline Rest (%i): %s\n", i, cline);
    }
    cline = strtok(NULL, " ");
    i++;
  }

  *mediaidx = ICELIB_addRemoteMediaStream(icelib,
                                          "icebox\0",
                                          "icebox\0",
                                          (struct sockaddr*)&ss);
}



void
parseSDP(ICELIB_INSTANCE* icelib,
         char*            sdp,
         int              len)
{
  (void)len;
  int32_t mediaidx = -1;
  printf("About to parse SDP---\n");

  char*      string = sdp;
  const char delim  = '\n';
  char*      line   = strchr(string, delim);
  while (line != NULL)
  {
    *line++ = '\0';
    char* attr = strtok(string, " \n");

    if (strncmp(attr, "c=IN", 4) == 0)
    {
      parseCline(icelib,
                 attr,
                 &mediaidx);
    }
    if (strncmp(attr, "a=candidate", 11) == 0)
    {
      if (mediaidx != -1)
      {
        parseAttr(icelib,
                  attr,
                  mediaidx);
      }
    }
    string = line;
    line   = strchr(string, delim);
  }

}
