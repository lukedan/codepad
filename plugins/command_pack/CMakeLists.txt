add_library(command_pack MODULE)

target_sources(command_pack PRIVATE "src/main.cpp")
target_link_libraries(command_pack PRIVATE codepad)
target_include_directories(command_pack PRIVATE "../common/include")
set_target_properties(command_pack
	PROPERTIES
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
