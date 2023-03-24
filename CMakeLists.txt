cmake_minimum_required(VERSION 3.17)
project(Heavy_Tree_DAWG)

set(CMAKE_CXX_STANDARD 20)

add_executable(Heavy_Tree_DAWG main.cpp includes/dawg.hpp includes/ordered_map.hpp includes/full_text_index.hpp)