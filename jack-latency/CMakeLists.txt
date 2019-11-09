add_executable(jack-latency main.c)
target_compile_options(jack-latency PRIVATE
    -Wall -Wextra -fstrict-aliasing -std=gnu11 -fverbose-asm
    -fno-stack-protector -g -march=native -mtune=native -D_GNU_SOURCE)
target_link_libraries(jack-latency jack)
