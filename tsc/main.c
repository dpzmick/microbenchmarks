#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common.h"

#define N_TRIAL (8192*2048)
#define TICS_HZ (2111999000) /* dmesg */

typedef struct {
  uint64_t tics;
  uint64_t real;
} measurement_t;

int main(int argc, char** argv) {
  bool pretty = false;
  if (argc > 1) {
    if (argc != 2) {
      fprintf(stderr, "usage: %s <print-mode>\n", argv[0]);
      return 1;
    }

    pretty = (strlen(argv[1]) == strlen("pretty"))
      && (0 == memcmp(argv[1], "pretty", strlen("pretty")));
  }

  measurement_t* measurments = malloc(sizeof(*measurments) * N_TRIAL);
  for (size_t i = 0; i < N_TRIAL; ++i) {
    measurments[i].real = now_realtime();
    measurments[i].tics = rdtscp();
  }

  for (size_t i = 1; i < N_TRIAL; ++i) {
    uint64_t dt    = measurments[i].real - measurments[0].real;
    uint64_t dtics = measurments[i].tics - measurments[0].tics;
    /* uint64_t dt    = measurments[i].real - measurments[i-1].real; */
    /* uint64_t dtics = measurments[i].tics - measurments[i-1].tics; */

    double ns_per_tick = 1e9/TICS_HZ;

    if (pretty) {
      printf("dt: %zu, dtics: %zu, dtics_ns: %f\n",
             dt, dtics, dtics*ns_per_tick);
    }
    else {
      printf("%zu,%f\n", dt, dtics*ns_per_tick);
    }
  }
}
