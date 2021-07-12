file(GLOB_RECURSE IMGUI_SRC
	"${CMAKE_SOURCE_DIR}/vendor/imgui/src/*.cpp"
)

add_executable(yabgbe)
target_sources(yabgbe 
	PRIVATE 
		"main.cpp" "bus.cpp" "cpu.cpp" "rom.cpp" "lcd.cpp" 
		${IMGUI_SRC}
	PUBLIC 
		${HEADER_FILES}
)

target_include_directories(yabgbe 
	PUBLIC 
		"${CMAKE_SOURCE_DIR}/include"
		"${CMAKE_SOURCE_DIR}/vendor/imgui/include"
		SDL2::SDL2 glad::glad
)

target_link_libraries(yabgbe 
	SDL2::SDL2 SDL2::SDL2main
	glad::glad
)

target_compile_options(yabgbe PRIVATE -Wimplicit-fallthrough)