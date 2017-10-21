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
#include "structs.h"

struct my_clock* clock_shm;
struct pcb** pcb_shm;
struct sched_block* sched_block_shm;

int main(int argc, char* argv[]) {
  if (argc != 5) {
    fprintf(
      stderr,
      "Usage: %s proc_id clock_seg_id pcb_seg_id sched_block_seg_id\n",
      argv[0]
    );
    return EXIT_FAILURE;
  }
  const int proc_id = atoi(argv[1]);
  const int clock_seg_id = atoi(argv[2]);
  const int pcb_seg_id = atoi(argv[3]);
  const int sched_block_seg_id = atoi(argv[4]);

  clock_shm = (struct my_clock*) shmat(clock_seg_id, 0, 0);
  pcb_shm = (struct pcb**) shmat(pcb_seg_id, NULL, 0);
  sched_block_shm = (struct sched_block*) shmat(sched_block_seg_id, NULL, 0);

  fprintf(stderr, "[USR] [%02d:%010d] Child %d generated\n", clock_shm->secs, clock_shm->nano_secs, proc_id);


  // while (sched_block_shm->proc_id != proc_id);

  shmdt(clock_shm);
  shmdt(pcb_shm);


  return EXIT_SUCCESS;
}