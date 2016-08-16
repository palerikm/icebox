

#include <sockaddr_util.h>
#include <stdlib.h>
#include <string.h>

#include "sdp.h"


char*
sdp_candidate_toString(ICE_CANDIDATE* cand)
{
  char* x = NULL;
  char  addr_str[SOCKADDR_MAX_STRLEN];
  asprintf(&x, "a=candidate:%s 1 %s %i %s %i %s\n",
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
    char*  sdp_cand = sdp_candidate_toString(&cand[i].ice);
    size_t cand_len = strlen(sdp_cand);
    if (sdpEnd + cand_len > *sdpSize)
    {
      return 0;
    }
    /* memcpy(sdp[sdpEnd], sdp_cand, candLen +1); */
    strcat(sdp, sdp_cand);
    free(sdp_cand);
    num += cand_len;
  }
  return num;
}
