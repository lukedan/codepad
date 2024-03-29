add_library(tree_sitter_lib STATIC)
configure_codepad_target(tree_sitter_lib)

target_sources(tree_sitter_lib
	PRIVATE
		"tree-sitter/lib/src/lib.c"

		"languages/tree-sitter-c/src/parser.c"

		"languages/tree-sitter-cpp/src/parser.c"
		"languages/tree-sitter-cpp/src/scanner.cc"

		"languages/tree-sitter-css/src/parser.c"
		"languages/tree-sitter-css/src/scanner.c"

		"languages/tree-sitter-html/src/parser.c"
		"languages/tree-sitter-html/src/scanner.cc"

		"languages/tree-sitter-javascript/src/parser.c"
		"languages/tree-sitter-javascript/src/scanner.c"

		"languages/tree-sitter-json/src/parser.c")
target_include_directories(tree_sitter_lib
	PUBLIC
		"tree-sitter/lib/include/")
set_property(TARGET tree_sitter_lib PROPERTY POSITION_INDEPENDENT_CODE ON)


add_library(tree_sitter SHARED)
configure_codepad_target(tree_sitter)

target_sources(tree_sitter
	PUBLIC
		"include/codepad/tree_sitter/highlight_collector.h"
		"include/codepad/tree_sitter/highlight_layer_iterator.h"
		"include/codepad/tree_sitter/interpretation_tag.h"
		"include/codepad/tree_sitter/language_configuration.h"
		"include/codepad/tree_sitter/manager.h"
		"include/codepad/tree_sitter/query.h"
		"include/codepad/tree_sitter/wrappers.h"
	PRIVATE
		"src/highlight_collector.cpp"
		"src/highlight_layer_iterator.cpp"
		"src/interpretation_tag.cpp"
		"src/language_configuration.cpp"
		"src/main.cpp"
		"src/manager.cpp"
		"src/query.cpp")
target_link_libraries(tree_sitter PUBLIC codepad_core editors tree_sitter_lib)
target_include_directories(tree_sitter
	PUBLIC
		"include/"
	PRIVATE
		"../common/include/")
set_target_properties(tree_sitter
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
