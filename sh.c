/*
 * Melody Lugo and Kyle Sullivan
 * Assignment #3 - sh.c
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <glob.h>
#include <time.h>
#include <sys/time.h>
#include "sh.h"
#include "strmap.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#ifdef HAVE_KSTAT
#include <kstat.h>
#endif

/*  Global Variables */
char *prompt, *commandline;
char *command, *arg, *commandpath, *p, *pwd, *owd;
char **args; 
int uid, i, status, argsct, aliasct;
int historyct = 0, go = 1, timeout = 0, redirect = 0;
int timeout_completed = 0, child_pid = 0, interruptFlag = 0;
int noclobber = 0;
struct passwd *password_entry;
char *homedir;
char prev_wd[1024];
char **history;
char cwd[1024];
extern char **environ;
struct pathelement *pathlist;
typedef enum { false, true } bool;
StrMap *alias_table; //to store the alias information
char buf[255];
float warnlevel = -1;
/**
 * sh
 * - Main function of mysh
 *   performs all of the major roles
 *   of the shell
 */
int sh( int argc, char **argv, char **envp )
{
  /* Allocate memory for variables to be used later on */
  prompt         = calloc(PROMPTMAX, sizeof(char)); 
  commandline    = calloc(MAX_CANON, sizeof(char));
  args           = calloc(MAXARGS, sizeof(char*));
  history        = calloc(MAXHISTORY, sizeof(char*));

  /*  Allocate the table to store all of the alias commands */
  alias_table = sm_new(MAXALIAS);
  if (alias_table == NULL) {
    /* Handle allocation failure... */
    fprintf(stderr, "The string map did not allocate for the alias table...\n");
    return -1;
  }

  /* Parse the timeout value */
  if(argc > 1){
    timeout = atoi(argv[1]);
  }else{
    timeout = 0;
  }

  uid = getuid();
  password_entry = getpwuid(uid);               /* get passwd info */
  homedir = password_entry->pw_dir;   /* Home directory to start out with*/

  if ( (pwd = getcwd(NULL, PATH_MAX+1)) == NULL )
  {
    perror("getcwd");
    exit(2);
  }
  
  strcpy(prev_wd,pwd); // initialize the previous working directory
  
  owd = calloc(strlen(pwd) + 1, sizeof(char));
  memcpy(owd, pwd, strlen(pwd));
  prompt[0] = ' '; prompt[1] = '\0';

  /* Put PATH into a linked list */
  pathlist = get_path();

  /* Main Shell while loop */
  while ( go )
  {
    
    //Catch the ^C (SIGINT)
    if(interruptFlag){
      interruptFlag = 0;
     // printf("\n");
      continue;
    }
    
    //clear the args variable for the next command to be inputted
    free(args);
    args = calloc(MAXARGS, sizeof(char*));
    
    //Let's check if there is a background child to wait on
    //pid_t returnValue = waitpid(-1, NULL, WNOHANG);
    while(waitpid(-1, NULL, WNOHANG) > 0){};
    //printf("return value is: %d\n", returnValue);

    /* print your prompt */
    //char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL){
      fprintf(stderr, "%s\n", strerror(errno));
    }

    printf("%s[%s]> ", prompt, cwd);

    /* get command line input (handle EOF also) */
    if(fgets(commandline, MAX_CANON, stdin) == NULL){
      
      char *ignoreeof = getenv("ignoreeof");
      if(ignoreeof != NULL && strcmp(ignoreeof, "1") == 0){
        printf(" ^D\nUse \"exit\" to leave mysh.\n");
        continue;
      }else{
        printf("exit\n");
        char *temp[1];
        temp[0] = "exit";
        runBuiltInCommand("exit", temp, 1);//manually run exit
        continue; 
      }
    }
    
    
    /* add the command line input to our history array */
    commandline[strlen(commandline) - 1] = '\0'; //to remove trailing new line

    char *history_entry = malloc((strlen(commandline) + 1) * sizeof(char));
    strcpy(history_entry, commandline);
    if(strcmp(history_entry, "") != 0){
      history[historyct] = history_entry; // gotta copy the string over
      historyct++;
    }

    /*  parse the tokens from the command line */
    char * pch;
    pch = strtok (commandline," ");
    int parseFlag = 1;
    argsct = 0;
    redirect = 0;
    while (pch != NULL)
    {
      if(parseFlag){
        command = pch;
        parseFlag = 0;
      }

      //check if there is a redirect
      if( pch[0] == '>' || pch[0] ==  '<'){
	redirect = argsct;//index of where the redirect characters are
	printf("redirect is %s and is at index %d\n", pch, argsct);
      }
      args[argsct] = pch;
      pch = strtok (NULL, " ");
      argsct++;
    }
    
    if(args[0] == NULL){
      continue;
    }
    
    /*
     * Start of Built in Commands
     */
     
    int rval;
    rval = runBuiltInCommand(command, args, argsct);
    if(rval){
      //The command was found and was executed, continue with while loop
      continue;
    }else if( (getAlias(alias_table, command)) != NULL) {
      
      char **temp_args = calloc(MAXARGS, sizeof(char*));;
      int temp_argsct = 0;
      char *temp = getAlias(alias_table, command);
      char *temp_command;
      
      //parse the tokens from the command line
      pch = strtok (temp," ");
      parseFlag = 1;
      argsct = 0;
      while (pch != NULL)
      {
        if(parseFlag){
          temp_command = pch;
          parseFlag = 0;
        }
        temp_args[temp_argsct] = pch;
        pch = strtok (NULL, " ");
        temp_argsct++;
      }
      
      rval = runBuiltInCommand(temp_command, temp_args, temp_argsct);
      if(rval == 1){
        free(temp_args);
        continue;
        
      }else if(rval == 0){
        rval = runExternalCommand(temp_argsct, temp_args, envp);
        if(rval == 1){
          free(temp_args);
          continue;
        }
      }

      free(temp_args);
      continue;
    /* end running alias command */  

    }else{
      
      //check to see if there is a wildcard
      int i, j;
      int wildcardFlag = 0;
      for(i = 0; i < argsct; i++){
        for(j = 0; j < strlen(args[i]); j++){
          if(args[i][j] == '*' || args[i][j] == '?'){
            wildcardFlag = 1;
          } 
        }
      }

      rval = runExternalCommand(argsct, args, envp);
    }
  }
  /* End While Loop  */  
  
  /* Don't forget to free all of the memory! */
  free(prompt); 
  free(commandline);
  free(args);       
  
  struct pathelement *temp = pathlist;
  struct pathelement *curr = pathlist;
   
  if(curr != NULL){
   while(curr != NULL){
     curr = curr->next;
     free(temp);
     temp = curr;
   }
   free(temp); // since it exits before freeing the final node
  }
  free(owd);
  sm_delete(alias_table);
  
  int i;
  for(i = 0; i < historyct; i++){
    free(history[i]);
  }
  free(history);     
  
  return 0;
} /* sh() */


