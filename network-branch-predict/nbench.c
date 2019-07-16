#include "pkt.h"

#include <assert.h>
#include <byteswap.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

// mumur3 hash finalizer
static inline uint32_t fmix32(uint32_t h) {
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;

  return h;
}

static inline uint64_t rdtscp(void) {
  uint32_t lo, hi;
  __asm__ volatile ("rdtscp"
      : /* outputs */ "=a" (lo), "=d" (hi)
      : /* no inputs */
      : /* clobbers */ "%rcx");
  return (uint64_t)lo | (((uint64_t)hi) << 32);
}

static inline void lfence(void) {
  asm volatile("lfence\n\t" : : : "memory");
}

static inline void clflush(volatile void *__p) {
  asm volatile("clflush %0" : "+m" (*(volatile char *)__p));
}

static inline uint64_t now(void) {
  struct timespec ts[1];
  clock_gettime(CLOCK_REALTIME, ts);
  return ts->tv_sec * 1000000000 + ts->tv_nsec;
}

// generate a few different data sets
// 1) one with all ip packets
// 2) one with a random mix of different kinds of packets
// 3) one which occassionally has non-matching packets
//
// tests:
// 1) packets coming as fast as possible
// 2) packets coming at actually realistic rates
//
// tricky bits:
// - the packet processing time will be too small to measure well
// - want to ensure packet is cold in cache when read for the first time
//  - is this true? if it's coming in off of a nic, where is it getting written?
//
// looking for:
// 1) averge time to process a packet
// 2) variance of time to process a packet (delays)

static char dst[6] = {6, 5, 4, 3, 2, 1};
static char src[6] = {1, 2, 3, 4, 5, 6};

typedef struct __attribute__((packed)) {
  eth_no_vlan_t eth[1];
  ip_hdr_t      ip[1];
  char          data[120];
} full_ip_pkt_no_vlan_t;

typedef struct __attribute__((packed)) {
  eth_single_vlan_t eth[1];
  ip_hdr_t          ip[1];
  char              data[120];
} full_ip_pkt_with_vlan_t;

// char* get_pkts(size_t n)
// {
//   // sktechy, but I don't have enough memory
//   int fd = open("pkts", O_CREAT | O_RDWR | O_TRUNC, S_IRWXU);
//   if (fd == -1) abort();

//   size_t sz = n*1500;
//   int ret = posix_fallocate(fd, 0, sz);
//   if (ret != 0) abort();

//   char* data = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
//   if (data == MAP_FAILED) {
//     perror("mmap");
//     abort();
//   }

//   return data;
// }

char* get_pkts(size_t n)
{
  char* ret = malloc(1500 * n);
  if (!ret) abort();
  return ret;
}

char* generate_all_ip_no_vlan(size_t n_pkt, uint8_t* answer)
{
  char* buffer = get_pkts(n_pkt);

  full_ip_pkt_no_vlan_t pkt[1];
  memcpy(&pkt->eth->addrs.dst, dst, sizeof(dst));
  memcpy(&pkt->eth->addrs.src, src, sizeof(src));
  pkt->eth->ethertype  = bswap_16(ETHERTYPE_IP);
  pkt->ip->ip_ttl      = 123;
  pkt->ip->ip_p        = 16; // CHAOS
  pkt->ip->srcaddr     = bswap_32(123456);
  pkt->ip->dstaddr     = bswap_32(654321);

  for (size_t i = 0; i < 120; ++i) {
    pkt->data[i] = (char)i;
  }

  char* ptr = buffer;
  for (size_t i = 0; i < n_pkt; ++i) {
    memcpy(ptr, pkt, sizeof(pkt));
    answer[i] = 16;
    ptr += 1500;
  }

  return buffer;
}

char* generate_all_ip_all_vlan(size_t n_pkt, uint8_t* answer)
{
  char* buffer = get_pkts(n_pkt);

  full_ip_pkt_with_vlan_t pkt2[1];
  memcpy(&pkt2->eth->addrs.dst, dst, sizeof(dst));
  memcpy(&pkt2->eth->addrs.src, src, sizeof(src));
  pkt2->eth->vlan_tag_ethertype = bswap_16(ETHERTYPE_VLAN_TAG);
  pkt2->eth->vlan               = bswap_16(666);
  pkt2->eth->ethertype          = bswap_16(ETHERTYPE_IP);
  pkt2->ip->ip_ttl              = 123;
  pkt2->ip->ip_p                = 16; // CHAOS
  pkt2->ip->srcaddr             = bswap_32(123456);
  pkt2->ip->dstaddr             = bswap_32(654321);

  for (size_t i = 0; i < 120; ++i) {
    pkt2->data[i] = (char)i;
  }

  char* ptr = buffer;
  for (size_t i = 0; i < n_pkt; ++i) {
    memcpy(ptr, pkt2, sizeof(pkt2));
    answer[i] = 16;
    ptr += 1500;
  }

  return buffer;
}

