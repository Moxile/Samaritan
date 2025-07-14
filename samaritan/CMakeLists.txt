cmake_minimum_required(VERSION 3.14)
project(Samaritan LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)
add_executable(samaritan samaritan.cxx ${SRC_FILES})
target_include_directories(samaritan PRIVATE ${PROJECT_SOURCE_DIR}/include)