cmake_minimum_required(VERSION 3.22.1 FATAL_ERROR)

project(arena_allocator LANGUAGES CXX)

add_subdirectory(lua-5.4.4)
add_subdirectory(src)