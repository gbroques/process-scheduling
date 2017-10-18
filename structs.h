#ifndef STRUCTS_H
#define STRUCTS_H

/**
 * Structures
 */

struct my_clock {
  unsigned int seconds;
  unsigned int nano_seconds;
};

/**
 * Process Control Block (PCB)
 * Contains information for scheduling child processes.
 */
struct pcb {
  struct my_clock total_time_used;     // Total CPU time used
  struct my_clock total_system_time;   // Total time in the system
  struct my_clock last_burst_time;     // Time used during the last burst
  int priority;              // May not have a priority
};

#endif