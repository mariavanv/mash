#include "watchmail.h"

struct watchmailelement *addWatchmail(char* filename) {
  struct watchmailelement* w = NULL;
  w = calloc(1, sizeof(struct watchmailelement));
  w->filename = calloc(strlen(filename), sizeof(char*));
  strcpy(w->filename, filename);
  w->next = NULL;
}

struct watchmailelement* removeWatchmail(struct watchmailelement* head, char* filename) {
  struct watchmailelement* curr = head;
  struct watchmailelement* next = head;
  if (0 == strcmp(filename, curr->filename)) {
    head = curr->next;
    pthread_cancel(curr->thread_id);
    if(pthread_join(curr->thread_id, NULL)) {
      perror("Could not join thread");
    }
    free(curr->filename);
    free(curr);
  }
  else {
    while(next = curr->next) {
      if (0 == strcmp(filename, next->filename)) {
        curr->next = next->next;
        pthread_cancel(next->thread_id);
        if(!pthread_join(next->thread_id, NULL)) {
          perror("Could not joing thread");
        }
        free(next->filename);
        free(next);
      }
    }
  }
  return head;
}
