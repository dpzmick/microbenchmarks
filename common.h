#pragma once

#include <stdint.h>
#include <time.h>

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

/* Make the compiler believe that something it cannot see is reading
   this value either from a register or directly from its location in
   memory. */

#define extern_read(p) asm volatile("" : : "mr"(p) : )

/* Make the compiler believe that something it cannot see is reading
   this value either from a register or directly from its location in
   memory. */

#define extern_write(p) asm volatile("" : "+rm"(p) : : )

static inline uint64_t now_realtime(void) {
  struct timespec ts[1];
  clock_gettime(CLOCK_REALTIME, ts);
  return ts->tv_sec * 1000000000 + ts->tv_nsec;
}

static inline uint64_t now_monotonic(void) {
  struct timespec ts[1];
  clock_gettime(CLOCK_MONOTONIC, ts);
  return ts->tv_sec * 1000000000 + ts->tv_nsec;
}

/* force kernel to allocate pages for the memory it gave us */
static inline void
prefault_mem( char * mem, size_t sz )
{
  for( size_t i = 0; i < sz; i+= 4096 ) {
    char val = mem[i];
    extern_write(val);
    mem[i] = val;
  }
}

/* mumur3 hash finalizer */
static inline uint32_t fmix32(uint32_t h) {
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;

  return h;
}
