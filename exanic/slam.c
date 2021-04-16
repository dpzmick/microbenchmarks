#include <stdio.h>
#include <sched.h>
#include <string.h>
#include <exanic/exanic.h>
#include <exanic/fifo_tx.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <byteswap.h>

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(*arr))

typedef struct {
  char const* device;
  int         port;
  int         core;
  uint16_t    id;
} dport_t;

static dport_t ports[] = {
  {.device = "exanic0", .port = 0, .core = 0,  .id = 0},
  {.device = "exanic0", .port = 0, .core = 2,  .id = 1},
  {.device = "exanic0", .port = 0, .core = 4,  .id = 3},
  {.device = "exanic0", .port = 0, .core = 6,  .id = 4},
  {.device = "exanic0", .port = 1, .core = 8,  .id = 5},
  {.device = "exanic0", .port = 1, .core = 10, .id = 6},
  {.device = "exanic0", .port = 1, .core = 12, .id = 7},
  {.device = "exanic0", .port = 1, .core = 14, .id = 8},
  // {.device = "exanic0", .port = 1, .core = 2},
  // {.device = "exanic0", .port = 2, .core = 4},
  // {.device = "exanic0", .port = 3, .core = 6},
  // {.device = "exanic1", .port = 0, .core = 16},
  // {.device = "exanic1", .port = 1, .core = 18},
  // {.device = "exanic1", .port = 2, .core = 20},
  // {.device = "exanic1", .port = 3, .core = 22},
};

static int thread_bind_core(pthread_t thread, int core)
{
  cpu_set_t mask[1];
  CPU_ZERO(mask);
  CPU_SET(core, mask);

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

  size_t pkt_size = 128-28;
  size_t n        = 128/4;

  exanic_tx_t* tx = exanic_acquire_tx_buffer(exanic, port->port, n*(pkt_size+28));
  if (!tx) {
    fprintf(stderr, "acquire buffer, %s\n", exanic_get_last_error());
    abort();
  }

  char frame[pkt_size];
  uint8_t dst[6] = {0x64, 0x3f, 0x5f, 0x01, 0x16, 0xf2};
  memcpy(frame,     dst, 6);

  uint8_t src[6] = {0x64, 0x3f, 0x5f, 0x01, 0x16, 0xf0 + port->port};
  memcpy(frame + 6, src, 6);

  uint16_t ethertype = 123;
  memcpy(frame + 12, &ethertype, 2);

  uint16_t len = bswap_16(84);
  memcpy(frame + 14, &len, 2);

  char pld[pkt_size-16]; memset(pld, 'x', sizeof(pld));
  memcpy(frame + 16, pld, sizeof(pld));

  uint16_t* feedback = exanic->tx_feedback_slots;
  printf("feedback at %p for %s/%d/%d\n", feedback, port->device, port->port, port->id);

  for (size_t i = 0; i < n; ++i) {
    struct tx_chunk* chunk = tx->buffer + (pkt_size+28)*i;
    size_t padding = exanic_payload_padding_bytes(EXANIC_TX_TYPE_RAW);

    chunk->feedback_id         = 0;
    chunk->feedback_slot_index = port->id*100 + i;
    chunk->length              = padding + 100;
    chunk->type                = EXANIC_TX_TYPE_RAW;
    chunk->flags               = 0;

    char* chunk_frame = chunk->payload + padding;
    memcpy(chunk_frame, frame, sizeof(frame));
  }

   tx->exanic->registers[REG_EXANIC_INDEX(REG_EXANIC_PCIE_IF_VER)]
     = 0xDEADBEEF;

  uint32_t now = 0;
  while (1) {
    if (now == 0) now = 1;
    else          now = 0;

    for (size_t i = 0; i < n; ++i) {
      struct tx_chunk* chunk = tx->buffer + (pkt_size+28)*i;
      chunk->feedback_id = port->id + now*100;

      int offset = (pkt_size+28)*i;

      // this stuff isn't documented?
      tx->exanic->registers[REG_PORT_INDEX(tx->port_number, REG_PORT_TX_COMMAND)]
        = offset + tx->buffer_offset;
    }

    for (size_t i = 0; i < n; ++i) {
      while (feedback[port->id*100 + i] != port->id + now*100) asm volatile("pause" ::: "memory");
    }
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
    int ret = thread_bind_core(threads[i], ports[i].core);
    if (ret != 0) {
      fprintf(stderr, "numa\n");
      return 1;
    }
  }

  while (1) {
    usleep(100);
  }
}
