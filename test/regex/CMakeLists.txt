add_executable(codepad_regex_test)

target_sources(codepad_regex_test
	PRIVATE
		"src/main.cpp")

target_link_libraries(codepad_regex_test PRIVATE codepad_core)
set_target_properties(codepad_regex_test
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