char* generate_all_ip_some_vlan(size_t n_pkt, uint8_t* answer)
{
  char* buffer = get_pkts(n_pkt);

  full_ip_pkt_no_vlan_t pkt1[1];
  memcpy(&pkt1->eth->addrs.dst, dst, sizeof(dst));
  memcpy(&pkt1->eth->addrs.src, src, sizeof(src));
  pkt1->eth->ethertype  = bswap_16(ETHERTYPE_IP);
  pkt1->ip->ip_ttl      = 123;
  pkt1->ip->ip_p        = 16; // CHAOS
  pkt1->ip->srcaddr     = bswap_32(123456);
  pkt1->ip->dstaddr     = bswap_32(654321);

  full_ip_pkt_with_vlan_t pkt2[1];
  memcpy(&pkt2->eth->addrs.dst, dst, sizeof(dst));
  memcpy(&pkt2->eth->addrs.src, src, sizeof(src));
  pkt2->eth->vlan_tag_ethertype = bswap_16(ETHERTYPE_VLAN_TAG);
  pkt2->eth->vlan               = bswap_16(666);
  pkt2->eth->ethertype          = bswap_16(ETHERTYPE_IP);
  pkt2->ip->ip_ttl              = 123;
  pkt2->ip->ip_p                = 16; // CHAOS
  pkt2->ip->srcaddr             = bswap_32(123456);
  pkt2->ip->dstaddr             = bswap_32(654321);

  for (size_t i = 0; i < 120; ++i) {
    pkt1->data[i] = (char)i;
    pkt2->data[i] = (char)i;
  }

  uint32_t rand = 0xcafebabe;
  char* ptr = buffer;
  for (size_t i = 0; i < n_pkt; ++i) {
    rand = fmix32(rand);
    if (rand > UINT32_MAX/2) memcpy(ptr, pkt1, sizeof(pkt1));
    else                     memcpy(ptr, pkt2, sizeof(pkt2));
    answer[i] = 16;
    ptr += 1500;
  }

  return buffer;
}

char* generate_some_ip_some_vlan(size_t n_pkt, uint8_t* answer)
{
  char* buffer = get_pkts(n_pkt);

  full_ip_pkt_no_vlan_t pkt1[1];
  memcpy(&pkt1->eth->addrs.dst, dst, sizeof(dst));
  memcpy(&pkt1->eth->addrs.src, src, sizeof(src));
  pkt1->eth->ethertype  = bswap_16(ETHERTYPE_IP);
  pkt1->ip->ip_ttl      = 123;
  pkt1->ip->ip_p        = 16; // CHAOS
  pkt1->ip->srcaddr     = bswap_32(123456);
  pkt1->ip->dstaddr     = bswap_32(654321);

  full_ip_pkt_with_vlan_t pkt2[1];
  memcpy(&pkt2->eth->addrs.dst, dst, sizeof(dst));
  memcpy(&pkt2->eth->addrs.src, src, sizeof(src));
  pkt2->eth->vlan_tag_ethertype = bswap_16(ETHERTYPE_VLAN_TAG);
  pkt2->eth->vlan               = bswap_16(666);
  pkt2->eth->ethertype          = bswap_16(ETHERTYPE_IP);
  pkt2->ip->ip_ttl              = 123;
  pkt2->ip->ip_p                = 17; // CHAOS
  pkt2->ip->srcaddr             = bswap_32(123456);
  pkt2->ip->dstaddr             = bswap_32(654321);

  for (size_t i = 0; i < 120; ++i) {
    pkt1->data[i] = (char)i;
    pkt2->data[i] = (char)i;
  }

  eth_no_vlan_t pkt3[1];
  memcpy(&pkt3->addrs.dst, dst, sizeof(dst));
  memcpy(&pkt3->addrs.src, src, sizeof(src));
  pkt3->ethertype = bswap_16(ETHERTYPE_ARP);

  eth_single_vlan_t pkt4[1];
  memcpy(&pkt3->addrs.dst, dst, sizeof(dst));
  memcpy(&pkt3->addrs.src, src, sizeof(src));
  pkt4->vlan_tag_ethertype = bswap_16(ETHERTYPE_VLAN_TAG);
  pkt4->ethertype          = bswap_16(ETHERTYPE_ARP);

  uint32_t rand = 0xcafebabe;
  char* ptr = buffer;
  for (size_t i = 0; i < n_pkt; ++i) {
    rand = fmix32(rand);
    size_t c = UINT32_MAX/4;
    if (rand < c) {
      answer[i] = 16;
      memcpy(ptr, pkt1, sizeof(pkt1));
    }
    else if (rand >= c && rand < 2*c){
      answer[i] = 17;
      memcpy(ptr, pkt2, sizeof(pkt2));
    }
    else if (rand >= 2*c && rand < 3*c) {
      answer[i] = 0;
      memcpy(ptr, pkt3, sizeof(pkt3));
    }
    else {
      answer[i] = 0;
      memcpy(ptr, pkt4, sizeof(pkt4));
    }
    ptr += 1500;
  }

  return buffer;
}

