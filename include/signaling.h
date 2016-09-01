#pragma once

#include <icelib.h>

#define MAX_USER_LEN 128

typedef enum {
  UNDEF,
  REGISTERING,
  REGISTERED,
  INVITING,
}client_state;



void
signalPathHandler(client_state*    state,
                  ICELIB_INSTANCE* icelib,
                  int              sockfd,
                  char*            message,
                  int              len);

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
