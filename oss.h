#ifndef OSS_H
#define OSS_H

static int setup_interrupt(void);
static void free_shm(void);
static void free_shm_and_abort(int s);
static void print_help_message(char* executable_name,
                               char* log_file);
static int is_required_argument(char optopt);
static void print_required_argument_message(char optopt);
static void fork_and_exec_child(int proc_id);
static int get_clock_shm(void);
static struct my_clock* attach_to_clock_shm(int id);
static int get_pcb_shm(void);
static struct pcb** attach_to_pcb_shm(int id);
static int is_proc_table_full();

#endif
