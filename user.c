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
#include <time.h>
#include "sem.h"
#include "structs.h"

struct my_clock* clock_shm;
struct pcb** pcb_shm;
struct curr_sched* curr_sched_shm;

int main(int argc, char* argv[]) {
  if (argc != 6) {
    fprintf(
      stderr,
      "Usage: %s proc_id clock_seg_id pcb_seg_id curr_sched_seg_id sem_id\n",
      argv[0]
    );
    return EXIT_FAILURE;
  }
  const int proc_id = atoi(argv[1]);
  const int clock_seg_id = atoi(argv[2]);
  const int pcb_seg_id = atoi(argv[3]);
  const int curr_sched_seg_id = atoi(argv[4]);
  const int sem_id = atoi(argv[5]);

  clock_shm = (struct my_clock*) shmat(clock_seg_id, 0, 0);
  pcb_shm = (struct pcb**) shmat(pcb_seg_id, NULL, 0);
  curr_sched_shm = (struct curr_sched*) shmat(curr_sched_seg_id, NULL, 0);

  fprintf(stderr, "[USR] [%02d:%010d] Child %d waiting ready queue\n", clock_shm->secs, clock_shm->nano_secs, proc_id);

  while (curr_sched_shm->proc_id != proc_id);

  fprintf(stderr, "[USR] [%02d:%010d] Child %d scheduled to run for %d nano seconds\n", clock_shm->secs, clock_shm->nano_secs, proc_id, curr_sched_shm->time_quantum);
  sem_post(sem_id);

  shmdt(clock_shm);
  shmdt(pcb_shm);
  shmdt(curr_sched_shm);

  return EXIT_SUCCESS;
}