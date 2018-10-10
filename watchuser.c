#include "watchuser.h"

struct watchuserelement *addWatchuser(char* username) {
  struct watchuserelement* w = NULL;
  w = calloc(1, sizeof(struct watchuserelement));
  w->username = calloc(strlen(username), sizeof(char*));
  strcpy(w->username, username);
  w->loggedOn = 0;
  w->next = NULL;
}

struct watchuserelement* removeWatchuser(struct watchuserelement* head, char* username) {
  struct watchuserelement* curr = head;
  struct watchuserelement* next = head;
  if (0 == strcmp(username, curr->username)) {
    head = curr->next;
    free(curr->username);
    free(curr);
  }
  else {
    while(next = curr->next) {
      if (0 == strcmp(username, next->username)) {
        curr->next = next->next;
        free(next->username);
        free(next);
      }
    }
  }
  return head;
}
