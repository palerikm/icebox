#pragma once

#include <icelib.h>
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

enum cline_f {
  nettype,
  addrtype,
  clineaddr,
  max_cline_f
};

char*
sdp_candidate_toString(const ICE_CANDIDATE* cand);

size_t
sdpCandCat(char*                sdp,
           size_t*              sdpSize,
           const char*          ufrag,
           const char*          passwd,
           const ICE_CANDIDATE* cand,
           size_t               numcand);

void
parseSDP(ICELIB_INSTANCE* icelib,
         char*            sdp,
         int              len);
