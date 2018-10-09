#include "sh.h"
#include <signal.h>
#include <stdio.h>

void sig_handler(int signal); 

int main( int argc, char **argv, char **envp )
{
  /* put signal set up stuff here */
  if (signal(SIGINT, sig_handler) == SIG_ERR)
    printf("\ncan't catch SIGINT\n");
  else if (signal(SIGTERM, sig_handler) == SIG_ERR) {
    printf("\ncan't catch SIGTERM");
  }
  else if (signal(SIGTSTP, sig_handler) == SIG_ERR) {
    printf("\ncan't catch SIGTSTP");
  }
  return sh(argc, argv, envp);
}

void sig_handler(int signal)
{
  /* define your signal handler */
  if (SIGINT == signal) {
    printf("\n");
  }
  else if (SIGTERM == signal || SIGTSTP == signal) {
  }
}

