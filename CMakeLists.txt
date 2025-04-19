# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.10)

# Set the project name
project(ServerProject)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)

# Find all the .cpp files in the current directory
file(GLOB SOURCES "*.cpp")

# Compile the .cpp files into an executable directly
add_executable(server ${SOURCES})

# Specify additional compile options (debugging flags)
target_compile_options(server PRIVATE -g)
