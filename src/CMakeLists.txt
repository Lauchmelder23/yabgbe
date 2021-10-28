add_executable(yabgbe "main.cpp" "bus.cpp" "cpu.cpp" "rom.cpp" "lcd.cpp")

file(GLOB_RECURSE OTHER_SOURCES
	"${CMAKE_SOURCE_DIR}/vendor/imgui/*.cpp"
	"${CMAKE_SOURCE_DIR}/vendor/glad/*.c"
)
target_sources(yabgbe PUBLIC ${OTHER_SOURCES})

target_include_directories(yabgbe 
	PUBLIC 
		"${CMAKE_SOURCE_DIR}/vendor/imgui/include"
		"${CMAKE_SOURCE_DIR}/vendor/glad/include"
		SDL2
)

target_link_libraries(yabgbe 
	SDL2
	${CMAKE_DL_LIBS}
)

if(WIN32)
	add_custom_command(TARGET yabgbe POST_BUILD 
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:SDL2> $<TARGET_FILE_DIR:yabgbe>
	)
endif()
