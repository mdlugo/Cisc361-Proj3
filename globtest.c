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
#include "sh.h"
#include "strmap.h"

int main( int argc, char **argv)
{
  
    char *args[] = {"/bin/ls", "-l", "*.c", 0};
    int argsct = 3;
    
    glob_t globbuf;
    globbuf.gl_offs = 0;
    
    int i;
    for(i = 0; i < argsct; i++){
        glob(args[i], GLOB_DOOFFS, NULL, &globbuf);
    }
    
    execvp("ls", &globbuf.gl_pathv[0]);
    /*
    glob_t globbuf;

    globbuf.gl_offs = 0;
    glob("ls", GLOB_DOOFFS, NULL, &globbuf);
    //glob("../*.c", GLOB_DOOFFS | GLOB_APPEND, NULL, &globbuf);
    globbuf.gl_pathv[0] = "ls";
    globbuf.gl_pathv[1] = "-l";
    char* temp = globbuf.gl_pathv[0];
    int index = 0;
    while(temp != NULL){
        printf("%s\n", temp);
        temp = globbuf.gl_pathv[++index];
    }
    execvp("ls", &globbuf.gl_pathv[0]);
    */
    globfree(&globbuf);

    return 1;
}
