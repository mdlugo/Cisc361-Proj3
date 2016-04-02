#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>

void forkthem(int n) {
    if(n > 0){
        fork();
        forkthem( n - 1);
    }else{
        printf("a");
    }
}

int calculate(int n){
   int answer = 1;
   int forkanswer;
   if(n > 0){
    forkanswer = calculate(n-1);
    answer = forkanswer + calculate(n-1);
   }
   return answer;
}

void question9(){
    int child = fork();
    int x = 5;
    
    if( child == 0){
        x += 5;
    } else {
        child = fork();
        x += 10;
        if(child) {
            x += 5;
        }
    }
    
    printf("X is: %d, and x addr is %p\n", x, &x);
}

void forktest(){
    int child_pid = fork();
    if ( child_pid == 0){
        printf("I am process #%d\n", getpid());
        printf("pid addr is: %p\n", &child_pid);
        return;
    }else {
        printf("I am the parent of process #%d\n", child_pid);
        printf("pid addr is: %p\n", &child_pid);
        return;
    }
}

int question10part1(){
    int val = 5;
    int pid;
    
    if(pid = fork())
        wait(pid);
    val++;
    printf("%d\n", val);
    return val;
}

int question10part2(){
    int val = 5;
    int pid;
    
    if(pid = fork())
        wait(pid);
    else
        exit(val);
    val++;
    printf("%d\n", val);
    return val;
}


long timediff(clock_t t1, clock_t t2) {
    long elapsed;
    elapsed = ((double)t2 - t1) / CLOCKS_PER_SEC * 1000;
    return elapsed;
}

void testtime(){
    struct timeval start, end;
    long mtime, secs, usecs; 
    gettimeofday(&start, NULL);
    sleep(5);
    gettimeofday(&end, NULL);
    secs  = end.tv_sec  - start.tv_sec;
    usecs = end.tv_usec - start.tv_usec;
    mtime = ((secs) + usecs/1000.0) + 0.5;
    printf("Elapsed time: %ld millisecs\n", mtime);
    
        
}

int main(int argc, char **argv) {
    printf("I think there are %d processes created\n", calculate(5));
    //forkthem(5);
    //question9();
    //forktest();
    //question10part1();
    //question10part2();
    testtime();
}