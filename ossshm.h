#ifndef OSSSHM_H
#define OSSSHM_H

#include "structs.h"
#include "myclock.h"

/**
 * Operating System Simulator Shared Memory
 */

int get_clock_shm(void);
struct my_clock* attach_to_clock_shm(int id);
void detach_from_clock_shm(struct my_clock* shm);

int get_pcb_shm(int num_blocks);
struct pcb* attach_to_pcb_shm(int id);
void detach_from_pcb_shm(struct pcb* shm);

int get_curr_sched_shm(void);
struct curr_sched* attach_to_curr_sched_shm(int id);
void detach_from_curr_sched_shm(struct curr_sched* shm);

#endif