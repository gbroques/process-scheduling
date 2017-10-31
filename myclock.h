#ifndef MYCLOCK_H
#define MYCLOCK_H

#define NANO_SECS_PER_SEC 1000000000 // 1 * 10^9 nano seconds

/*
 * A simple simulated clock
 * ------------------------*/
struct my_clock {
  unsigned int secs;        // Amount of time in seconds
  unsigned int nano_secs;   // Amount of time in nano seconds
};

int get_nano_secs(struct my_clock clock);

int get_nano_secs_past(struct my_clock a, struct my_clock b);

int is_past_time(struct my_clock a, struct my_clock b);

struct my_clock add_nano_secs_to_clock(struct my_clock clock, int nano_secs);

#endif