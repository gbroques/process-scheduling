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
#include <sys/sem.h>
#include <time.h>
#include "structs.h"
#include "oss.h"

// CONSTANTS
#define MAX_RUNNING_PROCS 18
#define NANO_SECS_PER_SEC 1000000000
#define TOTAL_PROCS = 100

// GLOBALS
static unsigned int clock_seg_id;
static struct my_clock* clock_shm;

static unsigned int pcb_seg_id;
static struct pcb** pcb_shm;

static unsigned int sched_block_seg_id;
static struct sched_block* sched_block_shm;

static int pcb_shm_ids[MAX_RUNNING_PROCS];

static struct my_clock last_created_at;

int num_procs_completed = 0;

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

  FILE* fp;
  fp = fopen(log_file, "w+");

  if (fp == NULL) {
    perror("Failed to open log file");
    exit(EXIT_FAILURE);
  }

  signal(SIGINT, free_shm_and_abort);
  signal(SIGCHLD, handle_child_termination);

  clock_seg_id = get_clock_shm();
  clock_shm = attach_to_clock_shm(clock_seg_id);

  // Simulate setup overhead by initializing clock to 1 second
  clock_shm->secs = 1;

  pcb_seg_id = get_pcb_shm();
  pcb_shm = attach_to_pcb_shm(pcb_seg_id);

  sched_block_seg_id = get_sched_block_shm();
  sched_block_shm = attach_to_sched_block_shm(sched_block_seg_id);

  fork_and_exec_child(0);
  printf("[OSS] [%02d:%010d] Creating child %d\n", clock_shm->secs, clock_shm->nano_secs, 0);


  int time_to_create_new_proc = rand() % 3;
  while (1) {
    update_clock_secs(clock_shm);
    int proc_id = get_proc_id(); // -1 if process table is full
    if (num_procs_completed == 5) { // TODO: Replace with TOTAL_PROCS
      break;
    } else if (is_past_last_created(time_to_create_new_proc) && proc_id != -1) {
      printf("[OSS] [%02d:%010d] Creating child %d\n", clock_shm->secs, clock_shm->nano_secs, proc_id);
      fork_and_exec_child(proc_id);
      last_created_at.secs = clock_shm->secs;
      last_created_at.nano_secs = clock_shm->nano_secs;
      time_to_create_new_proc = rand() % 3;
      clock_shm->nano_secs += 2;
    } else {
      // Schedule process
        // Put process ID and time quantum in shared memory
      clock_shm->nano_secs += rand() % 1001;
    }
  }

  free_shm();

  return EXIT_SUCCESS;
}

/**
 * Frees all allocated shared memory
 */
static void free_shm(void) {
  shmdt(clock_shm);
  shmctl(clock_seg_id, IPC_RMID, 0);

  shmdt(pcb_shm);
  shmctl(pcb_seg_id, IPC_RMID, 0);

  shmdt(sched_block_shm);
  shmctl(sched_block_seg_id, IPC_RMID, 0);
}


/**
 * Free shared memory and abort program
 */
static void free_shm_and_abort(int s) {
  free_shm();
  abort();
}


/**
 * Set up the interrupt handler
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

/**
 * Allocates shared memory for a simulated clock.
 * 
 * @return The shared memory segment ID
 */
static int get_clock_shm(void) {
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
static struct my_clock* attach_to_clock_shm(int id) {
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
static int get_pcb_shm(void) {
  int segment_size = sizeof(struct pcb) * MAX_RUNNING_PROCS;
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
static struct pcb** attach_to_pcb_shm(int id) {
  void* pcb_shm = shmat(id, NULL, 0);

  if (*((int*) pcb_shm) == -1) {
    perror("Failed to attach to process control block shared memory");
    exit(EXIT_FAILURE);
  }

  return (struct pcb**) pcb_shm;
}

/**
 * Allocates shared memory for a shedule quantum.
 * See sched_block definition in structs.h
 * 
 * @return The shared memory segment ID
 */
static int get_sched_block_shm(void) {
  int id = shmget(IPC_PRIVATE, sizeof(struct sched_block),
    IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

  if (id == -1) {
    perror("Failed to get shared memory for schedule block");
    exit(EXIT_FAILURE);
  }
  return id;
}

/**
 * Attaches to the shared memory segment for the schedule block.
 * 
 * @return A pointer to the schedule block in shared memory.
 */
static struct sched_block* attach_to_sched_block_shm(int id) {
  void* sched_block_shm = shmat(id, NULL, 0);

  if (*((int*) sched_block_shm) == -1) {
    perror("Failed to attach to schedule block shared memory");
    exit(EXIT_FAILURE);
  }

  return (struct sched_block*) sched_block_shm;
}

static void fork_and_exec_child(int proc_id) {
  pcb_shm_ids[proc_id] = 1;
  pid_t pid = fork();

  if (pid == -1) {
    perror("Failed to fork");
    exit(EXIT_FAILURE);
  }

  if (pid == 0) { // Child
    char proc_id_string[12];
    char clock_seg_id_string[12];
    char pcb_seg_id_string[12];
    char sched_block_seg_id_string[12];
    sprintf(proc_id_string, "%d", proc_id);
    sprintf(clock_seg_id_string, "%d", clock_seg_id);
    sprintf(pcb_seg_id_string, "%d", pcb_seg_id);
    sprintf(sched_block_seg_id_string, "%d", sched_block_seg_id);

    execlp(
      "user",
      "user",
      proc_id_string,
      clock_seg_id_string,
      pcb_seg_id_string,
      sched_block_seg_id_string,
      (char*) NULL
    );
    perror("Failed to exec");
    _exit(EXIT_FAILURE);
  }
}

void handle_child_termination(int signum) {
  int status;
  pid_t pid = wait(&status);

  fprintf(
    stderr,
    "[OSS] [%02d:%010d] Child %d is terminating\n",
    clock_shm->secs,
    clock_shm->nano_secs,
    pid
  );

  num_procs_completed++;
}

static int is_past_last_created(int seconds) {
  int sec_past = clock_shm->secs - last_created_at.secs;
  int ns_past = clock_shm->nano_secs - last_created_at.nano_secs;
  int total_past = ns_past + (sec_past * NANO_SECS_PER_SEC);
  if ((seconds * NANO_SECS_PER_SEC) < total_past) {
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