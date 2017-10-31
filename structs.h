#ifndef STRUCTS_H
#define STRUCTS_H

#include "myclock.h"

/**************
 * STRUCTURES *
 **************/

/*
 * Process Control Block (PCB)
 * Contains information for scheduling child processes.
 * ----------------------------------------------------*/
struct pcb {
  // Total CPU time used
  struct my_clock total_cpu_time;

  // Total time in the system
  struct my_clock total_sys_time;

  // Time used during the last burst in nanoseconds
  unsigned int last_burst_time;

  int priority;

  // Flag signaling if the program was interrupted
  unsigned char was_interrupted;

  // Time left to run if process was interrupted
  unsigned int remaining_time; // (in nanoseconds)

  // Flag signaling process is ready to terminate
  unsigned char ready_to_terminate;
};


/*
 * Currently Scheduled Process
 * Contains information for the currently scheduled process.
 * ---------------------------------------------------------*/
struct curr_sched {
  unsigned int proc_id;        // The currently scheduled process
  unsigned int time_quantum;   // An indivisable amount of time to run for
  unsigned int rand_sched_num; // See get_rand_sched_num() for details
};

#endif