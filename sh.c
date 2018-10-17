#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <glob.h>
#include <pthread.h>
#include <utmpx.h>
#include <fcntl.h>
#include "sh.h"

#define BUFFERSIZE 256

struct watchuserelement* watchuserList;
struct watchmailelement* watchmailList;
pthread_mutex_t watchuserMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t watchmailMutex = PTHREAD_MUTEX_INITIALIZER;
char* outRedir = ">";
char* outErrRedir = ">&";
char* outAppendRedir = ">>";
char* outErrAppendRedir = ">>&";
char* inRedir = "<";

int sh( int argc, char **argv, char **envp )
{
  char *prompt = calloc(PROMPTMAX, sizeof(char));
  char *commandline = calloc(MAX_CANON, sizeof(char));
  char *commandlinecopy = calloc(MAX_CANON, sizeof(char));
  char *command, *arg, *commandpath, *p, *pwd, *owd, *prevDir;
  char **args = calloc(MAXARGS, sizeof(char*));
  char **execargs = calloc(MAXARGS + 1, sizeof(char*));
  int uid, i, status, argsct, go = 1;
  struct passwd *password_entry;
  char *homedir;
  struct pathelement *pathlist;
  char* rootDir = "/";
  char* upDir = "..";
  char* thisDir = ".";
  char* space = " ";
  char* path = "PATH";
  char* home = "HOME";
  char* wildcard = "*";
  char* singlewildcard = "?";
  char* dash = "-";
  char* pipeSymbol= "|";
  struct historyelement *lastcommand = NULL;
  struct historyelement *newcommand = NULL;
  struct aliaselement* aliasList = NULL;
  int background = 0;
  pthread_t watchuserThread = 0;
  int noclobber = 0;

  uid = getuid();
  password_entry = getpwuid(uid);               /* get passwd info */
  homedir = password_entry->pw_dir;		/* Home directory to start
						  out with*/

  if ( (pwd = getcwd(NULL, PATH_MAX+1)) == NULL )
  {
    perror("getcwd");
    exit(2);
  }
  owd = calloc(strlen(pwd) + 1, sizeof(char));
  memcpy(owd, pwd, strlen(pwd));
  prompt[0] = '\0';
  prevDir = (char*) malloc(BUFFERSIZE);
  strcpy(prevDir, pwd);

  /* Put PATH into a linked list */
  pathlist = get_path();

  while ( go )
  {
    // reset background boolean
    background = 0;
    // clean up a finished child, if it exists
    int status;
    waitpid(-1, &status, WNOHANG);

    /* print your prompt */
    printf("%s [%s]>", prompt, pwd);

    /* get command line and process */
    char* fgetsResult;
    fgetsResult = fgets(commandline,  MAX_CANON, stdin);
    if (NULL == fgetsResult) {
      printf("\n");
      continue;
    }
    if (0 == strcmp(commandline, "\n") ) {
      continue;
    }
    commandline[strcspn(commandline, "\n")] = 0;// strip newline if it exists
    newcommand = historyCommand(commandline);
    newcommand->prev = lastcommand;
    if (NULL != lastcommand) {
      lastcommand->next = newcommand;
    }
    lastcommand = newcommand;
    const char space[2] = " ";
    strcpy(commandlinecopy, commandline);
    command = strtok(commandline, space);

    char* newArg = command;
    int argCounter = 0;
    /* for (int i = 0; i < MAXARGS; i++) { */
    while (newArg != NULL) {
      newArg = strtok(NULL, space);
      args[argCounter] = newArg;
      argCounter++;
    }
    argsct = argCounter;
    // check if & is set for backgrounding, then remove for rest of command processing
    if (1 < argsct && 0 == strcmp(args[argsct-2], "&")) {
      background = 1;
      args[argsct-2] = NULL;
    }
    argsct--;

    /* check for each built in command and implement */
    if (
	0 == strcmp(command, "exit") ||
	0 == strcmp(command, "which") ||
	0 == strcmp(command, "where") ||
	0 == strcmp(command, "cd") ||
	0 == strcmp(command, "pwd") ||
	0 == strcmp(command, "list") ||
	0 == strcmp(command, "pid") ||
	0 == strcmp(command, "kill") ||
	0 == strcmp(command, "prompt") ||
	0 == strcmp(command, "printenv") ||
	0 == strcmp(command, "alias") ||
	0 == strcmp(command, "history") ||
	0 == strcmp(command, "watchuser") ||
	0 == strcmp(command, "watchmail") ||
	0 == strcmp(command, "setenv")) {
      printf("Executing built-in ");
    }

    if (0 == strcmp(command, "exit")) {
      while(lastcommand) {
        struct historyelement* prev = lastcommand->prev;
        free(lastcommand->command);
        free(lastcommand);
        lastcommand = prev;
      } 
      while(aliasList) {
        struct aliaselement* next = aliasList->next;
        free(aliasList->command);
        for (int i = 0; i < MAXARGS; i++) {
          free(aliasList->expansion[i]);
        }
        free(aliasList->expansion);
        free(aliasList);
        aliasList = next;
      } 
      while(watchuserList) {
        struct watchuserelement* next = watchuserList->next;
        removeWatchuser(watchuserList, watchuserList->username);
        watchuserList = next;
      }
      if (watchuserThread) {
        pthread_cancel(watchuserThread);
        pthread_join(watchuserThread, NULL);
      }
      while(watchmailList) {
        struct watchmailelement* next = watchmailList->next;
        removeWatchmail(watchmailList, watchmailList->filename);
        watchmailList = next;
      }
      free(pathlist->element);
      while(pathlist) {
        struct pathelement* next = pathlist->next;
        free(pathlist);
        pathlist = next;
      }
      free(prompt);
      free(commandline);
      free(commandlinecopy);
      free(owd);
      free(pwd);
      free(prevDir);
      for (int i = 0; i < MAXARGS + 1; i++) {
        if (NULL != execargs[i]) {
        free(execargs[i]);
        }
      }
      free(execargs);
      free(args);

      printf("exit\n");
      break;
    }
    else if (0 == strcmp(command, "which")) {
      printf("which\n");
      // empty case
      if (NULL == args[0]) {
        continue;
      }
      // run which for each argument
      char* result;
      for (int i = 0; i < argsct-1; i++) {
        result = which(args[i], pathlist);
        if (NULL != result) {
          printf("%s\n", result);
        }
      }
      free(result);
    }
    else if (0 == strcmp(command, "where")) {
      printf("where\n");
      // run where for each argument
      for (int i = 0; i < argsct; i++) {
        if (NULL == args[i]) {
          continue;
        }
        else {
          where(args[i], pathlist);
        }
      }
    }
    else if (0 == strcmp(command, "cd")) {
      printf("cd\n");
      // cd should never have more than one argument (argsct includes the command name)
      if (argsct > 2) {
        printf("cd: Too many arguments.\n");
      }
      // chdir to the given arg
      else {
        cd(args[0], homedir, prevDir, pwd);
      }
    }
    else if (0 == strcmp(command, "pwd")) {
      printf("pwd\n");
      getcwd(pwd, BUFFERSIZE);
      printf("%s\n", pwd);
    }
    else if (0 == strcmp(command, "list")) {
      printf("list\n");
      if (NULL == args[0]) {
        // print current dir files
        list(pwd);
      }
      else {
        // print files in args[] directories
        char* prevCopy = calloc(sizeof(char), strlen(prevDir) + 1);
        strcpy(prevCopy, prevDir);
        for (int i = 0; i < argsct - 1; i++) {
          // changing to the dir allows for absolute, relative, ~, -, etc.
          cd(args[i], homedir, prevCopy, pwd);
          printf("\n%s:\n\n", pwd);
          list(pwd);
          // after listing, go back to the original directory
          cd("-", homedir, prevCopy, pwd);
        }
        free(prevCopy);
      }
    }
    else if (0 == strcmp(command, "pid")) {
      printf("pid\n");
      int pid = getpid();
      printf("%d\n", pid);
    }
    else if (0 == strcmp(command, "kill")) {
      printf("kill\n");
      // if only one arg, send sigterm
      if (2 == argsct) {
        int pid = atoi(args[0]);
        kill(pid, 15);
      }
      // otherwise send specified signal
      else if (3 == argsct) {
        int pid = atoi(args[1]);
        // move past dash in flag
        args[0]++;
        int sig = atoi(args[0]++);
        printf("%d", sig);
        kill(pid, sig);
      }
    }
    else if (0 == strcmp(command, "prompt")) {
      printf("prompt\n");
      // if prompt given, apply it
      if (NULL != args[0]) {
        strncpy(prompt, args[0], PROMPTMAX);
      }
      // otherwise prompt for one
      else {
        printf("prompt: ");
        fgets(prompt, PROMPTMAX, stdin);
        prompt[strlen(prompt) - 1] = '\0';
      }
    }
    else if (0 == strcmp(command, "printenv")) {
      printf("printenv\n");
      // at most one arg
      if (NULL != args[1]) {
        printf("printenv: Too many arguments.\n");
      }
      // print requested var
      else if (NULL != args[0]) {
        char* result = getenv(args[0]);
        printf("%s\n", result);
      }
      //print all vars
      else {
        printenv(envp);
      }
    }
    else if (0 == strcmp(command, "alias")) {
      printf("alias\n");
      // given a complete alias, create one
      if (NULL != args[1] && NULL != args[0]) {
        struct aliaselement* newAlias = addAlias(args[0], args+1, argsct-2);
        if (aliasList) {
          newAlias->next = aliasList;
        }
        aliasList = newAlias;
      }
      // given no alias, print existing ones
      else if (NULL == args[0]) {
        struct aliaselement* alias = aliasList;
        while(NULL != alias) {
          printf("%s\t%s ", alias->command, alias->expansion[0]);
          for (int i = 1; NULL != alias->expansion[i]; i++) {
            printf("%s ", alias->expansion[i]);
          }
          printf("\n");
          alias = alias->next;
        }
      }
      // otherwise, give usage info
      else {
        printf("alias: requires 0 arguments, or 2\n");
      }
    }
    else if (0 == strcmp(command, "history")) {
      printf("history\n");
      // default to 10 entries
      int count = 10;
      if (NULL != args[0]) {
        count = atoi(args[0]);
      }
      struct historyelement *he = lastcommand;
      // back up [count] entries
      while(NULL != he->prev && 0 < count) {
        he = he->prev;
        count--;
      }
      count = 0;
      // print out [count] entries
      while(NULL != he->next) {
        count++;
        printf("%d: %s\n", count, he->command);
        he = he->next;
      }
    }
    else if (0 == strcmp(command, "setenv")) {
      printf("setenv\n");
      // set given var
      if (NULL != args[1]) {
        setenv(args[0], args[1], 1);
      }
      // or, if only one arg, set var to empty
      else if (NULL != args[0]) {
        char* empty = "";
        int result = setenv(args[0], empty, 0);
      }
      // or, if no args, print all var
      else {
        printenv(envp);
      }
      // if path changed, update path
      if (NULL != args[0] && 0 == strcmp(args[0], path)) {
        struct pathelement *newPathlist = get_path();
        struct pathelement *next;
        while(NULL != pathlist->next) {
          next = pathlist->next;
          free(pathlist);
          pathlist = next;
        }
        pathlist = newPathlist;
      }
      // if home changed, update home
      if (NULL != args[0] && 0 == strcmp(args[0], home)) {
        homedir = getenv(home);
      }
    }
    else if (0 == strcmp(command, "watchuser")) {
      printf("watchuser\n");
      if (NULL == args[0]) {
        printf("watchuser: requires username\n");
        continue;
      }
      // block simultaneously editting watch list
      pthread_mutex_lock(&watchuserMutex);
      if (1 == argsct) {
        struct watchuserelement* watch = addWatchuser(args[0]);
        // if list is empty, new element becomes list
        if (NULL == watchuserList) {
          watchuserList = watch;
        }
        else {
          // add "watch" at end of list
          struct watchuserelement* curr = watchuserList;
          while(curr && curr->next) {
            curr = curr->next;
          }
          curr->next = watch;
        }
        checkUser(watch);
      }
      else if (2 == argsct && 0 == strcmp(args[1], "off")) {
        watchuserList = removeWatchuser(watchuserList, args[0]);
      }
      // end blocking
      pthread_mutex_unlock(&watchuserMutex);
      // start the watch thread if it doesn't exist
      if (0 == watchuserThread) {
        if (pthread_create(&watchuserThread, NULL, watchuser, NULL)) {
          fprintf(stderr, "error creating thread\n");
        }
      }
    }
    else if (0 == strcmp(command, "watchmail")) {
      printf("watchmail\n");
      if (NULL == args[0]) {
        printf("watchmail: requires filename\n");
        continue;
      }
      // block simultaneously editting watch list
      pthread_mutex_lock(&watchmailMutex);
      if (1 == argsct) {
        pthread_t thread_id;
        struct watchmailelement* file = addWatchmail(args[0]);
        // if list is empty, new element becomes list
        if (NULL == watchmailList) {
          watchmailList = file;
        }
        else {
          // add "file" at end of list
          struct watchmailelement* curr = watchmailList;
          while(curr && curr->next) {
            curr = curr->next;
          }
          curr->next = file;
        }
        // start the watch thread if it doesn't exist
        if (pthread_create(&thread_id, NULL, watchmail, file->filename)) {
          fprintf(stderr, "error creating thread\n");
        }
        else {
          file->thread_id = thread_id;
        }
      }
      else if (2 == argsct && 0 == strcmp(args[1], "off")) {
        // remove from mail list and close thread
        // TODO close thread
        watchmailList = removeWatchmail(watchmailList, args[0]);
      }
      // end blocking
      pthread_mutex_unlock(&watchmailMutex);
    }
    else if (0 == strcmp(command, "noclobber")) {
      noclobber = (noclobber + 1) % 2;
      printf("noclobber set to %d\n", noclobber);
    }
    else if (0 == strcmp(command, "set")) {
      if (args[0] && 0 == strcmp(args[0], "noclobber")) {
        noclobber = 1;
        printf("noclobber set to %d\n", noclobber);
      }
    }
    else if (0 == strcmp(command, "unset")) {
      if (args[0] && 0 == strcmp(args[0], "noclobber")) {
        noclobber = 0;
        printf("noclobber set to %d\n", noclobber);
      }
    }


    else {
      /*  else  program to exec */

      // check if it matches an alias in the alias table
      struct aliaselement* alias = aliasList;
      while(alias) {
        if (0 == strcmp(command, alias->command)) {
          for (int i = MAXARGS - 1; i > 0; i--) {
            args[i] = args[i-alias->parts];
          }
          for (int i = 0; i < alias->parts; i++) {
            args[i] = alias->expansion[i + 1];
          }
          strcpy(command, alias->expansion[0]);
          break;
        }
        alias = alias->next;
      }

      /* find it */
      char* com;
      if ((0 == strncmp(thisDir, command, 1)) || (0 == strncmp(rootDir, command, 1)) || (0 == strncmp(upDir, command, 2))) {
        com = (char*) malloc(sizeof(char) * BUFFERSIZE);
        snprintf(com, strlen(command) + 1, "%s", command);
      }
      else {
        com = which(command, pathlist);
      }
      /* do fork(), execve() and waitpid() */

      if(NULL != com && (0 == access(com, X_OK))) {
        printf("Executing %s\n", com);
        // TODO execute noclobber check to prevent forking and not execing
        pid_t parent = getpid();
        pid_t pid = fork();
        // parent
        if (pid > 0) {
          int status;
          int outputStatus = 0;
          // if background is not set, wait for child, otherwise it will check before next prompt
          if (!background) {
            waitpid(pid, &status, 0);
            if (0 != (outputStatus = WEXITSTATUS(status))) {
              printf("Exit %d\n", outputStatus);
            }
          }
        }
        // child
        else {
          char** const envp = {NULL};
          execargs[0] = com;
          int redirResult;
          // if not wildcards, run the program
          if (0 == strstr(commandlinecopy, wildcard) &&
              0 == strstr(commandlinecopy, singlewildcard)) {
            for (int i = 0; i < argsct; i++) {
              if(isRedirect(args[i])) {
                redirResult = checkRedirect(args[i], args[i+1], noclobber);
                i++;
              }
              else {
                execargs[i+1] = args[i];
              }
            }
            if (-1 == redirResult) {
              perror("Redirection error"); 
            }
            if (-1 == execve(com, execargs, envp)) {
              perror(command);
            }
          }
          // if there are wildcards, expand them all
          else {
            glob_t globbuf;
            int gl_offs_count = 1;
            for (int i = 0; i < argsct - 1; i++) {
              if(isRedirect(args[i])) {
                checkRedirect(args[i], args[i+1], noclobber);
                i++;
              }
              if (0 == strncmp(args[i], dash, 1)) {
                gl_offs_count++;
              }
            }
            // see glob(3)
            globbuf.gl_offs = gl_offs_count;
            glob(args[(gl_offs_count-1)], GLOB_DOOFFS, NULL, &globbuf);
            for (int i = 1; i < gl_offs_count; i++) {
              glob(args[gl_offs_count], GLOB_DOOFFS | GLOB_APPEND, NULL, &globbuf);
            }
            globbuf.gl_pathv[0] = (char*) malloc(strlen(command) + 1);
            strcpy(globbuf.gl_pathv[0], command);
            int idx = 1;
            for (int i = 0; i < MAXARGS; i++) {
              if (NULL != args[i] &&
                  0 == strstr(args[i], wildcard) &&
                  0 == strstr(args[i], singlewildcard)) {
                globbuf.gl_pathv[idx] = (char*) malloc(strlen(args[i]));
                globbuf.gl_pathv[idx] = args[i];
                idx++;
              }
            }
            if (-1 == execvp(com, &globbuf.gl_pathv[0])) {
              perror(command);
            }
            globfree(&globbuf);
          }
        }
      }
      else
        fprintf(stderr, "%s: Command not found.\n", command);
      free(com);
    }
  }
   return 0;
} /* sh() */

