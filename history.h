#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

struct historyelement* historyCommand(char* commandline);

struct historyelement
{
  char *command;
  struct historyelement *prev;
  struct historyelement *next;
};
