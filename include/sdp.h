#pragma once
#include "candidate_harvester.h"

char *
sdp_candidate_toString(ICE_CANDIDATE* cand);

size_t sdpCandCat(char *sdp, size_t *sdpSize,
                  struct hcand* cand, size_t candLen);