void cd(char *args, char* homedir, char* prevDir, char* pwd) {
  int result = -1;
  char* pd;
  pd = calloc(sizeof(char), BUFFERSIZE);
  getcwd(pd, BUFFERSIZE);
  // no args = cd home
  if (NULL == args) {
    result = chdir(homedir);
  }
  // ~ = home
  else if ('~' == args[0]) {
    args++;
    char* path = calloc(sizeof(char), strlen(homedir) + strlen(args) + 1);
    strcat(path, homedir);
    strcat(path, args);
    result = chdir(path);
    free(path);
  }
  // - = go back
  else if ('-' == args[0]) {
    if (NULL == prevDir || '\0' == prevDir[0]) {
      free(pd);
      return ;
    }
    result = chdir(prevDir);
  }
  // otherwise go where they said
  else {
    result = chdir(args);
  }
  if (0 == result) {
    strcpy(prevDir, pd);
    // set prevDir for cd -
    prevDir[strlen(pd)] = '\0';
    char* newPwd = calloc(sizeof(char), BUFFERSIZE);
    getcwd(newPwd, BUFFERSIZE);
    strcpy(pwd, newPwd);
    pwd[strlen(newPwd)] = '\0';
    free(newPwd);
  }
  else {
    perror(args);
  }
  free(pd);
}

