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

static int setup_interval_timer(int time);
static int setup_interrupt(void);
static void free_shared_memory(void);
static void free_shared_memory_and_abort(int s);
static void print_help_message(char* executable_name,
                               char* log_file);
static int is_required_argument(char optopt);
static void print_required_argument_message(char optopt);
static int get_clock_shared_segment_size(void);
static void attach_to_shared_memory(void);
static void get_shared_memory(void);
static void fork_and_exec_child(void);

static unsigned int clock_segment_id;
static unsigned int* clock_shared_memory;

const int NANO_SECONDS_PER_SECOND = 1000000000;

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

  if (setup_interval_timer(60) == -1) {
    perror("Faled to set up the ITIMER_PROF interval timer");
    return EXIT_FAILURE;
  }

  FILE* fp;
  fp = fopen(log_file, "w+");

  if (fp == NULL) {
    perror("Failed to open log file");
    exit(EXIT_FAILURE);
  }

  signal(SIGINT, free_shared_memory_and_abort);

  get_shared_memory();

  attach_to_shared_memory();

  fork_and_exec_child();

  int status;
  pid_t child = wait(&status);
  fprintf(stderr, "Child %d exited with status code %d\n", child, status);

  free_shared_memory();

  return EXIT_SUCCESS;
}

static int get_clock_shared_segment_size() {
  return 2 * sizeof(int);
}

/**
 * Frees all allocated shared memory
 */
static void free_shared_memory(void) {
  shmdt(clock_shared_memory);
  shmctl(clock_segment_id, IPC_RMID, 0);
}


/**
 * Free shared memory and abort program
 */
static void free_shared_memory_and_abort(int s) {
  free_shared_memory();
  abort();
}


/**
 * Set up the interrupt handler
 */
static int setup_interrupt(void) {
  struct sigaction act;
  act.sa_handler = free_shared_memory_and_abort;
  act.sa_flags = 0;
  return (sigemptyset(&act.sa_mask) || sigaction(SIGPROF, &act, NULL));
}

/**
 * Sets up an interval timer
 *
 * @param time The duration of the timer
 * @return Zero on success. -1 on error.
 */
static int setup_interval_timer(int time) {
  struct itimerval value;
  value.it_interval.tv_sec = time;
  value.it_interval.tv_usec = 0;
  value.it_value = value.it_interval;
  return (setitimer(ITIMER_PROF, &value, NULL));
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
 * Populates clock_segment_id with a
 * shared memory segment ID.
 */
static void get_shared_memory(void) {
  int shared_segment_size = get_clock_shared_segment_size();
  clock_segment_id = shmget(IPC_PRIVATE, shared_segment_size,
    IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

  if (clock_segment_id == -1) {
    perror("Failed to get shared memory");
    exit(EXIT_FAILURE);
  }
}

/**
 * Attaches to the clock shared memory segment.
 * Populates clock_shared_memory with a reference
 * to its appropriate memory location.
 */
static void attach_to_shared_memory(void) {
  clock_shared_memory = (unsigned int*) shmat(clock_segment_id, 0, 0);

  if (*clock_shared_memory == -1) {
    perror("Failed to attach to shared memory");
    exit(EXIT_FAILURE);
  }
}

static void fork_and_exec_child(void) {
    pid_t pid = fork();

    if (pid == -1) {
      perror("Failed to fork");
      exit(EXIT_FAILURE);
    }

    if (pid == 0) { // Child
      char clock_segment_id_string[12];
      sprintf(clock_segment_id_string, "%d", clock_segment_id);
      execlp(
        "user",
        "user",
        clock_segment_id_string,
        (char*) NULL
      );
      perror("Failed to exec");
      _exit(EXIT_FAILURE);
    }
}