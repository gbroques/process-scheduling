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
#include "myclock.h"

/*
 * CONSTANTS
 *-----------*/

// Maximum number of running processes at any given time
#define MAX_RUNNING_PROCS 18

// Total number of processes to be created
#define MAX_PROCS 3

/*
 * GLOBALS
 *-----------*/
static unsigned int clock_seg_id;
static struct my_clock* clock_shm;

static unsigned int pcb_seg_id;
static struct pcb* pcb_shm;

static unsigned int curr_sched_seg_id;
static struct curr_sched* curr_sched_shm;

static char pcb_shm_ids[MAX_PROCS];

static struct my_clock last_created_at;

int num_procs_completed = 0;
int num_procs_generated = 0;
static int sem_id;

FILE* fp;

TAILQ_HEAD(q1head, entry) high_prio_queue =
    TAILQ_HEAD_INITIALIZER(high_prio_queue);

struct q1head *q1headp;

struct entry {
  pid_t pid;
  TAILQ_ENTRY(entry) entries;
};


TAILQ_HEAD(q2head, entry) med_prio_queue =
    TAILQ_HEAD_INITIALIZER(med_prio_queue);

struct q2head *q2headp;

TAILQ_HEAD(q3head, entry) low_prio_queue =
    TAILQ_HEAD_INITIALIZER(low_prio_queue);

struct q3head *q3headp;

void print_queues() {
  struct entry* np;
  fprintf(fp, "[OSS] Queue 1: [ ");
  TAILQ_FOREACH_REVERSE(np, &high_prio_queue, q1head, entries)
      fprintf(fp, "%d ", np->pid);

  fprintf(fp, "]\n");

  fprintf(fp, "[OSS] Queue 2: [ ");
  TAILQ_FOREACH_REVERSE(np, &med_prio_queue, q2head, entries)
      fprintf(fp, "%d ", np->pid);

  fprintf(fp, "]\n");

  fprintf(fp, "[OSS] Queue 3: [ ");
  TAILQ_FOREACH_REVERSE(np, &low_prio_queue, q3head, entries)
      fprintf(fp, "%d ", np->pid);

  fprintf(fp, "]\n");
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

  fp = fopen(log_file, "w+");

  if (fp == NULL) {
    perror("Failed to open log file");
    exit(EXIT_FAILURE);
  }

  srand(time(NULL));

  int k;
  for (k = 0; k < MAX_PROCS; k++) {
    pcb_shm_ids[k] = 0;
  }

  TAILQ_INIT(&high_prio_queue);
  TAILQ_INIT(&med_prio_queue);
  TAILQ_INIT(&low_prio_queue);

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

  pcb_seg_id = get_pcb_shm(MAX_PROCS);
  pcb_shm = attach_to_pcb_shm(pcb_seg_id);

  curr_sched_seg_id = get_curr_sched_shm();
  curr_sched_shm = attach_to_curr_sched_shm(curr_sched_seg_id);
  // Initialize to a value that won't be equal to a process ID
  curr_sched_shm->proc_id = -10;

  fork_and_exec_child(0);

  struct my_clock create_at;
  create_at.secs = (rand() % 3) + clock_shm->secs;
  create_at.nanosecs = clock_shm->nanosecs;
  while (1) {
    if (clock_shm->nanosecs >= NANOSECS_PER_SEC) {
      clock_shm->secs += 1;
      clock_shm->nanosecs -= NANOSECS_PER_SEC;
    }

    int proc_id = get_proc_id();  // -1 if process table is full
    if (num_procs_completed >= MAX_PROCS && is_queue_empty()) {
      break;
    } else if (is_past_last_created(create_at) && proc_id != -1 && num_procs_generated < MAX_PROCS) {
      fork_and_exec_child(proc_id);
      last_created_at.secs = clock_shm->secs;
      last_created_at.nanosecs = clock_shm->nanosecs;
      create_at.secs = (rand() % 3) + clock_shm->secs;
      create_at.nanosecs = clock_shm->nanosecs;
      clock_shm->nanosecs += 2;
      continue;
    } else if (!is_queue_empty()) {
        int scheduled_pid = dispatch_process();
        clock_shm->nanosecs += rand() % 1001; // Scheduling Overhead
        sem_wait(sem_id);
        fprintf(
          fp,
          "[OSS] [%02d:%010d] Process %d ran for %d nanoseconds during last burst.\n",
          clock_shm->secs,
          clock_shm->nanosecs,
          scheduled_pid,
          pcb_shm[scheduled_pid].last_burst_time
        );
        clock_shm->nanosecs += pcb_shm[scheduled_pid].last_burst_time;
        if (!pcb_shm[scheduled_pid].ready_to_terminate) {
          fprintf(
            fp,
            "[OSS] [%02d:%010d] Putting process %d in queue %d.\n",
            clock_shm->secs,
            clock_shm->nanosecs,
            scheduled_pid,
            0
          );
          enqueue_process(scheduled_pid, 0);
        } else {
          num_procs_completed++;
          fprintf(
            fp,
            "[OSS] [%02d:%010d] Process %d terminated. Number of processes completed %d\n",
            clock_shm->secs,
            clock_shm->nanosecs,
            scheduled_pid,
            num_procs_completed
          );
        }
        // pcb_shm_ids[scheduled_pid] = 0;
    } else {
      clock_shm->nanosecs += rand() % 1001;
    }
  }

  // Wait for any remaining children to terminate
  pid_t pid;
  while ((pid = waitpid(-1, NULL, 0))) {
    if (errno == ECHILD) {
      break;
    }
  }

  print_report();

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

  int priority = 0;
  fprintf(
    fp,
    "[OSS] [%02d:%010d] Generating process %d and putting it in queue %d\n",
    clock_shm->secs,
    clock_shm->nanosecs,
    proc_id,
    priority
  );
  enqueue_process(proc_id, priority);
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
  int ns_past = create_at.nanosecs - clock_shm->nanosecs;
  int total_past = ns_past + (sec_past * NANOSECS_PER_SEC);
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
  for (proc_id = 0; proc_id < MAX_PROCS; proc_id++) {
    if (pcb_shm_ids[proc_id] != 1) {
      return proc_id;
    }
  }
  return -1;
}

