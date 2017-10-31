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
  struct my_clock total_cpu_time;      // Total CPU time used
  struct my_clock total_sys_time;      // Total time in the system
  struct my_clock last_burst_time;     // Time used during the last burst
  int priority;                        // May not have a priority
  unsigned char ready_to_terminate;    // Flag signaling process is ready to terminate
};


/*
 * Currently Scheduled Process
 * Contains information for the currently scheduled process.
 * ---------------------------------------------------------*/
struct curr_sched {
  unsigned int proc_id;       // The currently scheduled process
  unsigned int time_quantum;  // An indivisable amount of time to run for
};

#endif