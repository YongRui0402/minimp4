cmake_minimum_required(VERSION 3.14)
project(minimp4 C)

# Header-only library
add_library(minimp4 INTERFACE)
target_include_directories(minimp4 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Options
option(MINIMP4_BUILD_TESTS "Build tests" ON)
option(MINIMP4_ENABLE_SANITIZERS "Enable ASan/UBSan" OFF)
option(MINIMP4_ENABLE_COVERAGE "Enable gcov coverage" OFF)
option(MINIMP4_ENABLE_FUZZING "Build fuzz targets (requires clang)" OFF)

# Sanitizers
if(MINIMP4_ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Coverage
if(MINIMP4_ENABLE_COVERAGE)
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

# Legacy integration test (minimp4_test.c)
add_executable(minimp4_test minimp4_test.c)
target_link_libraries(minimp4_test PRIVATE minimp4)
target_compile_definitions(minimp4_test PRIVATE _FILE_OFFSET_BITS=64)
if(UNIX)
    target_link_libraries(minimp4_test PRIVATE m pthread)
endif()

if(MINIMP4_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