/**
 * which
 * - Utility function to provide support for
 *   the built in function which
 * 
 * - which takes in a command and the pathlist and returns
 *   the location path which the command will be executed from
 */
char *which(char *command, struct pathelement *pathlist )
{
   /* loop through pathlist until finding command and return it.  Return
   NULL when not found. */
   struct pathelement *curr;
   curr = pathlist;
   while(curr != NULL){

    char *filepath;
    filepath = malloc(strlen(curr->element)+strlen(command)+2); //1 for '/'' and 1 for null-terminated character
     
    //build the filepath
    strcpy(filepath, curr->element);
    strcat(filepath, "/");
    strcat(filepath, command); 
     
    /* Check file existence. */
    int rval;
    rval = access(filepath, X_OK);
    if (rval == 0){ 
      //it worked
      //printf("WHY AM I HERE?\n");
      //printf("%s\n", filepath);
      return filepath;

    }
    
    free(filepath);
    curr = curr->next;
   }

} /* which() */

/**
 * where
 * - Utility function to provide support for
 *   the built in function where
 * 
 * - where takes in a command and the pathlist and returns
 *   every location the command could be executed from
 */
char *where(char *command, struct pathelement *pathlist )
{  
   struct pathelement *curr;
   curr = pathlist;
   while(curr != NULL){

    char *filepath;
    filepath = malloc(strlen(curr->element)+strlen(command)+2); //1 for '/' and 1 for null-terminated character
     
    //build the filepath
    strcpy(filepath, curr->element);
    strcat(filepath, "/");
    strcat(filepath, command); 
     
    /* Check file existence. */
    int rval;
    rval = access(filepath, X_OK);
    if (rval == 0){ 
      //it worked
      printf("%s\n", filepath);
    }
    
    free(filepath);
    curr = curr->next;
   }
} /* where() */

