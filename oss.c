/**
 * Operating System Simulator
 *
 * Copyright (c) 2017 G Brenden Roques
 */

#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>
#include <sys/queue.h>
#include "structs.h"
#include "sem.h"
#include "oss.h"
#include "ossshm.h"

/*
 * CONSTANTS
 *-----------*/

// Maximum number of running processes at any given time
#define MAX_RUNNING_PROCS 18

// 1 * 10^9 nano seconds
#define NANO_SECS_PER_SEC 1000000000

// Total number of processes to be created
#define MAX_PROCS 18


/*
 * GLOBALS
 *-----------*/
static unsigned int clock_seg_id;
static struct my_clock* clock_shm;

static unsigned int pcb_seg_id;
static struct pcb** pcb_shm;

static unsigned int curr_sched_seg_id;
static struct curr_sched* curr_sched_shm;

static char pcb_shm_ids[MAX_RUNNING_PROCS];

static struct my_clock last_created_at;

int num_procs_completed = 0;
int num_procs_generated = 0;
static int sem_id;

TAILQ_HEAD(tailhead, entry) high_prio_queue =
    TAILQ_HEAD_INITIALIZER(high_prio_queue);

struct tailhead *headp;  /* Tail queue head. */

struct entry {
  pid_t pid;
  TAILQ_ENTRY(entry) entries;  /* Tail queue. */
} *n1, *n2;

void print_queue() {
  struct entry* np;
  printf("[OSS] Queue: [ ");
  TAILQ_FOREACH_REVERSE(np, &high_prio_queue, tailhead, entries)
      printf("%d ", np->pid);

  printf("]\n");
}

static void free_queue();

