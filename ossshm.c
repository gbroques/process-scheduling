#include <stdlib.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include "ossshm.h"

/**
 * Allocates shared memory for a simulated clock.
 * 
 * @return The shared memory segment ID
 */
int get_clock_shm(void) {
  int id = shmget(IPC_PRIVATE, sizeof(struct my_clock),
    IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

  if (id == -1) {
    perror("Failed to get shared memory for clock");
    exit(EXIT_FAILURE);
  }
  return id;
}

/**
 * Attaches to the clock shared memory segment.
 * 
 * @return A pointer to the clock in shared memory.
 */
struct my_clock* attach_to_clock_shm(int id) {
  void* clock_shm = shmat(id, NULL, 0);

  if (*((int*) clock_shm) == -1) {
    perror("Failed to attach to clock shared memory");
    exit(EXIT_FAILURE);
  }

  return (struct my_clock*) clock_shm;
}

/**
 * Allocates shared memory for multiple process control blocks.
 * 
 * @return The shared memory segment ID
 */
int get_pcb_shm(int num_blocks) {
  int segment_size = sizeof(struct pcb) * num_blocks;
  int id = shmget(IPC_PRIVATE, segment_size,
    IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

  if (id == -1) {
    perror("Failed to get shared memory for process control blocks");
    exit(EXIT_FAILURE);
  }
  return id;
}

/**
 * Attaches to the shared memory segment for process control blocks.
 * 
 * @return A pointer to an array of process control blocks in shared memory.
 */
struct pcb** attach_to_pcb_shm(int id) {
  void* pcb_shm = shmat(id, NULL, 0);

  if (*((int*) pcb_shm) == -1) {
    perror("Failed to attach to process control block shared memory");
    exit(EXIT_FAILURE);
  }

  return (struct pcb**) pcb_shm;
}

/**
 * Allocates shared memory for the currently scheduled process.
 * See curr_sched definition in structs.h
 * 
 * @return The shared memory segment ID
 */
int get_curr_sched_shm(void) {
  int id = shmget(IPC_PRIVATE, sizeof(struct curr_sched),
    IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

  if (id == -1) {
    perror("Failed to get shared memory for currently scheduled process");
    exit(EXIT_FAILURE);
  }
  return id;
}

/**
 * Attaches to the shared memory segment for the schedule block.
 * 
 * @return A pointer to the schedule block in shared memory.
 */
struct curr_sched* attach_to_curr_sched_shm(int id) {
  void* curr_sched_shm = shmat(id, NULL, 0);

  if (*((int*) curr_sched_shm) == -1) {
    perror("Failed to attach to currently scheduled process shared memory");
    exit(EXIT_FAILURE);
  }

  return (struct curr_sched*) curr_sched_shm;
}

void detach_from_clock_shm(struct my_clock* shm) {
  int return_value = shmdt(shm);
  if (return_value == -1) {
    perror("Failed to detach from clock shared memory");
  }
}

void detach_from_pcb_shm(struct pcb** shm) {
  int return_value = shmdt(shm);
  if (return_value == -1) {
    perror("Failed to detach from process control block shared memory");
  }
}

void detach_from_curr_sched_shm(struct curr_sched* shm) {
  int return_value = shmdt(shm);
  if (return_value == -1) {
    perror("Failed to detach from currnelty scheduled process shared memory");
  }
}