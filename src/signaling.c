
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <ctype.h>

#include "signaling.h"

int registerUser(client_state *state, int sockfd, char *user_reg)
{
    int numbytes;
    char str[255];

    printf("Registering: %s\n", user_reg);
    *state = REGISTERING;

    strncpy(str,"REGISTER ", 255);
    strncat(str, user_reg, 245);

    if ((numbytes = send(sockfd, str,strlen(str), 0 )) == -1) {
      perror("registerUser: send");
      return -1;
    }
    return 1;
}

int inviteUser(client_state *state, int sockfd, char *to, char *from)
{
    int numbytes;
    char str[255];
    memset(str, 0, sizeof str);

    if(*state!= REGISTERED){
      return -1;
    }

    *state = INVITING;

    strncpy(str,"INVITE ", 255);
    strncat(str, to, MAX_USER_LEN);
    strncat(str, "\n", 1);
    strncat(str, "To: ",4);
    strncat(str, to, MAX_USER_LEN);
    strncat(str, "\nFrom: ", 7);
    strncat(str, from, MAX_USER_LEN);

    printf("Sending invite\n%s\n", str);
    if ((numbytes = send(sockfd, str,strlen(str), 0 )) == -1) {
      perror("inviteUser: send");
      return -1;
    }
    return 1;
}

void signalPathHandler(client_state *state,
                       int sockfd,
                       unsigned char *message)
{
  char *delim = "\n:\\";
  char *tok = strtok((char *)message, delim);

  printf("Handling...\n");

  if (*state == REGISTERING){
    if(strncmp(tok, "200 OK", 6)==0){
      printf("Registered\n");
      *state = REGISTERED;
    }else{
       printf("Registration failed with %s\n", message);
    }
  }
  if (*state == INVITING){
    if(strncmp(tok, "100 Trying", 6)==0){
      printf("Invitation recieved at notice server\n");
      return;
    }
    if(strncmp(tok, "200 OK", 6)==0){
      printf("Got a 200 OK (Session Established..)\n");
    }else{
      printf("Invitation failed with %s\n", message);
    }
  }
  if (*state == REGISTERED){
    if(strncmp(tok, "INVITE", 6)==0){
      int numbytes;
      char str[255];
      char str_to[128];
      char str_from[128];
      int i;

      //Worst parser ever, works by accident
      for(i=0;i<2;i++){
        tok = strtok(NULL, delim);
        printf("tok: (%s)\n",tok);
        if( strcmp(tok, "To") == 0){
          tok = strtok(NULL, delim);
          while( isspace(*tok)) tok++;
          strncpy(str_from, tok, strlen(tok));
        }
        if( strcmp(tok, "From") == 0){
          printf("Got a from token\n");
          tok = strtok(NULL, delim);
          while( isspace(*tok)) tok++;
          strncpy(str_to, tok, strlen(tok));
        }
      }

      strncpy(str,"200 OK\nTo: \0", 12);
      strncat(str, str_to, strlen(str_to));
      strncat(str, "\nFrom: ", 7);
      strncat(str, str_from, strlen(str_from));
      printf("Sending 200OK\n%s\n", str);

      if ((numbytes = send(sockfd, str,strlen(str), 0 )) == -1) {
        perror("200Ok: send");
      }
    }else{
      printf("Got something %s\n", message);
    }
  }
}
