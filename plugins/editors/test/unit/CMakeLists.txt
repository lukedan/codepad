add_executable(editors_test)

target_sources(editors_test
	PRIVATE
		"src/main.cpp"
		
		"src/theme_configuration.cpp")

find_package(Catch2 CONFIG REQUIRED)
target_link_libraries(editors_test PRIVATE editors Catch2::Catch2)
set_target_properties(editors_test
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