static int dispatch_process() {
  int priority = 0;

  fprintf(
    fp,
    "[OSS] [%02d:%010d] Dispatching process %d from queue %d\n",
    clock_shm->secs,
    clock_shm->nanosecs,
    peek(priority),
    priority
  );

  int pid = dequeue_process(0);
  if (pid != -1) {
    curr_sched_shm->proc_id = pid;
    curr_sched_shm->time_quantum = MY_TIMESLICE;
    curr_sched_shm->rand_sched_num = get_rand_sched_num();
  }
  return pid;
}

static void enqueue_process(int proc_id, int priority) {
  pcb_shm[proc_id].priority = priority;
  struct entry* proc = malloc(sizeof(struct entry));
  proc->pid = proc_id;
  switch (priority) {
    case 0:
      TAILQ_INSERT_TAIL(&high_prio_queue, proc, entries);
      print_queues();
      break;
    case 1:
      TAILQ_INSERT_TAIL(&med_prio_queue, proc, entries);
      print_queues();
      break;
    case 2:
      TAILQ_INSERT_TAIL(&low_prio_queue, proc, entries);
      print_queues();
      break;
  }
}

static int peek(int priority) {
  int pid = -1;
  struct entry* np;
  switch (priority) {
    case 0:
      np = TAILQ_FIRST(&high_prio_queue);
      if (np != NULL) {
        pid = np->pid;
      }
      break;
    case 1:
      np = TAILQ_FIRST(&med_prio_queue);
      if (np != NULL) {
        pid = np->pid;
      }
      break;
    case 2:
      np = TAILQ_FIRST(&low_prio_queue);
      if (np != NULL) {
        pid = np->pid;
      }
      break;

  }
  return pid;
}

