cmake_minimum_required(VERSION 3.14)
project(Samaritan LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Define common compile options
set(COMMON_COMPILE_OPTIONS
    $<$<CONFIG:Release>:-O3 -march=native -g>
    $<$<CONFIG:Debug>:-O0 -g>
)

enable_testing()

# Find dependencies
find_package(CLI11 REQUIRED)
find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)

# Source files
file(GLOB MAIN_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER MAIN_SRC_FILES EXCLUDE REGEX "samaritan\\.cxx$")

# Create library
add_library(samaritan_lib STATIC ${MAIN_SRC_FILES})
target_include_directories(samaritan_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_options(samaritan_lib PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(samaritan_lib PUBLIC CLI11::CLI11 benchmark::benchmark)

# Create main executable
add_executable(samaritan "${CMAKE_CURRENT_SOURCE_DIR}/samaritan.cxx")
target_compile_options(samaritan PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(samaritan PRIVATE samaritan_lib)

# Self-play data generator
add_executable(datagen "${CMAKE_CURRENT_SOURCE_DIR}/datagen.cxx")
target_compile_options(datagen PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(datagen PRIVATE samaritan_lib CLI11::CLI11)

# Benchmarks
add_executable(nnue_bench "${CMAKE_CURRENT_SOURCE_DIR}/tests/nnue.cc")
target_include_directories(nnue_bench PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_options(nnue_bench PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(nnue_bench PRIVATE
    samaritan_lib
    benchmark::benchmark
)

# GTest correctness tests
set(TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/movegen_test.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/nnue_correctness_test.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/search_test.cc"
)

foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_include_directories(${test_name} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
    target_compile_options(${test_name} PRIVATE ${COMMON_COMPILE_OPTIONS})
    target_link_libraries(${test_name} PRIVATE samaritan_lib GTest::gtest_main)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()


# Install rules
install(TARGETS samaritan samaritan_lib datagen
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/" DESTINATION include)