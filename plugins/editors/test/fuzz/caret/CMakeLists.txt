add_executable(caret_test)

target_sources(caret_test
	PRIVATE "main.cpp")
target_link_libraries(caret_test
	PRIVATE codepad_core editors)
set_target_properties(caret_test
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
