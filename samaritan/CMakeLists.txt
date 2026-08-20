cmake_minimum_required(VERSION 3.16)
project(Samaritan LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Single-config generators (Makefiles, Ninja) default to an unoptimised build
# with no NDEBUG, which for a chess engine means perft numbers that look like a
# different program. Release unless the developer asked for something else.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SAMARITAN_NATIVE "Tune the build for this machine's CPU" ON)
option(SAMARITAN_TOOLS "Build the bench/ correctness and speed tools" ON)
option(SAMARITAN_INTEGRATION_TESTS
       "Register the tests that need the Python trainer or a reference engine" OFF)

include(CheckCXXCompilerFlag)

# ---------------------------------------------------------------------------
# Compiler options
#
# Kept as a list of *portable intents* rather than literal flags: MSVC spells
# every one of these differently, and hard-coding GCC syntax is what made this
# project Unix-only.
# ---------------------------------------------------------------------------
if(MSVC)
    set(COMMON_COMPILE_OPTIONS
        /utf-8                      # the sources contain UTF-8 in comments
        /EHsc
        /permissive-
        /Zc:preprocessor
        /wd4244 /wd4267             # narrowing conversions, pervasive here
        $<$<CONFIG:Release>:/O2 /Zi>
        $<$<CONFIG:Debug>:/Od /Zi>
    )
    # MSVC has no -march=native; AVX2 is the closest usable equivalent and is
    # the same trade -- faster here, will not start on an older CPU.
    if(SAMARITAN_NATIVE)
        list(APPEND COMMON_COMPILE_OPTIONS /arch:AVX2)
    endif()
    # Release builds still emit a PDB (/Zi above), so the linker needs telling.
    add_link_options($<$<CONFIG:Release>:/DEBUG> $<$<CONFIG:Release>:/OPT:REF>)
else()
    set(COMMON_COMPILE_OPTIONS
        $<$<CONFIG:Release>:-O3 -g>
        $<$<CONFIG:Debug>:-O0 -g>
    )
    if(SAMARITAN_NATIVE)
        check_cxx_compiler_flag("-march=native" HAVE_MARCH_NATIVE)
        if(HAVE_MARCH_NATIVE)
            list(APPEND COMMON_COMPILE_OPTIONS $<$<CONFIG:Release>:-march=native>)
        else()
            # Apple Silicon clang wants -mcpu=native instead.
            check_cxx_compiler_flag("-mcpu=native" HAVE_MCPU_NATIVE)
            if(HAVE_MCPU_NATIVE)
                list(APPEND COMMON_COMPILE_OPTIONS $<$<CONFIG:Release>:-mcpu=native>)
            endif()
        endif()
    endif()
endif()

if(WIN32)
    # <windows.h> otherwise defines min/max macros that break std::min/std::max,
    # and drags in half the platform SDK.
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

enable_testing()

# ---------------------------------------------------------------------------
# Dependencies
#
# Only CLI11 is required, and only by the UCI front end. GoogleTest and Google
# Benchmark are looked up quietly: a checkout that has neither still builds a
# working engine, it just builds fewer things.
# ---------------------------------------------------------------------------
find_package(CLI11 REQUIRED)
find_package(GTest QUIET)
find_package(benchmark QUIET)

# ---------------------------------------------------------------------------
# Core library: everything except the command-line front end.
#
# Split out so the bench tools and the tests do not link CLI11 or Google
# Benchmark, which they never use.
# ---------------------------------------------------------------------------
add_library(samaritan_core STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/movegen.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utility.cpp"
)
target_include_directories(samaritan_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_options(samaritan_core PRIVATE ${COMMON_COMPILE_OPTIONS})

# Front-end library: the UCI engine on top of the core.
add_library(samaritan_lib STATIC "${CMAKE_CURRENT_SOURCE_DIR}/src/engine.cpp")
target_compile_options(samaritan_lib PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(samaritan_lib PUBLIC samaritan_core CLI11::CLI11)

# ---------------------------------------------------------------------------
# Executables
# ---------------------------------------------------------------------------
add_executable(samaritan "${CMAKE_CURRENT_SOURCE_DIR}/samaritan.cxx")
target_compile_options(samaritan PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(samaritan PRIVATE samaritan_lib)
if(benchmark_FOUND)
    target_compile_definitions(samaritan PRIVATE SAMARITAN_HAS_BENCHMARK)
    target_link_libraries(samaritan PRIVATE benchmark::benchmark)
endif()

# Self-play data generator
add_executable(datagen "${CMAKE_CURRENT_SOURCE_DIR}/datagen.cxx")
target_compile_options(datagen PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(datagen PRIVATE samaritan_core CLI11::CLI11)

# ---------------------------------------------------------------------------
# bench/ tools
#
# These used to be built by a hand-written shell script, which is why none of
# them existed on Windows and why their binaries kept being committed. They are
# ordinary targets now, and the ones that are oracles rather than benchmarks are
# registered with CTest below.
# ---------------------------------------------------------------------------
if(SAMARITAN_TOOLS)
    set(SAMARITAN_TOOL_NAMES
        perft                   # standalone perft driver
        verify                  # differential perft against reference counts
        bench                   # movegen throughput
        coverage                # how often each rule is actually exercised
        invariant_test          # whole-state invariants after make/undo
        incremental_test        # incremental NNUE vs full refresh
        checkinfo_test          # check/pin metadata vs brute force
        dump_features           # feature indices, for trainer parity
        dump_bags               # per-perspective feature bags
        dump_network            # write an SNN1 net + its evaluations
        make_special_positions  # corpus generator
    )
    foreach(tool ${SAMARITAN_TOOL_NAMES})
        add_executable(${tool} EXCLUDE_FROM_ALL "${CMAKE_CURRENT_SOURCE_DIR}/bench/${tool}.cc")
        target_compile_options(${tool} PRIVATE ${COMMON_COMPILE_OPTIONS})
        # make_special_positions builds its corpus with the test helpers.
        target_include_directories(${tool} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        target_link_libraries(${tool} PRIVATE samaritan_core)
        set_target_properties(${tool} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                              "${CMAKE_CURRENT_BINARY_DIR}/bench")
    endforeach()

    # `cmake --build . --target tools` builds the lot.
    add_custom_target(tools DEPENDS ${SAMARITAN_TOOL_NAMES})
endif()

# ---------------------------------------------------------------------------
# Tests
#
# Two kinds run by default:
#   * the GoogleTest suites, which are fast and hermetic;
#   * the bench/ oracles, at bounded depths, which are the checks that have
#     actually caught bugs here (whole-state invariants, incremental NNUE
#     agreement, differential perft).
#
# Depths are chosen so the whole run stays inside a few seconds. Deeper runs are
# a deliberate act: build the tool and give it a bigger number.
# ---------------------------------------------------------------------------
set(CORPUS "${CMAKE_CURRENT_SOURCE_DIR}/bench/positions.tsv")

if(GTest_FOUND)
    set(TEST_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/movegen_test.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/search_test.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/nnue_test.cc"
    )

    foreach(test_src ${TEST_SOURCES})
        get_filename_component(test_name ${test_src} NAME_WE)
        add_executable(${test_name} ${test_src})
        target_include_directories(${test_name} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests"
        )
        target_compile_options(${test_name} PRIVATE ${COMMON_COMPILE_OPTIONS})
        target_link_libraries(${test_name} PRIVATE samaritan_core GTest::gtest_main)
        add_test(NAME ${test_name} COMMAND ${test_name})
        set_tests_properties(${test_name} PROPERTIES LABELS unit)
    endforeach()
else()
    message(STATUS "GoogleTest not found -- unit tests will not be built")
endif()

if(SAMARITAN_TOOLS)
    # A tool has to exist before CTest can run it, and CTest does not build.
    # This fixture builds them once, as the first "test" in the run.
    add_test(NAME build_tools
             COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --config $<CONFIG> --target tools)
    set_tests_properties(build_tools PROPERTIES FIXTURES_SETUP tools LABELS build)

    function(samaritan_tool_test name)
        add_test(NAME ${name} COMMAND ${ARGN})
        set_tests_properties(${name} PROPERTIES
            FIXTURES_REQUIRED tools
            LABELS oracle
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endfunction()

    # Whole-state invariants: piece list, king trackers, non-pawn counts and the
    # incremental Zobrist key, recomputed after every make and every undo.
    samaritan_tool_test(invariants $<TARGET_FILE:invariant_test> "${CORPUS}" 3)

    # Check and pin metadata against the brute-force attack scan.
    samaritan_tool_test(checkinfo $<TARGET_FILE:checkinfo_test> "${CORPUS}" 3)

    # Differential perft against the recorded reference counts, in both bulk and
    # no-bulk modes.
    samaritan_tool_test(differential_perft
        $<TARGET_FILE:verify> "${CORPUS}" "${CMAKE_CURRENT_SOURCE_DIR}/bench/ref_depth4.tsv" 4)

    # Incremental accumulators against a full refresh, with a real (random) net.
    samaritan_tool_test(incremental_nnue $<TARGET_FILE:incremental_test> "${CORPUS}" 2)
endif()

# Gates that need something this repository does not contain -- the Python
# trainer for feature/forward parity, and a reference engine for regenerating
# perft counts. Off by default so a plain `ctest` never fails for want of a
# virtualenv; turn on where those exist.
if(SAMARITAN_INTEGRATION_TESTS AND UNIX)
    add_test(NAME feature_parity
             COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/bench/check_feature_parity.sh")
    add_test(NAME network_parity
             COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/bench/check_network_parity.sh")
    set_tests_properties(feature_parity network_parity PROPERTIES
        LABELS integration
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------
install(TARGETS samaritan samaritan_lib samaritan_core datagen
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/" DESTINATION include)
