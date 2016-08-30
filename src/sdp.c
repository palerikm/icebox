

#include <sockaddr_util.h>
#include <stdlib.h>
#include <string.h>

#include <icelibtypes.h>

#include "sdp.h"


char*
sdp_candidate_toString(ICE_CANDIDATE* cand)
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

size_t
sdpCandCat(char*         sdp,
           size_t*       sdpSize,
           struct hcand* cand,
           size_t        candLen)
{

  size_t sdpEnd = strlen(sdp) + 1;
  size_t num    = 0;
  for (size_t i = 0; i < candLen; i++)
  {
    /* printhCandidate(&udp_cand[i]); */
    char* sdp_cand = sdp_candidate_toString(&cand[i].ice);
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
parseSDP(char* sdp,
         int   len)
{
  (void)len;
  printf("About to parse SDP---\n");
  /* for (int i = 0; i < len; i++) */
  /* { */
  /*  printf("%c", sdp[i]); */
  /* } */
  /* Find the candidates.. */
  char*      string = sdp;
  const char delim  = '\n';
  char*      line   = strchr(string, delim);
  while (line != NULL)
  {
    ICE_CANDIDATE ice_cand;
    /* printf( " %s\n", tok ); */
    *line++ = '\0';
    char* cand = strtok(string, " \n");
    int   i    = 0;
    while (cand != NULL)
    {
      /* Break up the candidate line */
      /* first is the foundation..Should be a string after ":"" */

      switch (i)
      {
      case fond:
        strncpy(ice_cand.foundation, strchr(cand, ':') + 1, 32);
        break;
      case compid:
        ice_cand.componentid = atoi(cand);
        break;
      case trnsp:
        if (strcmp(cand, "UDP") == 0)
        {
          ice_cand.transport = ICE_TRANS_UDP;
        }
        if (strcmp(cand, "TCP") == 0)
        {
          ice_cand.transport = ICE_TRANS_TCPACT;
        }
        break;
      case priority:
        ice_cand.priority = atoi(cand);
        break;
      case addr:
        sockaddr_initFromString( (struct sockaddr*)&ice_cand.connectionAddr,
                                 cand );
        break;
      case port:
        sockaddr_setPort( (struct sockaddr*)&ice_cand.connectionAddr,
                          atoi(cand) );
        break;

      case type:
        if (strcmp(cand, "host") == 0)
        {
          ice_cand.type = ICE_CAND_TYPE_HOST;

        }
        break;
      default:
        printf(" Rest (%i): %s\n", i, cand);
      }




      cand = strtok(NULL, " ");
      i++;
    }
    char* cand_str = sdp_candidate_toString(&ice_cand);
    printf("%s\n", cand_str);
    free(cand_str);
    string = line;
    line   = strchr(string, delim);
  }

}
