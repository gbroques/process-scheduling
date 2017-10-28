#ifndef OSS_H
#define OSS_H

/******************************************************************************
 * STRUCTURES                                                                 *
 ******************************************************************************/

/*
 * Multi-level Feedback Queue
 * --------------------------*/
struct my_mlfq {

};

// ============================================================================


/******************************************************************************
 * CONSTANTS                                                                  *
 ******************************************************************************/

#define MY_TIMESLICE 100000000; // 100 milliseconds in nano seconds
#define ALPHA = 1;
#define BETA = 2;

// ============================================================================


/******************************************************************************
 * PROTOTYPES                                                                 *
 ******************************************************************************/
static int setup_interrupt(void);
static void free_shm(void);
static void free_shm_and_abort(int s);
static void print_help_message(char* executable_name,
                               char* log_file);
static int is_required_argument(char optopt);
static void print_required_argument_message(char optopt);
static void fork_and_exec_child(int proc_id);
static int is_past_last_created(struct my_clock create_at);
static int get_proc_id();
static void update_clock_secs(struct my_clock* clock);
static int schedule_process();
static void enqueue_process(int proc_id, int priority);
static int dequeue_process(int priority);
static int is_queue_empty();

#endif
