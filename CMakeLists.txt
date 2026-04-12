cmake_minimum_required(VERSION 3.15)
project(byte_frost CXX)

find_package(LLVM REQUIRED)
find_package(GTest REQUIRED)

# Core library shared between the main executable and tests
add_library(
	byte_frost_lib
	STATIC
	src/tokenizer/tokens.cpp
	src/tokenizer/lexer.cpp
	src/parser/parser.cpp
	src/codegen/codegen.cpp
	src/linker/linker.cpp
)

target_include_directories(
	byte_frost_lib
	PUBLIC
	include
)

target_link_libraries(byte_frost_lib PUBLIC llvm-core::llvm-core)

# Main executable
add_executable(
	byte_frost
	src/main.cpp
)

target_link_libraries(byte_frost byte_frost_lib)

# Tests
enable_testing()

add_executable(
	unit_tests
	tests/lexer_test.cpp
	tests/parser_test.cpp
	tests/codegen_test.cpp
)

target_link_libraries(unit_tests byte_frost_lib GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)

install(TARGETS byte_frost DESTINATION "."
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        )
