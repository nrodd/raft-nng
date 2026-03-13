cmake_minimum_required(VERSION 3.10)
project(MyPiProject)

add_subdirectory(external/nng)

find_package(Threads REQUIRED)

add_executable(my_app server/server.cpp)

target_link_libraries(my_app nng Threads::Threads)