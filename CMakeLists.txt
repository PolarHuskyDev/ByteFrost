cmake_minimum_required(VERSION 3.15)
project(byte_frost CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Version injection — CI sets BF_VERSION via -DBF_VERSION=<tag>; dev builds use 0.0.0-dev.
if(NOT DEFINED BF_VERSION)
    set(BF_VERSION "0.0.0-dev")
endif()
configure_file(
    "${CMAKE_SOURCE_DIR}/include/version.h.in"
    "${CMAKE_BINARY_DIR}/include/version.h"
)

find_package(LLVM REQUIRED)
find_package(GTest REQUIRED)

# On Windows the Conan llvm-core package ships LLVM-C as an import-library DLL
# (LLVM-C.lib + LLVM-C.dll) even though the rest of LLVM is static.
# Copy it next to each executable so it can be found at runtime.
if(WIN32)
    set(_llvm_c_dll "${LLVM_TOOLS_BINARY_DIR}/LLVM-C.dll")
endif()

# Core library shared between the main executable and tests
add_library(
	byte_frost_lib
	STATIC
	src/tokenizer/tokens.cpp
	src/tokenizer/lexer.cpp
	src/parser/parser.cpp
	src/codegen/codegen.cpp
	src/module_resolver/module_resolver.cpp
	# Linker: select the platform-specific implementation at configure time.
	$<IF:$<BOOL:${WIN32}>,src/linker/linker_pe.cpp,src/linker/linker_elf.cpp>
)

target_include_directories(
	byte_frost_lib
	PUBLIC
	include
	"${CMAKE_BINARY_DIR}/include"
)

target_link_libraries(byte_frost_lib PUBLIC llvm-core::llvm-core)

# Main executable
add_executable(
	byte_frost
	src/main.cpp
)

target_link_libraries(byte_frost byte_frost_lib)

# Orca build orchestrator
add_executable(
	orca
	src/orca/main.cpp
)

target_link_libraries(orca byte_frost_lib)

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
gtest_discover_tests(unit_tests DISCOVERY_MODE PRE_TEST)

install(TARGETS byte_frost orca DESTINATION "."
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        )

if(WIN32 AND EXISTS "${_llvm_c_dll}")
    foreach(_target byte_frost orca unit_tests)
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_llvm_c_dll}"
                "$<TARGET_FILE_DIR:${_target}>"
            COMMENT "Copying LLVM-C.dll to output directory"
        )
    endforeach()
    install(FILES "${_llvm_c_dll}" DESTINATION bin)
endif()
