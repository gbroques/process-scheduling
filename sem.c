#include "sem.h"

int allocate_sem(key_t key, int sem_flags) {
  return semget(key, 1, sem_flags);
}

int deallocate_sem(int sem_id) {
  union semun ignored_argument;
  return semctl(sem_id, 1, IPC_RMID, ignored_argument);
}

int init_sem(int sem_id, int initial_val) {
  union semun argument;
  unsigned short values[1];
  values[0] = initial_val;
  argument.array = values;
  return semctl(sem_id, 0, SETALL, argument);
}

/**
 * Wait on a binary semaphore.
 * Block until the semaphore value is positive,
 * then decrement it by 1.
 * 
 * @param sem_id The semaphore's id
 * @return The return value of semop
 */
int sem_wait(int sem_id) {
  struct sembuf operations[1];
  /* Use the first (and only) semaphore. */
  operations[0].sem_num = 0;
  /* Decrement by 1. */
  operations[0].sem_op = -1;
  /* Permit undo'ing. */
  operations[0].sem_flg = SEM_UNDO;
  return semop(sem_id, operations, 1);
}

/**
 * Post to a binary semaphore: increment its value by 1.
 * This returns immediately.
 * 
 * @param sem_id The semaphore's id
 * @return The return value of semop
 */
int sem_post(int sem_id) {
  struct sembuf operations[1];
  /* Use the first (and only) semaphore. */
  operations[0].sem_num = 0;
  /* Increment by 1. */
  operations[0].sem_op = 1;
  /* Permit undo'ing. */
  operations[0].sem_flg = SEM_UNDO;
  return semop(sem_id, operations, 1);
}