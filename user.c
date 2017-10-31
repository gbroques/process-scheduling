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
      "[USR] [%02d:%010d] Process %d waiting in ready queue\n",
      clock_shm->secs,
      clock_shm->nano_secs,
      proc_id
    );

    // Wait to be scheduled
    while (curr_sched_shm->proc_id != proc_id);

    int should_use_full_time_quantum = rand() % 2;

    unsigned int time_quantum;
    if (pcb_shm[proc_id].was_interrupted) {
      pcb_shm[proc_id].was_interrupted = 0;
      time_quantum = pcb_shm[proc_id].remaining_time;
      pcb_shm[proc_id].remaining_time = 0;
      printf(
        "[USR] [%02d:%010d] Process %d resuming. Scheduled to run for %d nanoseconds\n",
        clock_shm->secs,
        clock_shm->nano_secs,
        proc_id,
        time_quantum
      );
    } else if (should_use_full_time_quantum) {
      time_quantum = curr_sched_shm->time_quantum;
      printf(
        "[USR] [%02d:%010d] Process %d scheduled to run for %d nanoseconds\n",
        clock_shm->secs,
        clock_shm->nano_secs,
        proc_id,
        time_quantum
      );
    } else { // Use partial time quantum
      time_quantum = rand() % curr_sched_shm->time_quantum;
      printf(
        "[USR] [%02d:%010d] Process %d scheduled to run for %d nanoseconds\n",
        clock_shm->secs,
        clock_shm->nano_secs,
        proc_id,
        time_quantum
      );
    }

    // Wait for an event
    if (curr_sched_shm->rand_sched_num == 2) {
      int time_ran_for = rand() % time_quantum;
      int time_left_to_run = time_quantum - time_ran_for;
      pcb_shm[proc_id].was_interrupted = 1;
      pcb_shm[proc_id].remaining_time = time_left_to_run;
      time_quantum = rand() % time_quantum;
      printf(
        "[USR] [%02d:%010d] Process %d was interrupted by an event after running for %d nanoseconds\n",
        clock_shm->secs,
        clock_shm->nano_secs,
        proc_id,
        time_ran_for
      );

      // Add wait time to total system time
      struct my_clock wait_time;
      wait_time.secs = rand() % 6;
      wait_time.nano_secs = rand() % 1001;
      pcb_shm[proc_id].total_sys_time = add_clocks(
        pcb_shm[proc_id].total_sys_time,
        wait_time
      );
    }

    // Preempted after using [1, 99] of time quantum
    if (curr_sched_shm->rand_sched_num == 3) {
      int time_ran_for = rand() % 99 + 1;
      int time_left_to_run = time_quantum - time_ran_for;
      pcb_shm[proc_id].was_interrupted = 1;
      pcb_shm[proc_id].remaining_time = time_left_to_run;
      printf(
        "[USR] [%02d:%010d] Process %d was preempted after running for %d nanoseconds\n",
        clock_shm->secs,
        clock_shm->nano_secs,
        proc_id,
        time_ran_for
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

    pcb_shm[proc_id].last_burst_time = time_quantum;

    int accumulated_cpu_time = get_nano_secs(pcb_shm[proc_id].total_cpu_time);

    // AND proccess is supposed to execute normally
    if (accumulated_cpu_time >= FIFTY_MILLISECS &&
        curr_sched_shm->rand_sched_num == 1) {
      is_process_complete = rand() % 2;
      if (is_process_complete) {
        pcb_shm[proc_id].ready_to_terminate = 1;
        printf(
          "[USR] [%02d:%010d] Process %d ready to terminate\n",
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
        "[USR] [%02d:%010d] Process %d NOT ready to terminate\n",
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