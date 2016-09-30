#pragma once

#define MAX_MEDIA_SOCKETS 30

struct mediaConfig {
  /* struct prg_data* prg; */
  /* Signaling socket.. */
  int msock[MAX_MEDIA_SOCKETS];
  /* "Media" socket */
  /* TURN_INSTANCE_DATA* turnInst; */
  /* int                 msock; */

  /*Handles normal data like RTP etc */
  /* void (* signal_path_handler)(client_state*    state, */
  /*                             ICELIB_INSTANCE* pInstance, */
  /*                             int              sockfd, */
  /*                             char*, */
  /*                             int              buflen); */
};
