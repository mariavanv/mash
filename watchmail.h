#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

struct watchmailelement* addWatchmail(char* mailname);
struct watchmailelement* removeWatchmail(struct watchmailelement* head, char* mailname);
struct watchmailelement* removeWatchmailElement(struct watchmailelement* head, char* mailname, int noCloseThread);

struct watchmailelement
{
  char* filename;
  pthread_t thread_id;
  struct watchmailelement *next;
};
