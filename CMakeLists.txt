cmake_minimum_required(VERSION 3.20)
project(SmilingDB LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SMILINGDB_BUILD_TESTS "Build unit tests" ON)
option(SMILINGDB_BUILD_BENCH "Build benchmarks" ON)
option(SMILINGDB_SANITIZE "Build with AddressSanitizer + UBSan" OFF)

if(SMILINGDB_SANITIZE)
	add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
	add_link_options(-fsanitize=address,undefined)
endif()

add_library(smilingdb src/skiplist.cc src/memtable.cc src/db.cc)
target_include_directories(smilingdb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(smilingdb PRIVATE -Wall -Wextra -Wpedantic)

include(FetchContent)

if(SMILINGDB_BUILD_TESTS)
	FetchContent_Declare(googletest
		URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
		DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
	set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(googletest)

	enable_testing()
	add_executable(skiplist_test tests/skiplist_test.cc)
	target_link_libraries(skiplist_test PRIVATE smilingdb GTest::gtest_main)
	include(GoogleTest)
	gtest_discover_tests(skiplist_test)

	add_executable(memtable_test tests/memtable_test.cc)
	target_link_libraries(memtable_test PRIVATE smilingdb GTest::gtest_main)
	target_compile_options(memtable_test PRIVATE -Wall -Wextra)
	gtest_discover_tests(memtable_test)

	add_executable(db_test tests/db_test.cc)
	target_link_libraries(db_test PRIVATE smilingdb GTest::gtest_main)
	target_compile_options(db_test PRIVATE -Wall -Wextra)
	gtest_discover_tests(db_test)
endif()

if(SMILINGDB_BUILD_BENCH)
	FetchContent_Declare(benchmark
		URL https://github.com/google/benchmark/archive/refs/tags/v1.9.1.tar.gz
		DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
	set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
	set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(benchmark)

	add_executable(skiplist_bench bench/skiplist_bench.cc)
	target_link_libraries(skiplist_bench PRIVATE smilingdb benchmark::benchmark_main)
endif()
