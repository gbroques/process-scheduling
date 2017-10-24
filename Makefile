CC = gcc
CFLAGS = -g -Wall -I.
EXECS = oss user

all: $(EXECS)

oss: structs.h sem.c

user: structs.h sem.c

clean:
	rm -f *.o $(EXECS)
