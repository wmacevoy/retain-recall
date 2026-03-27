cmake_minimum_required(VERSION 3.14)
project(retain-recall VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# retain is a header-only library
add_library(retain INTERFACE)
target_include_directories(retain INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# C sort library (used by the sort examples)
add_library(sort STATIC src/sort.c)
target_include_directories(sort PUBLIC include)

# Simple examples (single source, depend only on retain)
set(SIMPLE_EXAMPLES midi1 midi2 pool graph i18n logging profile checkpoint)
foreach(example IN LISTS SIMPLE_EXAMPLES)
    add_executable(${example} src/${example}.cpp)
    target_link_libraries(${example} PRIVATE retain)
endforeach()

# Sort examples
add_executable(oldsort src/oldsort.c)
target_link_libraries(oldsort PRIVATE sort)

add_executable(newsort src/newsort.cpp)
target_link_libraries(newsort PRIVATE retain sort)

# Multi-threaded test
find_package(Threads REQUIRED)
add_executable(test_retain src/test.cpp)
target_link_libraries(test_retain PRIVATE retain Threads::Threads)

# Tests
enable_testing()
add_test(NAME retain_test COMMAND test_retain)
add_test(NAME oldsort_test COMMAND oldsort)
add_test(NAME newsort_test COMMAND newsort)
add_test(NAME i18n_test
    COMMAND i18n greet english greet hello ohla spanish greet hello ohla)
