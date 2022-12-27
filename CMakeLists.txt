cmake_minimum_required(VERSION 3.24)
project(Networking_ex3 C)

set(CMAKE_C_STANDARD 11)

add_executable(Networking_ex3 threadpool.h threadpool.c server.c)
