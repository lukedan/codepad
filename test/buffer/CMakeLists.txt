add_executable(buffer_test)

target_sources(buffer_test
	PRIVATE "main.cpp")
target_link_libraries(buffer_test
	PRIVATE codepad_core editors)
set_target_properties(buffer_test
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
