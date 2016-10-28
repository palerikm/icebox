#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>



#include <icelib.h>
#include <stunclient.h>

#define MAX_MEDIA_SOCKETS 30

struct mediaConfig {
  pthread_t         mSocketListenThread;
  STUN_CLIENT_DATA* stunInstance;
  ICELIB_INSTANCE*  icelib;
};


void*
mSocketListen(void* ptr);
