add_executable(red_black_tree_test)

target_sources(red_black_tree_test
	PRIVATE "main.cpp")
target_link_libraries(red_black_tree_test
	PRIVATE codepad_core)
set_target_properties(red_black_tree_test
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
