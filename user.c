#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/sem.h>
#include <time.h>
#include <ctype.h>

unsigned int* clock_shared_memory;

int main(int argc, char* argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s clock_segment_id\n", argv[0]);
    return EXIT_FAILURE;
  }

  const int clock_segment_id = atoi(argv[1]);
  fprintf(stderr, "Child %d running...\n", getpid());

  clock_shared_memory = (unsigned int*) shmat(clock_segment_id, 0, 0);

  shmdt(clock_shared_memory);

  return EXIT_SUCCESS;
}