#ifndef MYCLOCK_H
#define MYCLOCK_H

#define NANOSECS_PER_SEC 1000000000 // 1 * 10^9 nanoseconds

/*
 * A simple simulated clock
 * ------------------------*/
struct my_clock {
  unsigned int secs;        // Amount of time in seconds
  unsigned int nanosecs;    // Amount of time in nanoseconds
};

int is_past_time(struct my_clock a, struct my_clock b);

struct my_clock add_nanosecs_to_clock(struct my_clock clock, int nanosecs);

struct my_clock add_clocks(struct my_clock a, struct my_clock b);

struct my_clock subtract_clocks(struct my_clock a, struct my_clock b);

struct my_clock divide_clock(struct my_clock a, int divisor);

struct my_clock get_clock_from_nanosecs(int nanosecs);

struct my_clock subract_nanosecs_from_clock(struct my_clock clock, int nanosecs);

#endif