char *which(char *command, struct pathelement *pathlist )
{
   /* loop through pathlist until finding command and return it.  Return
   NULL when not found. */
  while (NULL != pathlist->next) {
    DIR* folder = opendir(pathlist->element);
    if (NULL != folder) {
      struct dirent* dirEntry;
      while (NULL != (dirEntry = readdir(folder))) {
        if (0 == strcmp(command, dirEntry->d_name)) {
          char* result = (char*) malloc(sizeof(char) * BUFFERSIZE);
          snprintf(result, BUFFERSIZE, "%s/%s", pathlist->element, command);
          closedir(folder);
          return result;
        }
      }
    }
    pathlist = pathlist->next;
    closedir(folder);
  }
  return NULL;
} /* which() */

char *where(char *command, struct pathelement *pathlist )
{
  /* similarly loop through finding all locations of command */
  while (NULL != pathlist->next) {
    DIR* folder = opendir(pathlist->element);
    if (NULL != folder) {
      struct dirent* dirEntry;
      while (NULL != (dirEntry = readdir(folder))) {
        if (0 == strcmp(command, dirEntry->d_name)) {
          char* result = (char*) malloc(sizeof(char) * BUFFERSIZE);
          snprintf(result, BUFFERSIZE, "%s/%s", pathlist->element, command);
          printf("%s\n", result);
          free(result);
        }
      }
    }
    closedir(folder);
    pathlist = pathlist->next;
  }
  return NULL;
} /* where() */

