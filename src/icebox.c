
/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "signaling.h"

#define PORT "5061" // the port client will be connecting to
#define MAXBUFLEN 1024
#define MAXDATASIZE 200 // max number of bytes we can get at once







struct listenConfig{
    struct prg_data *prg;
    int sockfd;

    /*Handles normal data like RTP etc */
    void (*signal_path_handler)(client_state *state,
                                int sockfd,
                                unsigned char *);

};


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void *socketListen(void *ptr){
    struct pollfd ufds[1];
    struct sockaddr_storage their_addr;
    unsigned char buf[MAXBUFLEN];
    //int sockfd =*(int *)ptr;
    struct listenConfig *lconfig = (struct listenConfig*) ptr;
    socklen_t addr_len;
    int rv;
    int numbytes;
    int i;

    ufds[0].fd = lconfig->sockfd;
    ufds[0].events = POLLIN;

    addr_len = sizeof their_addr;

    while(1){
        rv = poll(ufds, 1, -1);
        if (rv == -1) {
            perror("poll"); // error occurred in poll()
        } else if (rv == 0) {
            printf("Timeout occurred! (Should not happen)\n");
        } else {
            // check for events on s1:
            for(i=0;i<1;i++){
                if (ufds[i].revents & POLLIN) {
                    if ((numbytes = recvfrom(ufds[i].fd, buf, MAXBUFLEN , 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
                        perror("recvfrom");
                        exit(1);
                    }
                    lconfig->signal_path_handler(&lconfig->prg->state,
                                                 ufds[i].fd,
                                                 buf);
                    }
                }
            }
        }
}




int main(int argc, char *argv[])
{
    int sockfd;
    //char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    struct prg_data prg;

    struct listenConfig lconf;

    lconf.prg = &prg;


    pthread_t socketListenThread;

    if (argc < 3) {
        fprintf(stderr,"usage: icebox hostname user remoteuser\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    lconf.sockfd = sockfd;
    lconf.signal_path_handler = signalPathHandler;
    pthread_create( &socketListenThread, NULL, socketListen, (void*)&lconf);

    strncpy(prg.user, argv[2], MAX_USER_LEN);

    registerUser(&prg.state, sockfd, prg.user);
    if(argc ==4){
      //Ok got nothing better to do. Busywaiting
      while(prg.state != REGISTERED){}
      inviteUser(&prg.state, sockfd, argv[3], prg.user);
    }


    //if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
    //    perror("recv");
    //    exit(1);
    //}

    //buf[numbytes] = '\0';

    //printf("client: received '%s'\n",buf);
    sleep(100);
    close(sockfd);

    return 0;
}
