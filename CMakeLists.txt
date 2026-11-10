cmake_minimum_required(VERSION 3.16)
project(seam_carving_greedy CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(greedy_core STATIC src/image.cpp src/energy.cpp src/greedy.cpp)
target_include_directories(greedy_core PUBLIC src third_party)

add_executable(greedy_carve src/main.cpp)
target_link_libraries(greedy_carve PRIVATE greedy_core)

add_executable(test_greedy tests/test_greedy.cpp)
target_link_libraries(test_greedy PRIVATE greedy_core)

enable_testing()
add_test(NAME test_greedy COMMAND test_greedy)
