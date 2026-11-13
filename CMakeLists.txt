cmake_minimum_required(VERSION 3.10)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(mini-rv64vm)

set(CMAKE_C_STANDARD 17)

set(CMAKE_C_STANDARD_REQUIRED True)

file(GLOB SOURCES "src/*.cpp")

add_executable(mini-rv64vm ${SOURCES})

set(CMAKE_C_FLAGS_DEBUG "-O1 -march=native -g")
set(CMAKE_C_FLAGS_RELEASE "-O3 -march=native")

