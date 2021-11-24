add_executable(arglast arglast.c)
target_link_libraries(arglast m)
target_compile_options(arglast PRIVATE
    -Wall -Wextra -fstrict-aliasing -std=gnu11 -fverbose-asm -fvisibility=hidden
    -fno-stack-protector -g -march=native -mtune=native -D_GNU_SOURCE)

add_executable(arglast_cpp arglast.cpp)
target_link_libraries(arglast_cpp m)
target_compile_options(arglast_cpp PRIVATE
    -Wall -Wextra -fstrict-aliasing -std=c++20 -fverbose-asm -fvisibility=hidden
    -fno-stack-protector -g -march=native -mtune=native -D_GNU_SOURCE)
