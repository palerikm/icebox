#pragma once
#include "candidate_harvester.h"


enum cand_f {
  fond,
  compid,
  trnsp,
  priority,
  addr,
  port,
  type,
  max_cand_f
};


char*
sdp_candidate_toString(ICE_CANDIDATE* cand);

size_t
sdpCandCat(char*         sdp,
           size_t*       sdpSize,
           struct hcand* cand,
           size_t        candLen);

void
parseSDP(char* sdp,
         int   len);