void list ( char *dir )
{
  /* see man page for opendir() and readdir() and print out filenames for
  the directory passed */
  DIR* folder = opendir(dir);
  struct dirent* dirEntry;
  while (NULL != (dirEntry = readdir(folder))) {
    printf("%s\n", dirEntry->d_name);
  }
  closedir(folder);
} /* list() */

void printenv(char** envp) {
  for (int i = 0; NULL != envp[i]; i++) {
    printf("%s\n", envp[i]);
  }
}

void* watchuser(void* param) {
  while(1) {
    struct watchuserelement* curr = watchuserList;
    while(curr) {
      if (!curr->loggedOn) {
        checkUser(curr);
      }
      curr = curr->next;
    }
    sleep(20);
  }
}

void checkUser(struct watchuserelement* watch) {
  struct utmpx* up;
  setutxent();
  while(up = getutxent()) {
    if (USER_PROCESS == up->ut_type && 0 == strcmp(up->ut_user, watch->username)) {
      printf("%s has logged on %s from %s\n", up->ut_user, up->ut_line, up->ut_host);
      watch->loggedOn = 1;
    }
  }
}

void* watchmail(void* param) {
  char* filename = (char*) param;
  struct stat fileStat;
  int statResult = stat(filename, &fileStat);
  while(1) {
    if (fileModified(filename, fileStat.st_mtime)) {
      statResult = stat(filename, &fileStat);
      // TODO ctime date format
      printf("\aYou've Got Mail in %s at %ld\n", filename, fileStat.st_mtime);
    }
    sleep(1);
  }
}

