cmake_minimum_required(VERSION 2.6)
project(microbenchmarks)

add_subdirectory(network-branch-predict)
add_subdirectory(exanic)
add_subdirectory(jack-latency)
add_subdirectory(tsc)
add_subdirectory(clock_gettime)