/**
 * cd (change directory)
 * - Utility function to provide support for 
 *   the built in command cd
 * 
 * - takes as arugments the directory, current working directory,
 *   the home directory, and the previous working directory
 */
void cd(char *dir, char *cwd, char *home, char *prev_wd){

  if(dir == NULL){
    if(home == NULL){
       fprintf(stderr, "%s: No such file or directory\n", dir);
       return;
    }
    dir = home;
  }
  
  if(strcmp(dir,"-") == 0){
    strcpy(dir, prev_wd);
  }
  
  char temp_cwd[1024];
  int rval;
  
  strcpy(temp_cwd, cwd);
  rval = chdir(dir); // this is what actually changes the current working directory
  if(rval == 0){

    strcpy(prev_wd, temp_cwd);
    prev_wd[strlen(temp_cwd)] = '\0';

    if ( (cwd = getcwd(NULL, PATH_MAX+1)) == NULL )
    {
      perror("getcwd");
      exit(2);
    }

  }else {
    if (errno == EACCES){ 
      printf ("%s is not accessible (%s)\n", dir, strerror(errno));
    }else{
      fprintf(stderr, "%s: No such file or directory\n", dir);
    }
  }
  return;
} /* cd() */

/**
 * list
 * - Utility function to provide support for
 *   the built in list function
 * 
 * - takes a directory name (possibly NULL) and 
 *   the current working directory, and prints
 *   the contents of the directory
 */
void list ( char *dir , char *cwd)
{
  struct dirent *rDir;
  DIR *oDir;

  if (dir == NULL ){
    dir = cwd;
  }
  
  oDir = opendir(dir);
    if (oDir == NULL) {
        printf("%s\n", strerror(errno));
        return;
    }
    
  printf("\n[%s]:\n", dir); //print directory
  rDir = readdir(oDir);
  while (rDir != NULL){
    printf("%s\n", rDir->d_name); //print what's in directory
    rDir = readdir(oDir);
  }
  closedir(oDir);
} /* list() */

/**
 * getAlias
 * - retrieves the alias matching <name> and returns it,
 *   if no entry matches <name>, returns a null terminating character
 */
char *getAlias (StrMap *alias_table, char *name){
 // printf("Getting alias named %s\n", name);
  
  /* Retrieve a value */
  int result;
  result = sm_get(alias_table, name, buf, sizeof(buf));
  if (result == 0) {
      /* Handle value not found... */
//      printf("%s was not found in the alias table\n", name);
      return '\0';
  }
 // printf("value: %s\n", buf);
  
  
  return buf;
}

/**
 * addAlias
 * - takes a command to be made into an alias 
 *  (entered by the user) and inserts it into
 *  the alias table for later use
 */
