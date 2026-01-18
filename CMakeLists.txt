cmake_minimum_required(VERSION 3.10)
project(perg LANGUAGES CXX)

# C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header files
include_directories(include)

# Source files
file(GLOB SOURCES "src/*.cpp")

# executable
add_executable(perg ${SOURCES})