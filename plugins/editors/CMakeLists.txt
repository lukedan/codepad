add_library(editors SHARED)

target_sources(editors
	PUBLIC
		"include/codepad/editors/binary/components.h"
		"include/codepad/editors/binary/contents_region.h"

		"include/codepad/editors/code/caret_gatherer.h"
		"include/codepad/editors/code/caret_set.h"
		"include/codepad/editors/code/contents_region.h"
		"include/codepad/editors/code/decoration_gatherer.h"
		"include/codepad/editors/code/fragment_generation.h"
		"include/codepad/editors/code/interpretation.h"
		"include/codepad/editors/code/line_number_display.h"
		"include/codepad/editors/code/linebreak_registry.h"
		"include/codepad/editors/code/minimap.h"
		"include/codepad/editors/code/theme.h"
		"include/codepad/editors/code/view.h"
		"include/codepad/editors/code/view_caching.h"
		"include/codepad/editors/code/whitespace_gatherer.h"

		"include/codepad/editors/buffer.h"
		"include/codepad/editors/buffer_manager.h"
		"include/codepad/editors/caret_set.h"
		"include/codepad/editors/decoration.h"
		"include/codepad/editors/editor.h"
		"include/codepad/editors/interaction_modes.h"
		"include/codepad/editors/json_parsers.inl"
		"include/codepad/editors/manager.h"
		"include/codepad/editors/overlapping_range_registry.h"
		"include/codepad/editors/theme_manager.h"
	PRIVATE
		"src/binary/components.cpp"
		"src/binary/contents_region.cpp"

		"src/code/caret_gatherer.cpp"
		"src/code/caret_set.cpp"
		"src/code/contents_region.cpp"
		"src/code/fragment_generation.cpp"
		"src/code/interpretation.cpp"
		"src/code/line_number_display.cpp"
		"src/code/minimap.cpp"
		"src/code/theme.cpp"

		"src/buffer.cpp"
		"src/decoration.cpp"
		"src/editor.cpp"
		"src/theme_manager.cpp"

		"src/main.cpp")
target_link_libraries(editors PUBLIC codepad_core)
target_include_directories(editors
	PUBLIC "include/"
	PRIVATE "../common/include/")
set_target_properties(editors
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
		WINDOWS_EXPORT_ALL_SYMBOLS YES)

add_subdirectory("test/")
