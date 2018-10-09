#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

struct aliaselement* addAlias(char* command, char** expansion, int argsct);

struct aliaselement
{
  char* command;
  char** expansion;
  struct aliaselement *next;
  int parts;
};
