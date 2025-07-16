cmake_minimum_required(VERSION 3.14)
project(Samaritan LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Define common compile options
set(COMMON_COMPILE_OPTIONS
    $<$<CONFIG:Release>:-O3 -march=native>
    $<$<CONFIG:Debug>:-O0 -g>
)

enable_testing()

# Find dependencies
find_package(CLI11 REQUIRED)
find_package(GTest REQUIRED)

# Source files (non-recursive GLOB since src/ has no subdirectories)
file(GLOB MAIN_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER MAIN_SRC_FILES EXCLUDE REGEX "samaritan\\.cxx$")
# Alternative: Explicit source list (uncomment to use)
# set(MAIN_SRC_FILES
#     src/file1.cpp
#     src/file2.cpp
#     # Add more as needed
# )

# Create library
add_library(samaritan_lib STATIC ${MAIN_SRC_FILES})
target_include_directories(samaritan_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_options(samaritan_lib PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(samaritan_lib PUBLIC CLI11::CLI11)

# Create main executable
add_executable(samaritan "${CMAKE_CURRENT_SOURCE_DIR}/samaritan.cxx")
target_compile_options(samaritan PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(samaritan PRIVATE samaritan_lib)

# Test sources (assuming tests/ directory exists)
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cc")
foreach(test_src IN LISTS TEST_SOURCES)
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_include_directories(${test_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_compile_options(${test_name} PRIVATE ${COMMON_COMPILE_OPTIONS})
    target_link_libraries(${test_name} PRIVATE samaritan_lib GTest::gtest GTest::gtest_main)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# Optional: Install rules
install(TARGETS samaritan samaritan_lib
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/" DESTINATION include)