add_library(command_pack SHARED)

target_sources(command_pack PRIVATE "src/main.cpp")
target_link_libraries(command_pack PRIVATE codepad_core)
target_include_directories(command_pack PRIVATE "../common/include/")
set_target_properties(command_pack
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
