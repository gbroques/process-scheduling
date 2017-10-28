CC = gcc
CFLAGS = -g -Wall -I.
EXECS = oss user

all: $(EXECS)

oss: structs.h sem.c ossshm.c

user: structs.h sem.c ossshm.c

clean:
	rm -f *.o $(EXECS)
