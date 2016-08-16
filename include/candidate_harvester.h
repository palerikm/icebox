
#pragma once

#include <net/if.h>
#include <icelibtypes.h>

struct hcand {
  char          ifname[IFNAMSIZ];
  int           sockfd;
  ICE_CANDIDATE ice;
};


void
printhCandidate(struct hcand* cand);

int32_t
harvest_host(struct hcand** candidates,
             int            proto);
