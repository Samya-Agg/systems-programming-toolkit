cmake_minimum_required(VERSION 3.16)
project(linux_systems_toolkit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(
    systems_toolkit
    src/main.cpp
    src/thread_pool.cpp
)
target_compile_options(systems_toolkit PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(systems_toolkit PRIVATE Threads::Threads)
