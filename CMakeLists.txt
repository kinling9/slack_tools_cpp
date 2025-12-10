cmake_minimum_required(VERSION 3.10)
project(cpp_parser)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT ENABLE_TSAN)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")
# set(CMAKE_INCLUDE_CURRENT_DIR ON)
# set(Boost_USE_STATIC_LIBS ON)
set(ABSL_PROPAGATE_CXX_STD ON)

option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(ENABLE_TSAN)
    # if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    #     message(FATAL_ERROR "ThreadSanitizer requires Clang (not GCC or MSVC)")
    # endif()

    # Add TSan compile and link flags
    add_compile_options(-fsanitize=thread -g -O1)
    link_libraries(-fsanitize=thread)

    # Disable inlining for better TSan reports (optional)
    add_compile_options(-fno-inline)

    message(STATUS "ThreadSanitizer (TSAN) enabled")
endif()

file(GLOB_RECURSE common_sources
    ${PROJECT_SOURCE_DIR}/src/dm/*.cpp
    ${PROJECT_SOURCE_DIR}/src/parser/*.cpp
    ${PROJECT_SOURCE_DIR}/src/flow/*.cpp
    ${PROJECT_SOURCE_DIR}/src/analyser/*.cpp
    ${PROJECT_SOURCE_DIR}/src/utils/*.cpp
)
if(ENV_EL7)
    find_package(Boost 1.82.0 REQUIRED iostreams)
else()
    find_package(Boost REQUIRED iostreams)
endif()

add_subdirectory(thirdparty/abseil-cpp)
add_subdirectory(thirdparty/re2)
add_subdirectory(thirdparty/argparse)
add_subdirectory(thirdparty/yaml-cpp)
add_subdirectory(thirdparty/fmt)
add_subdirectory(thirdparty/json)
add_subdirectory(thirdparty/csv-parser)
add_subdirectory(thirdparty/yyjson)
file(GLOB_RECURSE sources ${PROJECT_SOURCE_DIR}/src/main.cpp)
include_directories(${PROJECT_SOURCE_DIR}/src)

add_executable(slack_tool ${sources} ${common_sources})
if(ENV_EL7)
    target_link_libraries(slack_tool -static-libgcc -static-libstdc++)
endif()
target_link_libraries(slack_tool Boost::iostreams)
target_link_libraries(slack_tool re2::re2)
target_link_libraries(slack_tool argparse)
target_link_libraries(slack_tool yaml-cpp::yaml-cpp)
target_link_libraries(slack_tool fmt::fmt)
target_link_libraries(slack_tool nlohmann_json::nlohmann_json)
target_link_libraries(slack_tool csv)
target_link_libraries(slack_tool yyjson)
target_link_libraries(slack_tool gcov)

# RPT serial executable
file(GLOB_RECURSE rpt_main ${PROJECT_SOURCE_DIR}/src/rpt_serial.cpp)
add_executable(rpt_serial ${rpt_main} ${common_sources})
if(ENV_EL7)
    target_link_libraries(rpt_serial -static-libgcc -static-libstdc++)
endif()
target_link_libraries(rpt_serial Boost::iostreams)
target_link_libraries(rpt_serial re2::re2)
target_link_libraries(rpt_serial argparse)
target_link_libraries(rpt_serial yaml-cpp::yaml-cpp)
target_link_libraries(rpt_serial fmt::fmt)
target_link_libraries(rpt_serial nlohmann_json::nlohmann_json)
target_link_libraries(rpt_serial csv)
target_link_libraries(rpt_serial yyjson)
target_link_libraries(rpt_serial gcov)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Configuring for Debug build")
    set_target_properties(slack_tool PROPERTIES OUTPUT_NAME "slack_tool-debug")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Configuring for Release build")
    set_target_properties(slack_tool PROPERTIES OUTPUT_NAME "slack_tool")
endif()
