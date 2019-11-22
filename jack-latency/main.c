/* Create a bunch of jack clients and send audio through them. Record
   start time and end time. */

#include <assert.h>
#include <errno.h>
#include <jack/jack.h>
#include <jack/thread.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define N_PULSE 8192
// #define N_PULSE 4096
// #define N_PULSE 1

// get from /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq
// or look in dmesg
#define CLOCK_RT_HZ (3892685000)

static atomic_bool wait = ATOMIC_VAR_INIT(true);
static atomic_int  done = ATOMIC_VAR_INIT(0);

typedef enum {
  THREAD_MODE_SOURCE,
  THREAD_MODE_SINK,
  THREAD_MODE_FWD,
} thread_mode_t;

typedef struct {
  jack_client_t * client;
  jack_port_t *   incoming; // if none, send pulses
  jack_port_t *   outgoing; // if none, record pulses
  uint64_t        pulse_times[N_PULSE];
} thread_args_t;

static inline uint64_t now(void)
{
  struct timespec ts[1];
  clock_gettime(CLOCK_REALTIME, ts);
  return ts->tv_sec * 1000000000 + ts->tv_nsec;
}

static inline uint64_t
rdtscp(void)
{
  uint32_t lo, hi;
  __asm__ volatile ("rdtscp"
      : /* outputs */ "=a" (lo), "=d" (hi)
      : /* no inputs */
      : /* clobbers */ "%rcx");
  return (uint64_t)lo | (((uint64_t)hi) << 32);
}

static inline uint64_t
nano_diff(uint64_t rdtsc1, uint64_t rdtsc2)
{
  // 1/(cycle/sec) = sec/cycle
  // sec/cycle * ns/sec = ns/cycle
  static const float nanos_per_cycle = (1./CLOCK_RT_HZ)*1e9;
  uint64_t diff = rdtsc2 - rdtsc1; // cycles
  return (uint64_t)( (float)diff / nanos_per_cycle );
}

