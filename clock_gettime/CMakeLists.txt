add_executable(clock_gettime main.c)
target_compile_options(clock_gettime PRIVATE
    -Wall -Wextra -fstrict-aliasing -std=gnu11 -fverbose-asm
    -fno-stack-protector -g -march=native -mtune=native -D_GNU_SOURCE)
