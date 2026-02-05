cmake_minimum_required(VERSION 3.10)
project(perg LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

set(SOURCES
    src/main.cpp
    src/scanner.cpp
    src/search_engine.cpp
    src/tree_renderer.cpp
    src/mmap_file.cpp
)

add_executable(perg ${SOURCES})

find_package(Threads REQUIRED)
target_link_libraries(perg PRIVATE Threads::Threads)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        target_compile_options(perg PRIVATE /O2 /Oi)
    else()
        target_compile_options(perg PRIVATE -O3 -march=native -flto=auto)
        target_link_options(perg PRIVATE -O3 -march=native -flto=auto)
    endif()
endif()