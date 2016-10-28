#pragma once

#include <icelib.h>

#include "media.h"
#define MAX_USER_LEN 128

typedef enum {
  UNDEF,
  REGISTERING,
  REGISTERED,
  INVITING,
}client_state;

struct sig_data {
  client_state state;
  size_t       msgBufSize;
  char*        msgBuf;
  char*        sdp;
};


void
signalPathHandler(struct sig_data*    sData,
                  struct mediaConfig* mconf,
                  int                 sockfd,
                  char*               message,
                  int                 len);

int
inviteUser(client_state* state,
           int           sockfd,
           char*         to,
           char*         from,
           char*         sdp);
int
registerUser(client_state* state,
             int           sockfd,
             char*         user_reg);

void
harvest(ICELIB_INSTANCE* icelib);
