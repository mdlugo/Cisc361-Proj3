#include <unistd.h>
#include <stdio.h>

int main(void){
    char *argv[] = {"/bin/ls", "*", 0};
    
    execve (argv[0], &argv[0], NULL);
}