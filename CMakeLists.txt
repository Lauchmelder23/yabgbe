# CMakeList.txt : CMake project for GameboyEmu, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("GameboyEmu")
set(CMAKE_CXX_STANDARD 17)

add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

find_package(SDL2 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

file(GLOB_RECURSE HEADER_FILES 
	"include/*.hpp"
)

# Add source to this project's executable.
add_subdirectory(src)

# TODO: Add tests and install targets if needed.
