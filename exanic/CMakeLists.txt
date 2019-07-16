add_executable(slam slam.c)
target_compile_options(slam PRIVATE
    -pthread
    -Wall -Wextra -fstrict-aliasing -std=gnu11 -fverbose-asm
    -fno-stack-protector -g -march=native -mtune=native -D_GNU_SOURCE)

target_link_options(slam PRIVATE -pthread)
target_link_libraries(slam exanic)
