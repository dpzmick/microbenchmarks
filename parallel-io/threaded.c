#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>

static inline uint64_t wallclock()
{
  struct timespec t[1];
  clock_gettime(CLOCK_REALTIME, t);
  return (uint64_t)(t->tv_sec) * (uint64_t)1000000000 + (uint64_t)(t->tv_nsec);
}

typedef struct {
  int mode;
  int fd;
  off_t offset;

  void*  buffer;
  size_t len;
} arg_t;

static void* thread(void* _arg)
{
  arg_t* arg = _arg;

  if (arg->mode == 0) {
    pread(arg->fd, arg->buffer, arg->len, arg->offset);
  } else {
    ssize_t n = pwrite(arg->fd, arg->buffer, arg->len, arg->offset);
    if( n != arg->len ) {
      printf( "write failed" );
    }
  }

  return NULL;
}

#define N_CHUNK 32
#define CHUNK_SZ 512ul*1024ul*1024ul

int main() {
  pthread_t threads[N_CHUNK];
  void* buffers[N_CHUNK];
  arg_t args[N_CHUNK] = {0};

  char* chunk = malloc( CHUNK_SZ );
  for( size_t j = 0; j < CHUNK_SZ; ++j ) {
    // char x[] = {'c', 'a', 't'};
    // chunk[j] = x[j%3];
    chunk[j] = rand()*255;
  }

  for( size_t i = 0; i < N_CHUNK; ++i ) {
    buffers[i] = chunk;
  }

  int fd = open("/nas/test", O_RDWR | O_CREAT, 0666);
  ftruncate( fd, CHUNK_SZ * N_CHUNK );

  // writes first
  for( size_t i = 0; i < N_CHUNK; ++i ) {
    args[i].mode = 1;
    args[i].fd = fd;
    args[i].offset = CHUNK_SZ * i;
    args[i].buffer = buffers[i];
    args[i].len = CHUNK_SZ;
  }

  printf("starting writes\n");

  uint64_t st = wallclock();
  for( size_t i = 0; i < N_CHUNK; ++i ) {
    pthread_create( &threads[i], NULL, thread, &args[i] );
  }

  for( size_t i = 0; i < N_CHUNK; ++i ) {
    pthread_join( threads[i], NULL );
  }
  fsync(fd);
  close(fd);
  uint64_t ed = wallclock();

  double mb = (CHUNK_SZ * N_CHUNK)/1024/1024;
  double sec = (ed-st)/1e9;
  printf( "writes in %f sec, %f MiB/s\n", sec, mb/sec);

  // reads
  for( size_t i = 0; i < N_CHUNK; ++i ) {
    args[i].mode = 0;
  }

  fd = open("/nas/test", O_RDONLY);

  system("sudo bash -c 'echo 3 > /proc/sys/vm/drop_caches'");

  printf("starting reads\n");

  st = wallclock();
  for( size_t i = 0; i < N_CHUNK; ++i ) {
    pthread_create( &threads[i], NULL, thread, &args[i] );
  }

  for( size_t i = 0; i < N_CHUNK; ++i ) {
    pthread_join( threads[i], NULL );
  }
  ed = wallclock();

  sec = (ed-st)/1e9;
  printf( "reads in %f sec, %f MiB/s\n", sec, mb/sec);

  close(fd);
}
