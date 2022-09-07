cmake_minimum_required(VERSION 3.22)
project(mythreadpool)

set(CMAKE_CXX_STANDARD 14)

add_executable(mythreadpool main.cpp threadpool.cpp threadpool.h)
