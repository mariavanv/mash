#include "history.h"

struct historyelement *historyCommand(char* commandline) {
  struct historyelement* e = NULL;
  e = calloc(1, sizeof(struct historyelement));
  e->command = calloc(strlen(commandline), sizeof(char*));
  strcpy(e->command, commandline);
  e->prev = NULL;
  e->next = NULL;
}
