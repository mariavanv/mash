
#include "get_path.h"
#include "history.h"
#include "alias.h"
#include "watchuser.h"
#include "watchmail.h"

int pid;
int sh( int argc, char **argv, char **envp);
void cd(char *args, char* homedir, char* prevDir, char* pwd);
char *which(char *command, struct pathelement *pathlist);
char *where(char *command, struct pathelement *pathlist);
void list ( char *dir );
void printenv(char **envp);
void* watchuser(void* param);
void* watchmail(void* param);
void checkUser(struct watchuserelement* user);
int fileModified(char* filename, time_t oldTime);

#define PROMPTMAX 32
#define MAXARGS 10
#define MAXPIDS 10
