add_executable(unicode_dump)
configure_codepad_target(unicode_dump)

target_sources(unicode_dump
	PRIVATE "main.cpp")

target_link_libraries(unicode_dump PRIVATE codepad_core)
set_target_properties(unicode_dump
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
