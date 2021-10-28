# CMakeList.txt : CMake project for GameboyEmu, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("GameboyEmu")
set(CMAKE_CXX_STANDARD 17)

add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

add_subdirectory("vendor/SDL")

# some IDEs / text editors require compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add source to this project's executable.
add_subdirectory("src")

# TODO: Add tests and install targets if needed.
