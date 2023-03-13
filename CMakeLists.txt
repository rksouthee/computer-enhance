cmake_minimum_required(VERSION 3.26)

set(CMAKE_CXX_STANDARD 20)
project(computer-enhance LANGUAGES CXX)

add_executable(sim8086 sim8086.cpp)
if (MSVC)
	target_compile_options(sim8086 PRIVATE /W4)
endif()
