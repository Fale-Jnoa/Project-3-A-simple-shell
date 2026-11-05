
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic

OBJS = mysh.o parse.o exec.o

.PHONY: all clean

all: mysh

mysh: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o mysh

mysh.o: mysh.c parse.h job.h
parse.o: parse.c parse.h job.h
exec.o: exec.c job.h

clean:
	rm -f mysh $(OBJS)
