# find_package(re2 REQUIRED PATHS /home/kinling9/packages/re2)
file(GLOB_RECURSE sources *.cpp)
include_directories(${PROJECT_SOURCE_DIR}/src)

add_executable(parser ${sources})
target_link_libraries(parser -static-libgcc -static-libstdc++)
target_link_libraries(parser Boost::iostreams)
target_link_libraries(parser re2::re2) 
