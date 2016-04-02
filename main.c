/*
 * Melody Lugo and Kyle Sullivan
 * Assignment #2 - main.c
 */

#include "sh.h"
#include <signal.h>
#include <stdio.h>
#include <errno.h>

void sig_handler(int signal); 

int main( int argc, char **argv, char **envp )
{
    /* put signal set up stuff here */
    if (signal(SIGINT, sig_handler) == SIG_ERR) {
      printf("SIGINT install error (%s)\n", strerror(errno));
      exit(1);
    }
    
    if (signal(SIGALRM, sig_handler) == SIG_ERR) {
      printf("SIGINT install error (%s)\n", strerror(errno));
      exit(1);
    }

    return sh(argc, argv, envp);
}

/**
 * sig_handler
 * - handles any signals which the shell recieves while it is running
 */
void  sig_handler(int sig)
{
    if(sig == SIGINT){
        shell_INT_handler(sig);
    }else if(sig == SIGALRM){
        shell_ALARM_handler(sig);
    }
    
    //catch any other signals
    signal(sig, sig_handler);
}


