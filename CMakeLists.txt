cmake_minimum_required(VERSION 3.17)
project(Heavy_Tree_DAWG)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include_directories(sdsl/include)

add_executable(Heavy_Tree_DAWG main.cpp includes/dawg.hpp includes/map.hpp includes/full_text_index.hpp includes/level_ancestor_ladder.hpp includes/level_ancestor.hpp)
target_link_libraries(Heavy_Tree_DAWG sdsl)
