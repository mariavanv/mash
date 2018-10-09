#include "alias.h"
#define MAXARGS 10

struct aliaselement *addAlias(char* command, char** expansion, int argsct) {
  struct aliaselement* a = NULL;
  a = calloc(1, sizeof(struct aliaselement));
  a->command = calloc(strlen(command), sizeof(char*));
  a->expansion = calloc(MAXARGS, sizeof(char*));
  strcpy(a->command, command);
  for (int i = 0; i < argsct; i++) {
    a->expansion[i] = malloc(strlen(expansion[i]) * sizeof(char) + 2);
    snprintf(a->expansion[i], strlen(expansion[i]) + 2, "%s", expansion[i]);
    a->parts = i;
  }
  a->next = NULL;
}
