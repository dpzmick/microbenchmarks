#include <stdio.h>
#include <stdlib.h>

#include "../common.h"

#define MEM_SIZE (10ul*1024ul*1024ul*1024ul)

int main()
{
  char* mem = calloc( MEM_SIZE, 1 );
  // prefault_mem( mem, MEM_SIZE );

  for( size_t trial = 0; trial < 10; ++trial ) {
    mem[MEM_SIZE-1-trial*100] = 1;

    // find the memory
    uint64_t st = now_realtime();

    size_t where = 0;
    for( ; where < MEM_SIZE; ++where ) {
      if( mem[where] != 0 ) break;
    }

    uint64_t ed = now_realtime();
    extern_read( where ); // prevent loop from getting optimized out

    double sec = ((double)(ed-st))/1e9;
    double mib = ((double)MEM_SIZE)/1024/1024;
    printf( "Scanned memory at %f MiB/s, where=%zu\n", mib/sec, where );

    mem[MEM_SIZE-1-trial*100] = 0;
  }
}