int main() {
  const size_t n    = 500000;
  const size_t reps = 256;

  uint8_t* answer = malloc(n * sizeof(*answer));
  if (!answer) abort();

  uint8_t* results = malloc(n * sizeof(*results));
  if (!results) abort();

  char* pkts = generate_some_ip_some_vlan(n, answer);
  if (!pkts) abort();

  int64_t* times_a = malloc(n * reps * sizeof(*times_a));
  if (!times_a) abort();

  int64_t* times_b = malloc(n * reps * sizeof(*times_b));
  if (!times_b) abort();

  printf("running...\n");

  for (size_t rep = 0; rep < reps; ++rep) {
    char* ptr = pkts;
    clflush(ptr);

    //uint64_t s = now();
    for (size_t i = 0; i < n; ++i) {
      int64_t s = rdtscp();
      uint8_t ans = process_packet_branching(ptr, sizeof(full_ip_pkt_no_vlan_t));
      lfence();
      int64_t e = rdtscp();

      times_a[rep*i + i] = e - s;

      results[i]  = ans;
      ptr        += 1500;

      clflush(ptr); // not in cache next time around
    }
    // uint64_t e = now();
    // times_a[rep] = e-s;

    for (size_t i = 0; i < n; i++) {
      if (results[i] != answer[i]) abort();
    }
  }

  for (size_t rep = 0; rep < reps; ++rep) {
    char* ptr = pkts;
    clflush(ptr);
    // uint64_t s = now();
    for (size_t i = 0; i < n; ++i) {
      int64_t s = rdtscp();
      uint8_t ans = process_packet_branch_free(ptr, sizeof(full_ip_pkt_no_vlan_t));
      lfence();
      int64_t e = rdtscp();

      times_b[rep*i + i] = e - s;

      results[i]  = ans;
      ptr        += 1500;

      clflush(ptr); // not in cache next time around
    }
    // uint64_t e = now();
    // times_b[rep] = e-s;

    for (size_t i = 0; i < n; i++) {
      if (results[i] != answer[i]) abort();
    }
  }

  printf("saving results...\n");

  FILE* out_a = fopen("res_a", "w+");
  if (!out_a) abort();

  FILE* out_b = fopen("res_b", "w+");
  if (!out_b) abort();


  for (size_t rep = 0; rep < reps; ++rep) {
    for (size_t i = 0; i < n; ++i) {
      if (times_a[rep*i + i] < 0) abort();
      if (times_b[rep*i + i] < 0) abort();

      uint64_t a = times_a[rep*i + i];
      size_t ret = fwrite(&a, sizeof(a), 1, out_a);
      if (ret != 1) abort();

      a = times_b[rep*i + i];
      ret = fwrite(&a, sizeof(a), 1, out_b);
      if (ret != 1) abort();
    }

    // uint64_t a = times_a[rep];
    // size_t ret = fwrite(&a, sizeof(a), 1, out_a);
    // if (ret != 1) abort();

    // a = times_b[rep];
    // ret = fwrite(&a, sizeof(a), 1, out_b);
    // if (ret != 1) abort();
  }

  fclose(out_a);
  fclose(out_b);

  // FILE* out_answer = fopen("answer", "w+");
  // if (!out_answer) abort();

  // for (size_t i = 0; i < n; ++i) {
  //   uint64_t a = answer[i];
  //   size_t ret = fwrite(&a, sizeof(a), 1, out_answer);
  //   if (ret != 1) abort();
  // }

  // fclose(out_answer);
}
