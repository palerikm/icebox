

#define MAX_USER_LEN 128

typedef enum{
  UNDEF,
  REGISTERING,
  REGISTERED,
  INVITING,
}client_state;

struct prg_data{
  client_state state;
  char user[MAX_USER_LEN];
};

void signalPathHandler(client_state *state,
                       int sockfd,
                       unsigned char *message);
                       
int inviteUser(client_state *state, int sockfd, char *to, char *from);
int registerUser(client_state *state, int sockfd, char *user_reg);
