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
#include "ossshm.h"
#include "myclock.h"

#define FIFTY_MILLISECS 50000000 // 50 milliseconds in nano seconds

int main(int argc, char* argv[]) {
  if (argc != 6) {
    fprintf(
      stderr,
      "Usage: %s proc_id clock_seg_id pcb_seg_id curr_sched_seg_id sem_id\n",
      argv[0]
    );
    return EXIT_FAILURE;
  }

  srand(time(NULL));

  const int proc_id = atoi(argv[1]);
  const int clock_seg_id = atoi(argv[2]);
  const int pcb_seg_id = atoi(argv[3]);
  const int curr_sched_seg_id = atoi(argv[4]);
  const int sem_id = atoi(argv[5]);

  struct my_clock* clock_shm = attach_to_clock_shm(clock_seg_id);
  struct pcb* pcb_shm = attach_to_pcb_shm(pcb_seg_id);
  struct curr_sched* curr_sched_shm = attach_to_curr_sched_shm(curr_sched_seg_id);

  int is_process_complete = 0;
  pcb_shm[proc_id].ready_to_terminate = 0;
  do {
    struct my_clock start_time;
    start_time.secs = clock_shm->secs;
    start_time.nano_secs = clock_shm->nano_secs;

    printf(
      "[USR] [%d] [%02d:%010d] Child %d waiting in ready queue\n",
      getpid(),
      clock_shm->secs,
      clock_shm->nano_secs,
      proc_id
    );

    // Wait to be scheduled
    while (curr_sched_shm->proc_id != proc_id);

    int should_use_full_time_quantum = rand() % 2;

    unsigned int time_quantum;
    if (should_use_full_time_quantum) {
      time_quantum = curr_sched_shm->time_quantum;
      printf(
        "[USR] [%d] [%02d:%010d] Child %d using full time quantum %d nano seconds\n",
        getpid(),
        clock_shm->secs,
        clock_shm->nano_secs,
        proc_id,
        time_quantum
      );
    } else { // Use partial time quantum
      time_quantum = rand() % curr_sched_shm->time_quantum;
      printf(
        "[USR] [%d] [%02d:%010d] Child %d using partial time quantum %d nano seconds\n",
        getpid(),
        clock_shm->secs,
        clock_shm->nano_secs,
        proc_id,
        time_quantum
      );
    }

    struct my_clock end_time = add_nano_secs_to_clock(*clock_shm, time_quantum);

    int total_sys_time = get_nano_secs_past(end_time, start_time);

    pcb_shm[proc_id].total_sys_time = add_nano_secs_to_clock(
                                         pcb_shm[proc_id].total_sys_time,
                                         total_sys_time
                                       );

    pcb_shm[proc_id].total_cpu_time = add_nano_secs_to_clock(
                                         pcb_shm[proc_id].total_cpu_time,
                                         time_quantum
                                       );

    int accumulated_cpu_time = get_nano_secs(pcb_shm[proc_id].total_cpu_time);

    if (accumulated_cpu_time >= FIFTY_MILLISECS) {
      is_process_complete = rand() % 2;
      if (is_process_complete) {
        pcb_shm[proc_id].ready_to_terminate = 1;

        printf(
          "[USR] [%d] [%02d:%010d] Child %d ready to terminate\n",
          getpid(),
          clock_shm->secs,
          clock_shm->nano_secs,
          proc_id
        );
      }
    }

    // Set to a value that won't be equal to a process ID
    curr_sched_shm->proc_id = -10;
    sem_post(sem_id);

    if (!is_process_complete) {
      printf(
        "[USR] [%d] [%02d:%010d] Child %d NOT ready to terminate\n",
        getpid(),
        clock_shm->secs,
        clock_shm->nano_secs,
        proc_id
      );
    }
  } while (!is_process_complete);

  detach_from_clock_shm(clock_shm);
  detach_from_pcb_shm(pcb_shm);
  detach_from_curr_sched_shm(curr_sched_shm);

  return EXIT_SUCCESS;
}