int main(int argc, char* argv[]) {
  int help_flag = 0;
  char* log_file = "oss.out";
  opterr = 0;
  int c;

  while ((c = getopt(argc, argv, "hl:")) != -1) {
    switch (c) {
      case 'h':
        help_flag = 1;
        break;
      case 'l':
        log_file = optarg;
        break;
      case '?':
        if (is_required_argument(optopt)) {
          print_required_argument_message(optopt);
        } else if (isprint(optopt)) {
          fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        } else {
          fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
        }
        return EXIT_FAILURE;
      default:
        abort();
    }
  }

  if (help_flag) {
    print_help_message(argv[0], log_file);
    exit(EXIT_SUCCESS);
  }

  if (setup_interrupt() == -1) {
    perror("Failed to set up handler for SIGPROF");
    return EXIT_FAILURE;
  }

  srand(time(NULL));

  int k;
  for (k = 0; k < MAX_RUNNING_PROCS; k++) {
    pcb_shm_ids[k] = 0;
  }

  TAILQ_INIT(&high_prio_queue);

  FILE* fp;
  fp = fopen(log_file, "w+");

  if (fp == NULL) {
    perror("Failed to open log file");
    exit(EXIT_FAILURE);
  }

  signal(SIGINT, free_shm_and_abort);

  sem_id = allocate_sem(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

  int init_sem_success = init_sem(sem_id, 0);
  if (init_sem_success == -1) {
    perror("Faled to initilaize binary semaphore");
    return EXIT_FAILURE;
  }

  clock_seg_id = get_clock_shm();
  clock_shm = attach_to_clock_shm(clock_seg_id);

  // Initialize clock to 1 second to simulate overhead
  clock_shm->secs = 1;

  pcb_seg_id = get_pcb_shm(MAX_RUNNING_PROCS);
  pcb_shm = attach_to_pcb_shm(pcb_seg_id);

  curr_sched_seg_id = get_curr_sched_shm();
  curr_sched_shm = attach_to_curr_sched_shm(curr_sched_seg_id);
  curr_sched_shm->proc_id = -1;

  fork_and_exec_child(0);

  struct my_clock create_at;
  create_at.secs = (rand() % 3) + clock_shm->secs;
  create_at.nano_secs = clock_shm->nano_secs;
  while (1) {
    update_clock_secs(clock_shm);
    int proc_id = get_proc_id();  // -1 if process table is full
    if (num_procs_completed >= MAX_PROCS && is_queue_empty()) {
      break;
    } else if (is_past_last_created(create_at) && proc_id != -1 && num_procs_generated < MAX_PROCS) {
      fork_and_exec_child(proc_id);
      last_created_at.secs = clock_shm->secs;
      last_created_at.nano_secs = clock_shm->nano_secs;
      create_at.secs = (rand() % 3) + clock_shm->secs;
      create_at.nano_secs = clock_shm->nano_secs;
      clock_shm->nano_secs += 2;
    } else if (!is_queue_empty()) {
        int scheduled_pid = schedule_process();
        clock_shm->nano_secs += rand() % 1001;
        sem_wait(sem_id);
        num_procs_completed++;
        pcb_shm_ids[scheduled_pid] = 0;
    } else {
      clock_shm->nano_secs += rand() % 1001;
    }
  }

  free_shm();
  free_queue();
  fclose(fp);

  return EXIT_SUCCESS;
}

/**
 * Frees all allocated shared memory
 */
static void free_shm(void) {
  detach_from_clock_shm(clock_shm);
  shmctl(clock_seg_id, IPC_RMID, 0);

  detach_from_pcb_shm(pcb_shm);
  shmctl(pcb_seg_id, IPC_RMID, 0);

  detach_from_curr_sched_shm(curr_sched_shm);
  shmctl(curr_sched_seg_id, IPC_RMID, 0);

  int deallocate_sem_success = deallocate_sem(sem_id);
  if (deallocate_sem_success == -1) {
    perror("Failed to deallocate binary semaphore");
    exit(EXIT_FAILURE);
  }
}

/**
 * Free shared memory and abort program
 */
static void free_shm_and_abort(int s) {
  free_shm();
  abort();
}


/**
 * Set up the interrupt handler.
 */
static int setup_interrupt(void) {
  struct sigaction act;
  act.sa_handler = free_shm_and_abort;
  act.sa_flags = 0;
  return (sigemptyset(&act.sa_mask) || sigaction(SIGPROF, &act, NULL));
}

/**
 * Prints a help message.
 * The parameters correspond to program arguments.
 *
 * @param executable_name Name of the executable
 * @param log_file Name of the log file
 */
static void print_help_message(char* executable_name,
                               char* log_file) {
  printf("Operating System Simulator\n\n");
  printf("Usage: ./%s\n\n", executable_name);
  printf("Arguments:\n");
  printf(" -h  Show help.\n");
  printf(" -l  Specify the log file. Defaults to '%s'.\n", log_file);
}

/**
 * Determiens whether optopt is a reguired argument.
 *
 * @param optopt Value returned by getopt when there's a missing option argument.
 * @return 1 when optopt is a required argument and 0 otherwise.
 */
static int is_required_argument(char optopt) {
  switch (optopt) {
    case 'l':
      return 1;
    default:
      return 0;
  }
}

/**
 * Prints a message for a missing option argument.
 * 
 * @param option The option that was missing a required argument.
 */
static void print_required_argument_message(char option) {
  switch (option) {
    case 'l':
      fprintf(stderr, "Option -%c requires the name of the log file.\n", optopt);
      break;
  }
}


static void fork_and_exec_child(int proc_id) {
  pcb_shm_ids[proc_id] = 1;
  num_procs_generated++;
      fprintf(
    stderr,
    "[OSS] [%02d:%010d] NUM PROCS GENERATED %d\n",
    clock_shm->secs,
    clock_shm->nano_secs,
    num_procs_generated
  );
  enqueue_process(proc_id, 0);
  pid_t pid = fork();

  if (pid == -1) {
    perror("Failed to fork");
    exit(EXIT_FAILURE);
  }

  if (pid == 0) { // Child
    char proc_id_string[12];
    char clock_seg_id_string[12];
    char pcb_seg_id_string[12];
    char curr_sched_seg_id_string[12];
    char sem_id_string[12];
    sprintf(proc_id_string, "%d", proc_id);
    sprintf(clock_seg_id_string, "%d", clock_seg_id);
    sprintf(pcb_seg_id_string, "%d", pcb_seg_id);
    sprintf(curr_sched_seg_id_string, "%d", curr_sched_seg_id);
    sprintf(sem_id_string, "%d", sem_id);

    execlp(
      "user",
      "user",
      proc_id_string,
      clock_seg_id_string,
      pcb_seg_id_string,
      curr_sched_seg_id_string,
      sem_id_string,
      (char*) NULL
    );
    perror("Failed to exec");
    _exit(EXIT_FAILURE);
  }
}

static int is_past_last_created(struct my_clock create_at) {
  int sec_past = create_at.secs - clock_shm->secs;
  int ns_past = create_at.nano_secs - clock_shm->nano_secs;
  int total_past = ns_past + (sec_past * NANO_SECS_PER_SEC);
  if (total_past < 0) {
    return 1;
  } else {
    return 0;
  }
}

/**
 * Get a currently available process ID.
 *
 * @return The process ID. -1 if the process table is full.
 */
static int get_proc_id() {
  int proc_id;
  for (proc_id = 0; proc_id < MAX_RUNNING_PROCS; proc_id++) {
    if (pcb_shm_ids[proc_id] != 1) {
      return proc_id;
    }
  }
  return -1;
}

static void update_clock_secs(struct my_clock* clock) {
  if (clock->nano_secs >= NANO_SECS_PER_SEC) {
    clock->secs += 1;
    clock->nano_secs -= NANO_SECS_PER_SEC;
  }
}

static int schedule_process() {
  int pid = dequeue_process(0);
  if (pid != -1) {
    curr_sched_shm->proc_id = pid;
    curr_sched_shm->time_quantum = MY_TIMESLICE;
  }
  return pid;
}

static void enqueue_process(int proc_id, int priority) {
  struct entry* proc = malloc(sizeof(struct entry));
  proc->pid = proc_id;
  switch (priority) {
    case 0:
      printf("[OSS] Enqueue child %d\n", proc_id);
      TAILQ_INSERT_TAIL(&high_prio_queue, proc, entries);
      print_queue();
      break;
    case 1:
      break;
    case 2:
      break;
  }
}

static int dequeue_process(int priority) {
  int pid = -1;
  struct entry* np;
  switch (priority) {
    case 0:
      np = TAILQ_FIRST(&high_prio_queue);
      if (np != NULL) {
        pid = np->pid;
        printf("[OSS] Dequeue child %d\n", pid);
        TAILQ_REMOVE(&high_prio_queue, np, entries);
        free(np);
        print_queue();
      }
      break;
    case 1:

      break;
    case 2:

      break;

  }
  return pid;
}

static int is_queue_empty() {
  return TAILQ_EMPTY(&high_prio_queue);
}

static void free_queue() {
  n1 = TAILQ_FIRST(&high_prio_queue);
  while (n1 != NULL) {
    n2 = TAILQ_NEXT(n1, entries);
    free(n1);
    n1 = n2;
  }
}
