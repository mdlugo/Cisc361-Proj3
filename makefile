# choose your compiler
CC=gcc
#CC=gcc -Wall

mysh: sh.o get_path.o strmap.o main.c 
	$(CC) -g main.c sh.o get_path.o strmap.o -o mysh -lkstat -DHACE_KSTAT

sh.o: sh.c sh.h
	$(CC) -g -c sh.c

strmap.o: strmap.c strmap.h
	$(CC) -g -c strmap.c 

get_path.o: get_path.c get_path.h
	$(CC) -g -c get_path.c

clean:
	rm -rf sh.o get_path.o strmap.o mysh
	rm -f $(PROGS)$(TEMPFILES)*.o