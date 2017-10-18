CC = gcc
CFLAGS = -g -Wall -I.
EXECS = oss user

all: $(EXECS)

oss: structs.h

user: structs.h

clean:
	rm -f *.o $(EXECS)