void addAlias (StrMap *alias_table, char **args, int argsct){
  printf("Adding alias: %s\n", args[1]);
  if(MAXALIAS < argsct){
    fprintf(stderr, "Exceeded max alias size\n");
    return;
  }else if(argsct < 2){
    fprintf(stderr, "Usage: alias <name> <command>\n");
    return;
  }
  
  int total_length = argsct - 2; //to account for the spaces in between
  for(i = 2; i < argsct; i++){
    total_length += strlen(args[i]);
  }

  char* alias = malloc(total_length * sizeof(char));
  
  for(i = 2; i < argsct; i++){
    strncat (alias, args[i], strlen(args[i]));
    strcat (alias, " ");
  }
  alias[total_length-1] = '\0'; //hopefully this works 
  printf("added alias: %s\n", alias);

  sm_put(alias_table, args[1], alias);
  
}

/*  I dont think we need this, I think its built in */
static void freeAliasTable(const char *key, const char *value, const void *obj){
  printf("Freeing %s from alias table\n", value);
  free((char*)value);
}

/**
 * printAliasTable
 * - Utility function which prints out all of the entires
 *   inside of the alias table
 */
static void printAliasTable(const char *key, const char *value, const void *obj)
{
    printf("%s\t(%s)\n", key, value);
}

/**
 * runExternalCommand
 * - runs an external command, passing it the args and envp provided
 *   returns 1 if the command was successful run, else returns 0
 */
