cmake_minimum_required(VERSION 3.17)
project(Packed_DAWG)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# ./Packed_DAWG/sdsl/include
include_directories(sdsl/include)

add_executable(Packed_DAWG main.cpp includes/dawg.hpp includes/map.hpp includes/full_text_index.hpp includes/level_ancestor_ladder.hpp includes/level_ancestor.hpp)
# ./Packed_DAWG/sdsl/lib
target_link_libraries(Packed_DAWG sdsl)
