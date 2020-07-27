add_library(editors SHARED)

target_sources(editors
	PRIVATE
		"src/binary/components.cpp"
		"src/binary/contents_region.cpp"

		"src/code/caret_set.cpp"
		"src/code/contents_region.cpp"
		"src/code/line_number_display.cpp"
		"src/code/minimap.cpp"

		"src/buffer.cpp"
		"src/editor.cpp"
		"src/main.cpp")
target_link_libraries(editors PUBLIC codepad)
target_include_directories(editors
	PUBLIC "include/"
	PRIVATE "../common/include/")
set_target_properties(editors
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
		WINDOWS_EXPORT_ALL_SYMBOLS YES)