int runExternalCommand(int argsct, char **args, char **envp){
  char *command = args[0];
  char *exe = malloc( sizeof(char) * ( 256 + 1 ) );
  int bgFlag = 0;
  
  if(command[0]=='/' || command[0]=='.'){
    strcpy(exe, command);
  }else if(which(command, pathlist) != NULL){
    strcpy(exe, which(command, pathlist));
  }
  
  //printf("Last arg is %s\n", args[argsct-1]);
  //printf("Compare is %d\n", strcmp(args[argsct-1], '&'));

  //handle background character '&'
  if(strcmp(args[argsct-1], "&") == 0){
    args[argsct-1] = '\0';
    argsct--;
    bgFlag = 1;
  }

  //variables for file redirections
  char redirectChar[3];
  redirectChar[0] = '\0';
  redirectChar[1] = '\0';
  redirectChar[2] = '\0';
  
  int fid;

  //handle file redirections
  if(redirect){
    printf("redirect file is: %s\n", args[redirect+1]);
    strcpy(redirectChar, args[redirect]);
    printf("redirect char is: %s\n", redirectChar);

    if(strcmp(redirectChar, ">") == 0){
      fid = open(args[redirect+1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP);
      close(1);
      dup(fid);
      close(fid);
    } else if(strcmp(redirectChar, ">&") == 0){

    } else if(strcmp(redirectChar, ">>") == 0){

    } else if(strcmp(redirectChar, ">>&") == 0){
      
    } else if(strcmp(redirectChar, "<") == 0){
      
    }

    //adjust the command args so it executes properly
    args[redirect] = '\0';
    argsct = redirect;
  }


  /*
  * Wild cards are handled usingthe glob function,
  * each argument is passed into glob and expanded
  * if needed, the result is a new args list with all of
  * the expanded file paths included
  */
  glob_t globbuf;
  
  glob(args[0], GLOB_NOCHECK, NULL, &globbuf);
  for(i = 1; i < argsct; i++){
      glob(args[i], GLOB_NOCHECK | GLOB_APPEND, NULL, &globbuf);
  }
  /* end  handling wild cards */

  printf("Background flag is: %d\n", bgFlag);

  //check if we have authority before we execute
  if(access(exe, X_OK) == 0){ 
    child_pid = fork();
    if (child_pid == 0) {                 /* child */

      int rval;

      //check if we need to put the child into the background
      if(bgFlag){
	//printf("moved child into background\n");
	rval = setpgid(0, 0); //change process group
	if(rval != 0){
	  fprintf(stderr, "%s\n", strerror(errno));
	}
      }
      
      rval = execve(exe, &globbuf.gl_pathv[0], envp);
      if(rval != 0){
        fprintf(stderr, "%s\n", strerror(errno));
      }
      exit(0);//make the child exit after he is done executing
    }else{                          /* parent */
      
      //set process group of child to the same as the parent
      //setpgid(child_pid, 0);
      
      //optional: timeout set at shell start
      if(timeout){
        //set an alarm in case the child takes too long
        alarm(timeout);
      } 

      int waitFlag = WUNTRACED;
      if(bgFlag){
	waitFlag = waitFlag | WNOHANG;
      }
      //wait on the child process
      waitpid(child_pid, NULL, waitFlag); 
    }

    //fix file redirection if needed
    if(redirect){
      fid = open("/dev/tty", O_WRONLY);
      close(1);
      dup(fid);
      close(fid);
    }

    if(timeout_completed){
      timeout_completed = 0;
      printf("!!! taking too long to execute this command !!!\n");
    }
    child_pid = 0; //set back the global child_pid to 0 
    alarm(0); //cancel the previous alarm if needed
  } else{
      fprintf(stderr, "%s: Command not found.\n", args[0]);
  }
  
  return 1;
}

static void *myWarnloadThread(void *param){
  int count = 0;
  double loads[1];
  const char *name = param;
  while(1){
    if(get_load(loads)){
      printf("getload %d\n", get_load(loads)); 
      printf("load->%.2lf\n", loads[0]/100);
      if(loads[0]/100 > warnlevel){
	printf("Warning load level is %.2lf\n", loads[0]/100);
      }
    }
    printf("hello from %s [%d]\n", name, count++);
    sleep(1);
    if(warnlevel == 0.0){
      warnlevel = -1;
      pthread_exit(0);
    }
  }
}

int get_load(double *loads){
#ifdef HAVE_KSTAT
  kstat_ctl_t *kc;
  kstat_t *ksp;
  kstat_named_t *kn;

  kc = kstat_open();
  if (kc == 0)
  {
    perror("kstat_open");
    exit(1);
  }

  ksp = kstat_lookup(kc, "unix", 0, "system_misc");
  if (ksp == 0)
  {
    perror("kstat_lookup");
    exit(1);
  }
  if (kstat_read(kc, ksp,0) == -1) 
  {
    perror("kstat_read");
    exit(1);
  }

  kn = kstat_data_lookup(ksp, "avenrun_1min");
  if (kn == 0) 
  {
    fprintf(stderr,"not found\n");
    exit(1);
  }
  loads[0] = kn->value.ul/(FSCALE/100);
  printf("LOADS[0]->%.2lf",loads[0]/100);

  kn = kstat_data_lookup(ksp, "avenrun_5min");
  if (kn == 0)
  {
    fprintf(stderr,"not found\n");
    exit(1);
  }
  loads[1] = kn->value.ul/(FSCALE/100);

  kn = kstat_data_lookup(ksp, "avenrun_15min");
  if (kn == 0) 
  {
    fprintf(stderr,"not found\n");
    exit(1);
  }
  loads[2] = kn->value.ul/(FSCALE/100);

  kstat_close(kc);
  return 0;
#else
  /* yes, this isn't right */
  loads[0] = loads[1] = loads[2] = 0;
  return -1;
#endif
}


/**
 * runBuiltInCommand
 * - takes in a command, arguments, and an argument count
 * - parses the command to see if it matches any known
 *   built in commands, if there is a match it runs the 
 *   built in command with the supplied arguments (returns 1)
 *   else it returns a 0
 */
int runBuiltInCommand(char *command, char **args, int argsct){

 /*  exit  */
  if(strcmp(command, "exit") == 0){

    go = 0;
  }
  /*  Which */
  else if(strcmp(command, "which") == 0){
     printf("Executing built-in [which]\n");
     if(argsct < 2){
       fprintf(stderr, "Usage: which <command>\n");
     }else{
       int count;
       for(count = 1; count < argsct; count++){
        printf("%s\n", which(args[count], pathlist));
        
       }
     }
  }
  /*  Where */
  else if(strcmp(command, "where") == 0){
     printf("Executing built-in [where]\n");
     if(argsct < 2){
       fprintf(stderr, "Usage: where <command>\n");
     }else{
       int count;
       for(count = 1; count < argsct; count++){
       where(args[count], pathlist);
       }
     }
  }
  /*  cd  */
  else if(strcmp(command, "cd") == 0){
    printf("Executing built-in [cd]\n");
    if(argsct > 2){
       fprintf(stderr, "Usage: cd <directory>\n");
     }else{
       cd(args[1], cwd, homedir, prev_wd);
     }
  }
  /*  pwd  */
  else if(strcmp(command, "pwd") == 0){
     printf("Executing built-in [pwd]\n");
     if(argsct !=1){
       fprintf(stderr, "Usage: pwd\n");
     }else{
       printf("%s\n", cwd); //doesn't need a function necessarily
     }
     
  }
  /*  list*/
  else if(strcmp(command, "list") == 0){
     printf("Executing built-in [list]\n");
     if ((argsct-1) == 0){
       list(NULL,cwd);
     }else{
       int count;
       for(count = 1; count < argsct; count++){
         list(args[count], cwd);
       }
     }
  }
  /*  pid */
  else if(strcmp(command, "pid") == 0){
    printf("Executing built-in [pid]\n");
     printf("%d\n", getpid());
  }
  /*  kill  */
  else if(strcmp(command, "kill") == 0){
    printf("Executing built-in [kill]\n");
    int const DEFAULTSIGNAL = SIGTERM; //signum 15
    
    if(argsct < 2 || 3 < argsct ){
      fprintf(stderr, "Usage: kill [options] <pid>\n");
    }else{
      int rval, pid;
      
      if(argsct == 2){
        pid = atoi(args[1]);
        rval = kill(pid, DEFAULTSIGNAL);
      }else{ //user supplied a signum to use
        pid = atoi(args[2]);
        //cast the signal number
        char *signum = (char*)malloc((strlen(args[1]) + 1) * sizeof(char));
        strcpy(signum, args[1]);
        signum++; //truncate the leading -
        
        rval = kill(pid, atoi(signum));
        
        signum--;
        free(signum);//finished with signum
      }
      
      printf("pid is: %d\n", pid);
      
      if(rval != 0 ){ 
        //there was an issue, print out the error message
        fprintf(stderr, "kill: (%d) - %s\n", pid, strerror(errno));
      }
    }
  }
  /*  prompt  */
  else if(strcmp(command, "prompt") == 0){
     printf("Executing built-in [prompt]\n");
     if(argsct > 2 || strlen(command) > PROMPTMAX){
       fprintf(stderr, "Usage: prompt <prefix>\n");
     }else{
       if (args[1] == NULL){
         printf("input prompt prefix: ");
         fgets(prompt, 256, stdin);
         prompt[strlen(prompt) - 1] = ' ';
         prompt[strlen(prompt)] = '\0';
       }else{
         strcpy(prompt, args[1]);
         prompt[strlen(args[1])] = ' ';
         prompt[strlen(args[1]) + 1] = '\0';
       }
     }
  }
  /*  printenv  */
  else if(strcmp(command, "printenv") == 0){
     printf("Executing built-in [printenv]\n");
     if(argsct > 2){
       fprintf(stderr, "Usage: printenv [VARIABLE]\n");
     }else if (argsct == 2){
       char* variable = getenv(args[1]);
       if(variable == NULL){
         printf("No match found for %s\n", args[1]);
       }else{
         printf("%s\n", variable);
       }
     }else{
        char **current;
        for(current = environ; *current; current++) {
            printf("%s\n", *current);
        }
     }
  }
  /*  alias */
  else if(strcmp(command, "alias") == 0){
     printf("Executing built-in [alias]\n");
     
     if(argsct == 1){
       //print out all alias entries
       sm_enum(alias_table, printAliasTable, NULL);
     }else{
       addAlias(alias_table, args, argsct);
     }
  }
  /*  history */
  else if(strcmp(command, "history") == 0){
     printf("Executing built-in [history]\n");
     
     int temp = 0;
     int offset = historyct - DEFAULTHISTORY;
     if(offset < 0 || historyct < offset ){
       offset = 0;
     }
     
     if(argsct > 2){
       fprintf(stderr, "Usage: history [number]");
     }else if(argsct == 2){
        int temp = atoi(args[1]);
        if(historyct < temp){
          temp = historyct;
        }
        offset = historyct - temp;
     }
     
     int i;
     for(i = offset; i < historyct; i++){
       printf("%s\n", history[i]);
     }
  }
  /*  warnload  */
  else if(strcmp(command, "warnload") == 0){
    printf("Executing built-in [warnload]\n");
    if( argsct > 2){
      fprintf(stderr, "warnload: Too many arguments.\n"); 
    }else if(argsct < 2){
      fprintf(stderr, "Usage: warnload <value>\n"); 
    }else if (argsct == 2){
      printf("I am running warnload correctly\n");
      if(warnlevel == -1){
	int i = 0;
	pthread_t tid1;
	warnlevel = atof(args[1]);
	printf("warnlevel is %f\n",warnlevel);
	pthread_create(&tid1, NULL, myWarnloadThread, "Thread 1");
      }else{
	warnlevel = atof(args[1]);
	printf("warnlevel is %f\n",warnlevel);
      }
    }
  }
  /*  setenv  */
  else if(strcmp(command, "setenv") == 0){
     printf("Executing built-in [setenv]\n");
     if(argsct > 3){
       fprintf(stderr, "setenv: Too many arguments.");
       return;
     }else if(argsct == 1){
       char *temp[1];
       temp[0] = "printenv";
       int count = 1;
       runBuiltInCommand("printenv", temp, count);
       return;
     }else if(argsct == 2){
     
      args[2] = '\0';
       
     }
       
     //do the actual setting of the environment variables
     int overwrite = 1; //1 = overwrite, 0 = don't overwrite
     int rval;
     rval = setenv(args[1], args[2], overwrite);
     if( rval != 0){
       fprintf(stderr, "%s\n", strerror(errno));
       return;
     }
     
     if(strcmp("HOME", args[1]) == 0){
       strcpy(homedir, args[2]);
       homedir[strlen(args[2])] = '\0';
       printf("New HOME = %s\n", homedir);
     }
     
     //free the old pathlist before getting the new one
     struct pathelement *temp = pathlist;
     struct pathelement *curr = pathlist;
     
     if(curr != NULL){
       while(curr != NULL){
         curr = curr->next;
         free(temp);
         temp = curr;
       }
       free(temp); // since it exits before freeing the final node
     }
     
     //update the pathlist
     pathlist = get_path();
  }
  /*  Noclobber */
  else if(strcmp(command, "noclobber") == 0){
    
    printf("Executing built-in [noclobber]\n");
    
    if(argsct < 1){
      printf("Too many arguments\n");
    }

    if(noclobber){
      noclobber = 0;
    } else{
      noclobber = 1;
    }
  }
  /*  No command matched */
  else{
    return 0; // status code 0 indicates no command matched
  }
  return 1; // one of the commands matched
}

/*
 * Signal Handlers
 */

/*
* Interupt Signal Handler
* - checks if there is a child running and passes along
*   the signal to the child, also sets a flag
*
*/
void shell_INT_handler(sig){

  signal(SIGINT, SIG_IGN);
  
  if(child_pid){
    kill((-1*child_pid), sig);
  } else {
    interruptFlag = 1;
  }
}


/*
* Alarm Signal Handler
* This function handles the alarm signal
* An alarm is set to trigger the timeout if a command is taking too long
*/
void shell_ALARM_handler(int sig)
{
  signal(SIGALRM, SIG_IGN);          /* ignore this signal       */
  //if there is a child running, lets destroy him
  if(child_pid){
    timeout_completed = 1;
    kill(child_pid, SIGKILL);
  }
}







