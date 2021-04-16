add_executable(tsc main.c)
target_compile_options(tsc PRIVATE
    -Wall -Wextra -fstrict-aliasing -std=gnu11 -fverbose-asm
    -fno-stack-protector -g -march=native -mtune=native -D_GNU_SOURCE)
