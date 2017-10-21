#ifndef STRUCTS_H
#define STRUCTS_H

/**
 * Structures
 */

/**
 * A simple simulated clock.
 */
struct my_clock {
  unsigned int secs;       // Amount of time in seconds
  unsigned int nano_secs;  // Amount of time in nano seconds
};

/**
 * Process Control Block (PCB)
 * Contains information for scheduling child processes.
 */
struct pcb {
  struct my_clock total_time_used;     // Total CPU time used
  struct my_clock total_system_time;   // Total time in the system
  struct my_clock last_burst_time;     // Time used during the last burst
  int priority;                        // May not have a priority
};

/**
 * Schedule Block
 * Contains information for the currently scheduled process.
 */
struct sched_block {
  unsigned int proc_id;       // The currently scheduled process
  unsigned int time_quantum;  // An indivisable amount of time to run for
};

#endif