#include "../common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define N      (1024*1024)
#define N_ITER (200)

typedef bool (*pred_t)( int32_t e );

// interesting because compiler doesn't inline anything in C

static inline size_t
arglast( pred_t         pred,
         int32_t const* elems,
         size_t         n_elem )
{
  size_t last = -1;
  for( size_t i = 0; i < n_elem; ++i ) {
    if( pred( elems[i] ) ) last = i;
  }
  return last;
}

static inline size_t
arglast_streaming( pred_t         pred,
                   int32_t const* elems,
                   size_t         n_elem )
{
  size_t last = -1;
  for( size_t i = 0; i < n_elem; ++i ) {
    size_t nxt[] = { last, i };
    bool cmp = pred( elems[i] );
    last = nxt[(size_t)cmp];
  }
  return last;
}

static inline void
test( char const * name,
      pred_t       pred )
{
  // needs to be sufficiently random to avoid introducing predictor bias
  int * b = malloc( N*sizeof(int) );
  for( size_t i = 0; i < N; ++i ) b[i] = fmix32( i );

  uint64_t st = now_realtime();
  for( size_t i = 0; i < 200; ++i ) {
    size_t res = arglast( pred, b, N );
    extern_read(res);
  }
  uint64_t ed = now_realtime();

  uint64_t st_streaming = now_realtime();
  for( size_t i = 0; i < 200; ++i ) {
    size_t res = arglast_streaming( pred, b, N );
    extern_read(res);
  }
  uint64_t ed_streaming = now_realtime();

  printf( "[%s] normal: %f per element\n", name, ((double)ed-st)/((double)(N_ITER*N)) );
  printf( "[%s] streaming: %f per element\n", name, ((double)ed_streaming-st_streaming)/((double)(N_ITER*N)) );

  free( b );
}

static inline bool
gt0( int32_t e )
{
  return e > 0;
}

static inline bool
sgt0( int32_t e )
{
  return sin(e) > 0;
}

int main()
{
  test( "gt0", gt0 );
  test( "sgt0", sgt0 );
}
