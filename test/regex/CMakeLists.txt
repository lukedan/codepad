add_executable(codepad_regex_test)
configure_codepad_target(codepad_regex_test)

target_sources(codepad_regex_test
	PRIVATE
		"src/test_suites/pcre2.cpp"

		"src/main.cpp")

find_package(Catch2 CONFIG REQUIRED)
target_link_libraries(codepad_regex_test PRIVATE codepad_core Catch2::Catch2)
set_target_properties(codepad_regex_test
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
