/*
 * Melody Lugo and Kyle Sullivan
 * Assignment #2 - sh.h
 */

#include "get_path.h"
#include "strmap.h"

int pid;
int sh( int argc, char **argv, char **envp);

/* Built in Commands */
char *which( char *command, struct pathelement *pathlist );
char *where( char *command, struct pathelement *pathlist );
void cd( char *dir, char *cwd, char *home, char *prev_wd );
void list ( char *dir , char *cwd);
void list();
void getPid();
void myKill();
void printenv(char **envp);
void mySetenv();

/* Handles Alias Commands Table */
void addAlias(StrMap *alias_table, char **args, int argsct);
char *getAlias(StrMap *alias_table, char* name);
static void freeAliasTable(const char *key, const char *value, const void *obj);
static void printAliasTable(const char *key, const char *value, const void *obj);

/* Execute the Commands */
int runExternalCommand(int argsct, char **args, char **envp);
int runBuiltInCommand(char *command, char **args, int argsct);

/* Used to handle signals */
void shell_INT_handler(int sig);
void shell_ALARM_handler(int sig);

static void *myWarnloadThread(void *param);
int get_load(double *loads);

/*  Constants */
#define PROMPTMAX 32
#define MAXARGS 10
#define MAXALIAS 1024
#define MAXHISTORY 5012
#define DEFAULTHISTORY 10








