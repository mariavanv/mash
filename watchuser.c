#include "watchuser.h"

struct watchuserelement *addWatchuser(char* username) {
  struct watchuserelement* w = NULL;
  w = calloc(1, sizeof(struct watchuserelement));
  w->username = calloc(strlen(username), sizeof(char*));
  strcpy(w->username, username);
  w->loggedOn = 0;
  w->next = NULL;
}
