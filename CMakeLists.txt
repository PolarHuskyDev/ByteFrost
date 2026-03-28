cmake_minimum_required(VERSION 3.15)
project(byte_frost CXX)

find_package(LLVM REQUIRED)

add_executable(
	byte_frost
	src/main.cpp
)

target_include_directories(
	byte_frost
	PUBLIC
	include
)

target_link_libraries(byte_frost llvm-core::llvm-core)

install(TARGETS byte_frost DESTINATION "."
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        )
