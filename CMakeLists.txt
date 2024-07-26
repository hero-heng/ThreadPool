cmake_minimum_required(VERSION 3.27)
project(ThreadPool)

set(CMAKE_CXX_STANDARD 17)
include_directories(.)

add_executable(ThreadPool main.cpp
        threadpool.cpp
        thread.cpp)
