#ifndef SEM_H
#define SEM_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

union semun {
  int val;                /* Value for SETVAL */
  struct semid_ds *buf;   /* Buffer for IPC_STAT, IPC_SET */
  unsigned short *array;  /* Array for GETALL, SETALL */
  struct seminfo *__buf;  /* Buffer for IPC_INFO (Linux-specific) */
};

int allocate_sem(key_t key, int sem_flags);
int deallocate_sem(int sem_id);
int init_sem(int sem_id, int initial_val);
int sem_wait(int sem_id);
int sem_post(int sem_id);

#endif