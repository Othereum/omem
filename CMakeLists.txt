cmake_minimum_required(VERSION 3.16)
project(omem)

set(OMEM_BUILD_STATIC FALSE CACHE BOOL "Whether to build as static library")
if(OMEM_BUILD_STATIC)
	add_library(omem STATIC "src/omem.cpp")
	target_compile_definitions(omem PUBLIC OMEM_BUILD_STATIC=1)
else()
	add_library(omem SHARED "src/omem.cpp")
	target_compile_definitions(omem PUBLIC OMEM_BUILD_STATIC=0)
endif()

set_target_properties(omem PROPERTIES CXX_STANDARD 17)
target_include_directories(omem PUBLIC "include")
target_compile_definitions(omem INTERFACE OMEM_BUILD=0 PRIVATE OMEM_BUILD=1)

set(OMEM_POOL_SIZE 1048576 CACHE STRING "Pool size in bytes")
target_compile_definitions(omem PUBLIC OMEM_POOL_SIZE=${OMEM_POOL_SIZE})

set(OMEM_THREADSAFE 0 CACHE STRING "")
target_compile_definitions(omem PUBLIC OMEM_THREADSAFE=${OMEM_THREADSAFE})

set(OMEM_BUILD_TESTS FALSE CACHE BOOL "Whether to build a test")
if(OMEM_BUILD_TESTS)
	file(GLOB_RECURSE TEST_SRC_FILES "tests/*.cpp")
	add_executable(omem_test ${TEST_SRC_FILES})
	set_target_properties(omem_test PROPERTIES CXX_STANDARD 17)

	find_package(GTest 1.10.0 REQUIRED)
	target_link_libraries(omem_test PRIVATE omem GTest::GTest)

	enable_testing()
	add_test(NAME "omem test" COMMAND omem_test)
endif()
