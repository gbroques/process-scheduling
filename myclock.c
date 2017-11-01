#include "myclock.h"
#include <stdio.h>
#include <stdlib.h>

unsigned long get_nanosecs(struct my_clock clock) {
  unsigned long max_size = sizeof(unsigned long);
  unsigned long max_secs = max_size / NANOSECS_PER_SEC;
  unsigned long max_nanosecs = max_size % NANOSECS_PER_SEC;
  if (clock.secs > max_secs && clock.nanosecs > max_nanosecs) {
    fprintf(stderr, "Clock to nanoseconds conversion overflow\n");
    exit(EXIT_FAILURE);
  }
  unsigned long nanosecs = clock.secs * NANOSECS_PER_SEC;
  nanosecs += clock.nanosecs;
  return nanosecs;
}

/**
 * Returns true if a is past b.
 */
int is_past_time(struct my_clock a, struct my_clock b) {
  struct my_clock tmp = subtract_clocks(a, b);
  return tmp.secs <= 0 && tmp.nanosecs <= 0 ? 1 : 0;
}

struct my_clock add_nanosecs_to_clock(struct my_clock clock, int nanosecs) {
  struct my_clock new_time;
  new_time.secs = clock.secs;
  new_time.nanosecs = clock.nanosecs + nanosecs;
  if (new_time.nanosecs >= NANOSECS_PER_SEC) {
    int secs = new_time.nanosecs / NANOSECS_PER_SEC;
    new_time.secs += secs;
    new_time.nanosecs -= secs * NANOSECS_PER_SEC;
  }
  return new_time;
}

struct my_clock subract_nanosecs_from_clock(struct my_clock clock, int nanosecs) {
  struct my_clock new_time;
  new_time.secs = clock.secs;
  int subtracted_nanosecs = clock.nanosecs - nanosecs;
  if (subtracted_nanosecs < 0) {
    int quotient = nanosecs / NANOSECS_PER_SEC;
    new_time.secs -= (quotient + 1);
    quotient = quotient == 0 ? 1 : quotient;
    new_time.nanosecs = (quotient * NANOSECS_PER_SEC) - (nanosecs - clock.nanosecs);
  }
  return new_time;
}


/**
 * Adds the time of two clocks together.
 */
struct my_clock add_clocks(struct my_clock a, struct my_clock b) {
  struct my_clock new_clock;
  new_clock.secs = a.secs + b.secs;
  int sum = a.nanosecs + b.nanosecs;
  if (sum > NANOSECS_PER_SEC) {
    int quotient = sum / NANOSECS_PER_SEC;
    new_clock.secs += quotient;
    new_clock.nanosecs = sum % NANOSECS_PER_SEC;
  } else {
    new_clock.nanosecs = sum;
  }
  return new_clock;
}

/**
 * Subtracts the time of one clock from another.
 */
struct my_clock subtract_clocks(struct my_clock a, struct my_clock b) {
  struct my_clock new_clock;
  new_clock = subract_nanosecs_from_clock(a, b.nanosecs);
  new_clock.secs -= b.secs;
  int secs = new_clock.secs - b.secs;
  new_clock.secs = secs < 0 ? 0 : secs;
  return new_clock;
}

struct my_clock divide_clock(struct my_clock a, int divisor) {
  double secs = (double) a.secs / divisor;
  int nanosecs = a.nanosecs / divisor;
  if (secs < 1) {
    secs = 0;
    nanosecs += secs * NANOSECS_PER_SEC;
  }
  struct my_clock new_clock;
  new_clock.secs = secs;
  new_clock.nanosecs = nanosecs;
  return new_clock;
}

struct my_clock get_clock_from_nanosecs(int nanosecs) {
  struct my_clock clock;
  int quotient = nanosecs / NANOSECS_PER_SEC;
  if (quotient == 0) {
    clock.secs = 0;
    clock.nanosecs = nanosecs;
    return clock;
  } else {
    clock.secs = quotient;
    clock.nanosecs = nanosecs - (quotient * NANOSECS_PER_SEC);
    return clock;
  }
}