static void*
jack_thread(void* thread_args_p)
{
  thread_args_t* args = thread_args_p;
  int ret = jack_acquire_real_time_scheduling(pthread_self(), 95);
  if (ret != 0) {
    fprintf(stderr, "Failed to go realtime\n");
    atomic_fetch_add(&done, 1);
    jack_cycle_signal(args->client, 1);
    return NULL;
  }

  while (atomic_load(&wait)) {
    jack_cycle_wait(args->client);
    jack_cycle_signal(args->client, 0);
  }

  size_t rem = N_PULSE;
  if (!args->incoming) {
    size_t frames_till_next = 32;
    while (1) {
      jack_nframes_t frames  = jack_cycle_wait(args->client);
      float*         buf_out = jack_port_get_buffer(args->outgoing, frames);
      //printf("source cycle at %zu\n", rdtscp());
      for (size_t i = 0; i < frames; ++i) {
        frames_till_next -= 1;
        if (frames_till_next == 0) {
          buf_out[i] = 1.0;
          /* args->pulse_times[N_PULSE-rem] = now(); */
          args->pulse_times[N_PULSE-rem] = rdtscp();
          //printf("sent pulse at %zu\n", args->pulse_times[N_PULSE-rem]);

          rem              -= 1;
          frames_till_next  = 32;
        }
        else {
          buf_out[i] = 0.0;
        }
      }

      if (rem == 0) {
        atomic_fetch_add(&done, 1);
      }
      jack_cycle_signal(args->client, rem == 0 ? 1 : 0);
    }
  }
  else if (!args->outgoing) {
    while (1) {
      jack_nframes_t frames  = jack_cycle_wait(args->client);
      float*         buf_in  = jack_port_get_buffer(args->incoming, frames);
      //printf("sink cycle at %zu\n", rdtscp());

      for (size_t i = 0; i < frames; ++i) {
        if (buf_in[i] == 1) {
          args->pulse_times[N_PULSE-rem] = rdtscp();
          //printf("got pulse at %zu\n", args->pulse_times[N_PULSE-rem]);
          rem -= 1;
        }
      }

      if (rem == 0) {
        atomic_fetch_add(&done, 1);
      }
      jack_cycle_signal(args->client, rem == 0 ? 1 : 0);
    }
  }
  else {
    while (1) {
      jack_nframes_t frames  = jack_cycle_wait(args->client);
      float*         buf_in  = jack_port_get_buffer(args->incoming, frames);
      float*         buf_out = jack_port_get_buffer(args->outgoing, frames);
      memcpy(buf_out, buf_in, frames * sizeof(float));

      for (size_t i = 0; i < frames; ++i) {
        if (buf_in[i] == 1) {
          args->pulse_times[N_PULSE-rem] = rdtscp();
          rem -= 1;
        }
      }

      if (rem == 0) {
        atomic_fetch_add(&done, 1);
      }
      jack_cycle_signal(args->client, rem == 0 ? 1 : 0);
    }
  }

  return NULL;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <n-clients>\n", argv[0]);
    return 1;
  }

  char const* n_client_str = argv[1];

  errno = 0;
  char* err_str = NULL; // why is this not const? that seems broken
  size_t n_clients = strtoul(n_client_str, &err_str, 10);
  if (errno != 0 || n_clients == ULONG_MAX || err_str != n_client_str + strlen(n_client_str)) {
    fprintf(stderr, "<n-clients> of '%s' was not an int\n", n_client_str);
    return 1;
  }

  if (n_clients > 64) {
    fprintf(stderr, "<n-client> must be less than 64\n");
    return 1;
  }

  fprintf(stderr, "Running jack round-trip latency benchmark with %zu clients\n", n_clients);

  thread_args_t * thread_args = calloc(64, sizeof(thread_args_t));
  if (!thread_args) assert(false);

  for (size_t i = 0; i < n_clients; ++i) {
    char          client_name[1024];
    sprintf(client_name, "bench%zu", i);
    fprintf(stderr, "Creating client: %s\n", client_name);

    jack_status_t status;
    thread_args[i].client = jack_client_open(client_name, 0, &status);
    if (!thread_args[i].client) {
      fprintf(stderr, "Failed to create jack client, status=%d\n", status);
      return 1;
    }

    if (i != 0) {
      thread_args[i].incoming = jack_port_register(thread_args[i].client,
                                                   "incoming",
                                                   JACK_DEFAULT_AUDIO_TYPE,
                                                   JackPortIsInput,
                                                   0);

      if (!thread_args[i].incoming) {
        fprintf(stderr, "Failed to register port\n");
        return 1;
      }
    }
    else {
      thread_args[i].incoming = NULL;
    }

    if (i != n_clients-1) {
      thread_args[i].outgoing = jack_port_register(thread_args[i].client,
                                                   "outgoing",
                                                   JACK_DEFAULT_AUDIO_TYPE,
                                                   JackPortIsOutput,
                                                   0);
      if (!thread_args[i].outgoing) {
        fprintf(stderr, "Failed to register port\n");
        return 1;
      }
    }
    else {
      thread_args[i].outgoing = NULL;
    }

    int ret = jack_set_process_thread(thread_args[i].client,
                                      jack_thread,
                                      &thread_args[i]);

    if (ret != 0) {
      fprintf(stderr, "failed to start thread with %d\n", ret);
      return 1;
    }

    ret = jack_activate(thread_args[i].client);
    if (ret != 0) {
      fprintf(stderr, "failed to activate with %d\n", ret);
      return 1;
    }
  }

  // connect everything after clients activated
  for (size_t i = 0; i < n_clients; ++i) {
    if (i != 0) {
      char out[1024];
      char in[1024];
      sprintf(out, "bench%zu:outgoing", i-1);
      sprintf(in, "bench%zu:incoming", i);
      fprintf(stderr, "Connecting %s to %s\n", out, in);

      int ret = jack_connect(thread_args[i].client, out, in);
      if (ret != 0) {
        fprintf(stderr, "Failed to connect ports\n");
      }
    }
  }

  atomic_store(&wait, false); // off to the races

  while (atomic_load(&done) != (int)n_clients) {
    usleep(100000);
  }

  for (size_t c = 0; c < n_clients; ++c) {
    jack_client_close(thread_args[c].client);
  }

  for (size_t p = 0; p < N_PULSE; ++p) {
    /* for (size_t c = 0; c < n_clients; ++c) { */
    /*   printf("%zu,", thread_args[c].pulse_times[p]); */
    /* } */
    /* printf("\n"); */

    // printf("%zu\n", thread_args[n_clients-1].pulse_times[p] - thread_args[0].pulse_times[p]);

    /* for (size_t c = 0; c < n_clients; ++c) { */
    /*   printf("%zu,", thread_args[c].pulse_times[p]-thread_args[0].pulse_times[p]); */
    /* } */
    for (size_t c = 0; c < n_clients; ++c) {
      printf("%zu,",
             nano_diff(thread_args[0].pulse_times[p], thread_args[c].pulse_times[p]));
    }
    printf("\n");
  }
}
