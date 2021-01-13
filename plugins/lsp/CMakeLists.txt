add_library(lsp SHARED)

target_sources(lsp
	PRIVATE
		"src/types/client.cpp"
		"src/types/common.cpp"
		"src/types/diagnostics.cpp"
		"src/types/general.cpp"
		"src/types/language_features.cpp"
		"src/types/text_synchronization.cpp"
		"src/types/window.cpp"
		"src/types/workspace.cpp"
		"src/main.cpp")
target_link_libraries(lsp PUBLIC codepad_core)
target_include_directories(lsp
	PUBLIC "include/"
	PRIVATE "../common/include/")
set_target_properties(lsp
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
		WINDOWS_EXPORT_ALL_SYMBOLS YES)

find_package(RapidJSON CONFIG REQUIRED)
target_include_directories(lsp
	PUBLIC ${RapidJSON_INCLUDE_DIRS})
