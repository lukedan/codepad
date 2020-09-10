add_library(tree_sitter SHARED)

target_sources(tree_sitter
	PRIVATE
		"tree-sitter/lib/src/lib.c"
		"src/main.cpp")
target_link_libraries(tree_sitter PRIVATE codepad_core editors)
target_include_directories(tree_sitter
	PRIVATE "../common/include" "tree-sitter/lib/include")
set_target_properties(tree_sitter
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

target_sources(tree_sitter
	PRIVATE
		"languages/tree-sitter-c/src/parser.c"

		"languages/tree-sitter-cpp/src/parser.c"
		"languages/tree-sitter-cpp/src/scanner.cc")
