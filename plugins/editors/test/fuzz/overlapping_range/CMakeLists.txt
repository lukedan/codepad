add_executable(overlapping_range_test)
configure_codepad_target(overlapping_range_test)

target_sources(overlapping_range_test
	PRIVATE "main.cpp")
target_link_libraries(overlapping_range_test
	PRIVATE codepad_core editors)
set_target_properties(overlapping_range_test
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
