#include "myclock.h"

int get_nano_secs(struct my_clock clock) {
  int nano_secs = clock.secs * NANO_SECS_PER_SEC;
  nano_secs += clock.nano_secs;
  return nano_secs;
}

/**
 * Returns how many nano seconds a is past b.
 */
int get_nano_secs_past(struct my_clock a, struct my_clock b) {
  int ns_past = get_nano_secs(a) - get_nano_secs(b);
  return ns_past < 0 ? 0 : ns_past;
}


/**
 * Returns true if a is past b.
 */
int is_past_time(struct my_clock a, struct my_clock b) {
  int ns_past = get_nano_secs_past(a, b);
  return ns_past >= 0 ? 1 : 0;
}

struct my_clock add_nano_secs_to_clock(struct my_clock clock, int nano_secs) {
  struct my_clock new_time;
  new_time.secs = clock.secs;
  new_time.nano_secs = clock.nano_secs + nano_secs;
  if (new_time.nano_secs >= NANO_SECS_PER_SEC) {
    int secs = new_time.nano_secs / NANO_SECS_PER_SEC;
    new_time.secs += secs;
    new_time.nano_secs -= secs * NANO_SECS_PER_SEC;
  }
  return new_time;
}

/**
 * Adds the time of two clocks together.
 */
struct my_clock add_clocks(struct my_clock a, struct my_clock b) {
  struct my_clock new_clock;
  new_clock.secs = a.secs + b.secs;
  struct my_clock temp_clock = add_nano_secs_to_clock(a, b.nano_secs);
  new_clock.secs += temp_clock.secs;
  new_clock.nano_secs = temp_clock.nano_secs;
  return new_clock;
}