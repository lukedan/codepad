add_executable(encoding_test)
configure_codepad_target(encoding_test)

target_sources(encoding_test
	PRIVATE "main.cpp")
target_link_libraries(encoding_test
	PRIVATE codepad_core)
set_target_properties(encoding_test
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
