#include <stdio.h>
#include <sched.h>
#include <string.h>
#include <exanic/exanic.h>
#include <exanic/fifo_tx.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(*arr))

typedef struct {
  char const* device;
  int         port;
  int         numa_node;
} dport_t;

static dport_t ports[] = {
  {.device = "exanic0", .port = 0, .numa_node = 0},
  {.device = "exanic0", .port = 1, .numa_node = 0},
  {.device = "exanic0", .port = 2, .numa_node = 0},
  {.device = "exanic0", .port = 3, .numa_node = 0},
  {.device = "exanic1", .port = 0, .numa_node = 1},
  {.device = "exanic1", .port = 1, .numa_node = 1},
  {.device = "exanic1", .port = 2, .numa_node = 1},
  {.device = "exanic1", .port = 3, .numa_node = 1},
};

static int thread_bind(pthread_t thread, int numa_node)
{
  cpu_set_t mask[1];
  CPU_ZERO(mask);
  for (size_t i = 0; i < 16; ++i) {
    CPU_SET(16*numa_node + i, mask);
  }

  if (0 == pthread_setaffinity_np(thread, sizeof(mask), mask)) {
    return 0;
  }
  else {
    return -1;
  }
}

void* thread(void* _arg)
{
  dport_t const* port = _arg;
  exanic_t* exanic = exanic_acquire_handle(port->device);
  if (!exanic) abort();

  exanic_tx_t* tx = exanic_acquire_tx_buffer(exanic, port->port, 0);
  if (!tx) {
    fprintf(stderr, "acquire buffer, %s\n", exanic_get_last_error());
    abort();
  }

  char frame[1000];
  memset(frame, 0xff, 1000);
  while (1) {
    if (exanic_transmit_frame(tx, frame, sizeof(frame)) != 0) abort();
  }

  return NULL;
}

int main()
{
  pthread_t threads[ARRAY_SIZE(ports)];
  for (size_t i = 0; i < ARRAY_SIZE(ports); ++i) {
    int ret = pthread_create(&(threads[i]), NULL, thread, &(ports[i]));
    if (ret != 0) {
      fprintf(stderr, "pthread_create\n");
      return 1;
    }
  }

  for (size_t i = 0; i < ARRAY_SIZE(ports); ++i) {
    int ret = thread_bind(threads[i], ports[i].numa_node);
    if (ret != 0) {
      fprintf(stderr, "numa\n");
      return 1;
    }
  }

  while (1) {
    usleep(100);
  }
}
