cmake_minimum_required(VERSION 3.15)
project(byte_frost CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#########################################################################################
# Version injection — CI sets BF_VERSION via -DBF_VERSION=<tag>; dev builds use 0.0.0-dev.
#########################################################################################
if(NOT DEFINED BF_VERSION)
set(BF_VERSION "0.0.0-dev")
endif()
configure_file(
	"${CMAKE_SOURCE_DIR}/include/version.h.in"
	"${CMAKE_BINARY_DIR}/include/version.h"
)

#########################################################################################


#########################################################################################
# Dependencies
#########################################################################################
find_package(LLVM REQUIRED)
find_package(GTest REQUIRED)

# On MSVC, Conan's llvm-core package links both the individual static LLVM .lib
# files AND LLVM-C.lib (import stub for LLVM-C.dll, the C API wrapper).  The
# DLL bundles its own copy of LLVM, which creates a second instance of the LLVM
# target-registry singleton.  InitializeAllTargets() registers targets into one
# instance while TargetRegistry::lookupTarget() searches the other, so lookup
# always fails.  Since this project only uses LLVM's C++ API, suppress the
# LLVM-C import-lib so the DLL is never pulled in.
if(MSVC)
	set_property(TARGET LLVM-C PROPERTY INTERFACE_LINK_LIBRARIES "")
endif()

#########################################################################################
# Logger static library
#########################################################################################
add_library(
	logger_lib
	STATIC
	src/logger/logger.cpp
)

target_include_directories(
	logger_lib
	PUBLIC
	include
)

#########################################################################################
# Core library shared between the main executable and tests
#########################################################################################
add_library(
	bytefrost_lib
	STATIC
	src/bytefrost/tokenizer/tokens.cpp
	src/bytefrost/tokenizer/tokenizer.cpp
)

target_include_directories(
	bytefrost_lib
	PUBLIC
	include
	"${CMAKE_BINARY_DIR}/include"
)

target_link_libraries(bytefrost_lib PUBLIC llvm-core::llvm-core logger_lib)

#########################################################################################
# Compiler main executable - bytefrost
#########################################################################################
add_executable(
	bytefrost
	src/bytefrost/main.cpp
)

target_link_libraries(bytefrost bytefrost_lib)

#########################################################################################
# Orca build orchestrator - orca
#########################################################################################
add_executable(
	orca
	src/orca/main.cpp
)

target_link_libraries(orca bytefrost_lib)

#########################################################################################
# Tests
#########################################################################################
# Compiler - bytefrost
enable_testing()

add_executable(
	bytefrost_compiler_tests
	tests/bytefrost/tokenizer.spec.cpp
)

target_link_libraries(bytefrost_compiler_tests bytefrost_lib GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(bytefrost_compiler_tests DISCOVERY_MODE PRE_TEST)


# Orca
# enable_testing()

# add_executable(
# 	orca_tests
# 	tests/orca/tokenizer.spec.cpp
# )

# target_link_libraries(orca_tests bytefrost_lib GTest::gtest GTest::gtest_main)

# include(GoogleTest)
# gtest_discover_tests(orca_tests DISCOVERY_MODE PRE_TEST)


#########################################################################################
# Installation
# Allows user to run `cmake --install .` to copy the bytefrost and orca executables to the bin/ directory
# and the bytefrost_lib static library to the lib/ directory. Adjust as needed for your specific installation requirements.
#########################################################################################
install(TARGETS bytefrost orca DESTINATION "."
	RUNTIME DESTINATION bin
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
)