// given a file and the last time it was modified, see if it has been updated
int fileModified(char* path, time_t prevTime) {
  struct stat fileStat;
  int statResult = stat(path, &fileStat);
  if (0 != statResult) {
    perror("watchmail");
    pthread_mutex_lock(&watchmailMutex);
    watchmailList = removeWatchmailElement(watchmailList, path, NO_CLOSE_THREAD);
    // TODO remove from watchmail list
    pthread_mutex_unlock(&watchmailMutex);
    pthread_exit(NULL);
  }
  return fileStat.st_mtime > prevTime;
}

int checkRedirect(char* redir, char* filename, int noclobber) {
  int fid;
  int mode = 0644;
  if (0 == strcmp(redir, outRedir) || 0 == strcmp(redir, outErrRedir)) {
    if (!noclobber) {
      fid = open(filename, O_WRONLY | O_CREAT | O_TRUNC, mode);
    } else {
      fid = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_EXCL, mode);
    }
    if (-1 == fid) {
      perror(filename);
    }
    else {
      close(1);
      dup(fid);
      close(fid);
    }
  }
  if (0 == strcmp(redir, outErrRedir)) {
    if (!noclobber) {
      fid = open(filename, O_WRONLY | O_CREAT | O_TRUNC, mode);
    } else {
      fid = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_EXCL, mode);
    }
    if (-1 == fid) {
      perror(filename);
    }
    else {
      close(2);
      dup(fid);
      close(fid);
    }
  }
  else if (0 == strcmp(redir, outAppendRedir) || 0 == strcmp(redir, outErrAppendRedir)) {
    if (!noclobber) {
      fid = open(filename, O_WRONLY | O_CREAT | O_APPEND);
    }
    else {
      fid = open(filename, O_WRONLY | O_APPEND);
    }
    if (-1 == fid) {
      perror(filename);
    }
    else {
      close(1);
      dup(fid);
      close(fid);
    }
  }
  else if (0 == strcmp(redir, outErrAppendRedir)) {
    if (!noclobber) {
      fid = open(filename, O_WRONLY | O_CREAT | O_APPEND);
    }
    else {
      fid = open(filename, O_WRONLY | O_APPEND);
    }
    if (-1 == fid) {
      perror(filename);
    }
    else {
      close(2);
      dup(fid);
      close(fid);
    }
  }
  else if (0 == strcmp(redir, inRedir)) {
    if (!noclobber) {
      fid = open(filename, O_RDONLY | O_CREAT);
    }
    else {
      fid = open(filename, O_RDONLY);
    }
    if (-1 == fid) {
      perror(filename);
    }
    else {
      close(2);
      dup(fid);
      close(fid);
    }
  }
  return fid;
}

int isRedirect(char* arg) {
  return !(strcmp(arg, outRedir) &&
           strcmp(arg, outErrRedir) &&
           strcmp(arg, outAppendRedir) &&
           strcmp(arg, outErrAppendRedir) &&
           strcmp(arg, inRedir));
}
