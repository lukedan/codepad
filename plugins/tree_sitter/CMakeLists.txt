add_library(tree_sitter SHARED)

target_sources(tree_sitter
	PRIVATE
		"tree-sitter/lib/src/lib.c"
		"src/interpretation_interface.cpp"
		"src/language_configuration.cpp"
		"src/main.cpp"
		"src/query.cpp")
target_link_libraries(tree_sitter PRIVATE codepad_core editors)
target_include_directories(tree_sitter
	PRIVATE
		"../common/include/"
		"tree-sitter/lib/include/"
		"include/")
set_target_properties(tree_sitter
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# parsers
target_sources(tree_sitter
	PRIVATE
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