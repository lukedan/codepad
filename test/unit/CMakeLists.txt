add_executable(codepad_test)
configure_codepad_target(codepad_test)

target_sources(codepad_test
	PRIVATE
		"src/main.cpp"

		"src/text.cpp")

if(WIN32)
	target_sources(codepad_test
		PRIVATE
			"src/direct2d_renderer.cpp")
endif()

find_package(Catch2 CONFIG REQUIRED)
target_link_libraries(codepad_test PRIVATE codepad_core Catch2::Catch2)
set_target_properties(codepad_test
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
