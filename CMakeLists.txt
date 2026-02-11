cmake_minimum_required(VERSION 3.10)
project(MyPiProject)

# 1. Tell CMake to include the NNG directory
add_subdirectory(external/nng)

# 2. Create your executable
add_executable(my_app main.cpp)

# 3. "Link" NNG to your app
# This automatically handles both the .h files and the compiled library
target_link_libraries(my_app nng)