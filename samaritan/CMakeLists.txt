cmake_minimum_required(VERSION 3.14)
project(Samaritan LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.5.0
)
FetchContent_MakeAvailable(CLI11)

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

enable_testing()

file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_executable(samaritan samaritan.cxx ${SRC_FILES})
target_include_directories(samaritan PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(samaritan PRIVATE CLI11::CLI11)

file(GLOB_RECURSE TEST_SOURCES "*.cc")

foreach(test_src IN LISTS TEST_SOURCES)
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE samaritan_lib GTest::GTest GTest::Main)
    target_compile_options(${test_name} PRIVATE
        $<$<CONFIG:Release>:-O3 -march=native>
        $<$<CONFIG:Debug>:-O0 -g>
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()