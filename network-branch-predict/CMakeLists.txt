add_executable(nbench branching.c branch_free.c nbench.c)
target_compile_options(nbench PRIVATE
    -Wall -Wextra -fstrict-aliasing -std=gnu11 -fverbose-asm
    -fno-stack-protector -g -march=native -mtune=native -D_GNU_SOURCE)
