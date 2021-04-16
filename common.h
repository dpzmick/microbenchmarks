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
