add_library(lsp SHARED)

target_sources(lsp
	PUBLIC
		"include/codepad/lsp/backends/stdio.h"

		"include/codepad/lsp/types/client.h"
		"include/codepad/lsp/types/common.h"
		"include/codepad/lsp/types/diagnostics.h"
		"include/codepad/lsp/types/general.h"
		"include/codepad/lsp/types/language_features.h"
		"include/codepad/lsp/types/serialization.h"
		"include/codepad/lsp/types/text_synchronization.h"
		"include/codepad/lsp/types/window.h"
		"include/codepad/lsp/types/workspace.h"

		"include/codepad/lsp/backend.h"
		"include/codepad/lsp/client.h"
		"include/codepad/lsp/uri.h"
	PRIVATE
		"src/types/client.cpp"
		"src/types/common.cpp"
		"src/types/diagnostics.cpp"
		"src/types/general.cpp"
		"src/types/language_features.cpp"
		"src/types/text_synchronization.cpp"
		"src/types/window.cpp"
		"src/types/workspace.cpp"
		"src/client.cpp"
		"src/main.cpp"
		"src/uri.cpp")
target_link_libraries(lsp PUBLIC codepad_core editors)
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

find_package(uriparser CONFIG REQUIRED char)
target_link_libraries(lsp
	PRIVATE uriparser::uriparser)
