#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../common.h"

#define WAIT_USEC (2000)

// Too many things I don't trust
// - how accurate are the timestamps?
// - how accurate is the usleep?

// Things:
// - jitter (do the times jump around)
// - skew
// - how long does it take to make the clock call?

#define CLOCKS(_)                            \
  _(CLOCK_REALTIME)                          \
  _(CLOCK_REALTIME_COARSE)                   \
  _(CLOCK_MONOTONIC)

static void usage(char const* app) {
  fprintf(stderr, "usage: %s clock\n", app);
  fprintf(stderr, "Supported clocks:\n");
#define ELT(clock) fprintf(stderr, "- %s\n", #clock);
  CLOCKS(ELT)
#undef ELT
}

static void
test_clock(int        clock,
           uint64_t * out,
           size_t     n)
{
  for (size_t i = 0; i < n; ++i) {
    struct timespec ts[1];
    clock_gettime(clock, ts);
    out[i] = ts->tv_sec * 1000000000 + ts->tv_nsec;
    usleep(WAIT_USEC);
  }
}

int main(int argc, char** argv) {
  if (argc != 2) {
    usage(argv[0]);
    return 1;
  }

  int clock = -1;
#define ELT(c) if (0 == strcmp(argv[1], #c)) clock = c;
  CLOCKS(ELT);
#undef ELT

  if (clock == -1) {
    usage(argv[0]);
    return 1;
  }

  size_t     n      = 8192;
  uint64_t * values = calloc(sizeof(*values), n);
  test_clock(clock, values, n);

  uint64_t last = values[0];
  for (size_t i = 1; i < n; ++i) {
    uint64_t actual   = values[i];
    uint64_t intended  = last + WAIT_USEC * 1000;
    int64_t  diff      = (int64_t)intended - (int64_t)actual;
    float    perc_diff = (float)((int64_t)intended - (int64_t)actual)/(float)(WAIT_USEC*1000);
    printf("%zu,%zu,%ld,%0.30f\n", actual, intended, diff, perc_diff);
    last = actual;
  }
}

// FIXME determine granularity
// FIXME test all clocks if no arg given?