static int dequeue_process(int priority) {
  int pid = -1;
  struct entry* np;
  switch (priority) {
    case 0:
      np = TAILQ_FIRST(&high_prio_queue);
      if (np != NULL) {
        pid = np->pid;
        TAILQ_REMOVE(&high_prio_queue, np, entries);
        free(np);
        print_queues();
      }
      break;
    case 1:
      np = TAILQ_FIRST(&med_prio_queue);
      if (np != NULL) {
        pid = np->pid;
        TAILQ_REMOVE(&med_prio_queue, np, entries);
        free(np);
        print_queues();
      }
      break;
    case 2:
      np = TAILQ_FIRST(&low_prio_queue);
      if (np != NULL) {
        pid = np->pid;
        TAILQ_REMOVE(&low_prio_queue, np, entries);
        free(np);
        print_queues();
      }
      break;

  }
  return pid;
}

static int is_queue_empty() {
  return TAILQ_EMPTY(&high_prio_queue);
}

static void free_queue() {
  struct entry *n1, *n2;
  n1 = TAILQ_FIRST(&high_prio_queue);
  while (n1 != NULL) {
    n2 = TAILQ_NEXT(n1, entries);
    free(n1);
    n1 = n2;
  }
}

/**
 * Get a random number in the range [1, 3],
 * to simulate the randomness of a real
 * process scheduling environment.
 *
 *   - 1 means the process executes normally
 *   - 2 means the process waits for an event
 *   - 3 means the process gets preempted after using
 *     [1, 99] of its time quantum
 */
static int get_rand_sched_num() {
  int rand_sched_num = rand() % 3 + 1;
  
  // Make 1 a more likely outcome
  int i; 
  while (rand_sched_num != 1 && i < 3) {
    rand_sched_num = rand() % 3 + 1;
    i++;
  }
  return rand_sched_num;
}

/*----------------------------*
 | TODO: Print report         |
 |----------------------------|
 | - Average turnaround time  |
 | - Average wait time        |
 | - How long CPU was idle    |
 *----------------------------*/
static void print_report() {
  struct my_clock total_turnaround_time;
  total_turnaround_time.secs = 0;
  total_turnaround_time.nanosecs = 0;
  struct my_clock total_cpu_time;
  // printf("\n\nGenerating report...\n");
  int i;
  for (i = 0; i < MAX_PROCS; i++) {
    // printf("Process %d | Total System Time %d:%d | Total CPU Time %d:%d \n", i, pcb_shm[i].total_sys_time.secs, pcb_shm[i].total_sys_time.nanosecs, pcb_shm[i].total_cpu_time.secs, pcb_shm[i].total_cpu_time.nanosecs);
    // printf("%d:%d + %d:%d = ", total_turnaround_time.secs, total_turnaround_time.nanosecs, pcb_shm[i].total_sys_time.secs, pcb_shm[i].total_sys_time.nanosecs);
    total_turnaround_time = add_clocks(
                              total_turnaround_time,
                              pcb_shm[i].total_sys_time
                            );
    // printf("%d:%d\n", total_turnaround_time.secs, total_turnaround_time.nanosecs);
    total_cpu_time = add_clocks(
                       total_cpu_time,
                       pcb_shm[i].total_cpu_time
                     );
  }
  // printf("\nTotal turnaround time %d:%d\n\n", total_turnaround_time.secs, total_turnaround_time.nanosecs);
  struct my_clock avg_turnaround_time;
  avg_turnaround_time = divide_clock(total_turnaround_time, MAX_PROCS);
  struct my_clock avg_cpu_time;
  avg_cpu_time = divide_clock(total_cpu_time, MAX_PROCS);
  struct my_clock avg_wait_time;
  avg_wait_time = subtract_clocks(avg_turnaround_time, avg_cpu_time);

  char report_title[33] = "Operating System Simulator Report";
  fprintf(fp, "\n%s\n", report_title);
  int j = 0;
  while (report_title[j] != '\0') {
    fprintf(fp, "-");
    j++;
  }
  fprintf(fp, "\n");
  fprintf(fp, "Average Turnaround Time: %d:%d \n", avg_turnaround_time.secs, avg_turnaround_time.nanosecs);
  fprintf(fp, "Average Wait Time: %d:%d \n", avg_wait_time.secs, avg_wait_time.nanosecs);
}

/**
 * TODO: Write method to calculate average
 *       wait time of processes in queue "X"
 */

/**
 * TODO: Write method to move process to
 *       another queue
 */