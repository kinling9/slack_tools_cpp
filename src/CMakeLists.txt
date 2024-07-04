cmake_minimum_required(VERSION 3.10)
project(cpp_parser)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

find_package(Boost REQUIRED iostreams)
# find_package(re2 REQUIRED PATHS /home/kinling9/packages/re2)

add_executable(parser main.cpp)
target_link_libraries(parser Boost::iostreams)
add_subdirectory(thirdparty/re2)
# target_link_libraries(parser re2::re2) 
