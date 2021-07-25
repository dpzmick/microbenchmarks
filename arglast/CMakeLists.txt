add_executable(arglast arglast.c)
target_link_libraries(arglast m)
target_compile_options(arglast PRIVATE
    -Wall -Wextra -fstrict-aliasing -std=gnu11 -fverbose-asm
    -fno-stack-protector -g -march=native -mtune=native -D_GNU_SOURCE)
