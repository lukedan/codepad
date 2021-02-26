cmake_minimum_required(VERSION 3.15)
project(codepad)

cmake_policy(SET CMP0092 NEW) # don't add /W3 for MSVC

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")


# options
# plugin interface
set(CODEPAD_ENABLE_APIGEN NO CACHE BOOL "Whether to generate and build the plugin interface using apigen.")
set(CODEPAD_APIGEN_PATH "./apigen" CACHE FILEPATH "Path to the apigen executable.")

# graphics backend
set(CODEPAD_USE_CAIRO YES CACHE BOOL "Whether or not to include the Cairo graphics backend.")
set(CODEPAD_USE_SKIA YES CACHE BOOL "Whether or not to include the Skia graphics backend.")
if(UNIX)
	set(CODEPAD_USE_GTK YES CACHE BOOL "Whether or not to use GTK3 on Linux.")
endif()

# backtrace
set(CODEPAD_ENABLE_BACKTRACE YES CACHE BOOL "Whether or not to support printing backtraces.")


# main library
add_library(codepad_core SHARED)
target_compile_features(codepad_core PUBLIC cxx_std_20)
target_sources(codepad_core
	PUBLIC
		"include/codepad/core/json/default_engine.h"
		"include/codepad/core/json/misc.h"
		"include/codepad/core/json/parsing.h"
		"include/codepad/core/json/rapidjson.h"
		"include/codepad/core/json/storage.h"

		"include/codepad/core/assert.h"
		"include/codepad/core/binary_tree.h"
		"include/codepad/core/color.h"
		"include/codepad/core/encodings.h"
		"include/codepad/core/event.h"
		"include/codepad/core/intrusive_priority_queue.h"
		"include/codepad/core/json_parsers.inl"
		"include/codepad/core/logger_sinks.h"
		"include/codepad/core/logging.h"
		"include/codepad/core/math.h"
		"include/codepad/core/misc.h"
		"include/codepad/core/plugins.h"
		"include/codepad/core/profiling.h"
		"include/codepad/core/red_black_tree.h"
		"include/codepad/core/settings.h"
		"include/codepad/core/threading.h"

		"include/codepad/os/dynamic_library.h"
		"include/codepad/os/filesystem.h"
		"include/codepad/os/misc.h"

		"include/codepad/ui/backends/cairo_renderer_base.h"
		"include/codepad/ui/backends/pango_harfbuzz_text_context.h"
		"include/codepad/ui/backends/skia_renderer_base.h"

		"include/codepad/ui/elements/tabs/animated_tab_button_panel.h"
		"include/codepad/ui/elements/tabs/host.h"
		"include/codepad/ui/elements/tabs/manager.h"
		"include/codepad/ui/elements/tabs/split_panel.h"
		"include/codepad/ui/elements/tabs/tab.h"

		"include/codepad/ui/elements/button.h"
		"include/codepad/ui/elements/label.h"
		"include/codepad/ui/elements/scroll_viewport.h"
		"include/codepad/ui/elements/scrollbar.h"
		"include/codepad/ui/elements/stack_panel.h"
		"include/codepad/ui/elements/text_edit.h"

		"include/codepad/ui/animation.h"
		"include/codepad/ui/arrangements.h"
		"include/codepad/ui/commands.h"
		"include/codepad/ui/config_parsers.h"
		"include/codepad/ui/element.h"
		"include/codepad/ui/element_classes.h"
		"include/codepad/ui/element_parameters.h"
		"include/codepad/ui/hotkey_registry.h"
		"include/codepad/ui/json_parsers.inl"
		"include/codepad/ui/manager.h"
		"include/codepad/ui/misc.h"
		"include/codepad/ui/panel.h"
		"include/codepad/ui/property_path.h"
		"include/codepad/ui/property_path_parser.h"
		"include/codepad/ui/renderer.h"
		"include/codepad/ui/scheduler.h"
		"include/codepad/ui/window.h"

		"include/codepad/api_export.h"
		"include/codepad/apigen_definitions.h"
	PRIVATE
		"src/core/globals.cpp"
		"src/core/logger_sinks.cpp"
		"src/core/logging.cpp"
		"src/core/plugins.cpp"
		"src/core/profiling.cpp"
		
		"src/core/settings.cpp"

		"src/ui/elements/tabs/host.cpp"
		"src/ui/elements/tabs/manager.cpp"
		"src/ui/elements/tabs/tab.cpp"

		"src/ui/elements/button.cpp"
		"src/ui/elements/label.cpp"
		"src/ui/elements/scroll_viewport.cpp"
		"src/ui/elements/scrollbar.cpp"
		"src/ui/elements/stack_panel.cpp"
		"src/ui/elements/text_edit.cpp"

		"src/ui/arrangements.cpp"
		"src/ui/commands.cpp"
		"src/ui/element.cpp"
		"src/ui/element_parameters.cpp" 
		"src/ui/enum_parsers.cpp"
		"src/ui/hotkey_registry.cpp"
		"src/ui/manager.cpp"
		"src/ui/misc.cpp"
		"src/ui/panel.cpp"
		"src/ui/property_path.cpp"
		"src/ui/property_path_parser.cpp"
		"src/ui/renderer.cpp"
		"src/ui/scheduler.cpp"
		"src/ui/window.cpp")

target_include_directories(codepad_core PUBLIC "include/")

# compile definitions
target_compile_definitions(codepad_core
	PUBLIC UNICODE _UNICODE)
if(CODEPAD_ENABLE_BACKTRACE)
	target_compile_definitions(codepad_core
		PUBLIC CP_LOG_STACKTRACE)
endif()
if(CODEPAD_USE_CAIRO)
	target_sources(codepad_core
		PRIVATE
			"src/ui/backends/pango_harfbuzz_text_context.cpp"
			"src/ui/backends/cairo_renderer_base.cpp")
	target_compile_definitions(codepad_core
		PUBLIC CP_USE_CAIRO)
endif()
if(CODEPAD_USE_SKIA)
	target_sources(codepad_core
		PRIVATE
			"src/ui/backends/pango_harfbuzz_text_context.cpp"
			"src/ui/backends/skia_renderer_base.cpp")
	target_compile_definitions(codepad_core
		PUBLIC CP_USE_SKIA)
endif()

# enable export so that plugins can link to the executable
set_target_properties(codepad_core PROPERTIES ENABLE_EXPORTS YES)

# set warning level
if(MSVC)
	target_compile_options(codepad_core
		PRIVATE /W4 /permissive-)
elseif(CMAKE_COMPILER_IS_GNUCXX)
	target_compile_options(codepad_core
		PRIVATE -Wall -Wextra -Wconversion)
endif()

# libraries
find_package(RapidJSON CONFIG REQUIRED)
target_include_directories(codepad_core
	PUBLIC ${RapidJSON_INCLUDE_DIRS})

# platform dependent sources & settings
if(WIN32)
	target_sources(codepad_core
		PUBLIC
			"include/codepad/os/windows/cairo_renderer.h"
			"include/codepad/os/windows/direct2d_renderer.h"
			"include/codepad/os/windows/misc.h"
			"include/codepad/os/windows/skia_renderer.h"
			"include/codepad/os/windows/window.h"
		PRIVATE
			"src/os/windows/direct2d_renderer.cpp"
			"src/os/windows/dynamic_library.cpp"
			"src/os/windows/filesystem.cpp"
			"src/os/windows/misc.cpp"
			"src/os/windows/window.cpp"
			"src/os/windows/windows.cpp")
	target_compile_definitions(codepad_core
		PUBLIC CP_PLATFORM_WINDOWS NOMINMAX OEMRESOURCE)
	target_link_libraries(codepad_core
		PUBLIC gdi32 dwrite d3d11 d2d1 shlwapi Dwmapi imm32 windowscodecs)
	set_target_properties(codepad_core PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS YES)

	if(CMAKE_COMPILER_IS_GNUCXX)
		target_compile_options(codepad_core
			PRIVATE -Wa,-mbig-obj)
	elseif(MSVC)
		target_compile_options(codepad_core
			PRIVATE /bigobj)
		# uncomment to debug slow compilation times
#		target_compile_options(codepad_core
#			PRIVATE /Bt /d1reportTime /d2cgsummary)
		# enable detailed memory leak report (not THAT effective)
#		target_compile_definitions(codepad_core
#			PUBLIC _CRTDBG_MAP_ALLOC)
	endif()

	# finding cairo and dependencies on windows is quite different from finding cairo on linux
	if(CODEPAD_USE_CAIRO)
		find_package(harfbuzz REQUIRED)
		find_package(unofficial-glib CONFIG REQUIRED)
		find_package(unofficial-cairo CONFIG REQUIRED)

		# the cairo renderer depends on Pango for text shaping
		# pango include dir
		find_path(Pango_INCLUDE_DIR
			NAMES pango/pango.h
			PATHS ${Pango_PKGCONF_INCLUDE_DIRS}
			PATH_SUFFIXES pango-1.0)
		# pango libs
		find_library(Pango_LIBRARY
			NAMES pango-1.0
			PATHS ${Pango_PKGCONF_LIBRARY_DIRS})
		find_library(PangoCairo_LIBRARY
			NAMES pangocairo-1.0
			PATHS ${Pango_PKGCONF_LIBRARY_DIRS})
		# create target
		add_library(Pango::Pango INTERFACE IMPORTED)
		target_include_directories(Pango::Pango INTERFACE ${Pango_INCLUDE_DIR})
		target_link_libraries(Pango::Pango INTERFACE ${Pango_LIBRARY} ${PangoCairo_LIBRARY})

		target_link_libraries(codepad_core
			PUBLIC
				unofficial::cairo::cairo unofficial::cairo::cairo-gobject
				Pango::Pango harfbuzz::harfbuzz
				unofficial::glib::gobject)
	endif()

	if(CODEPAD_USE_SKIA)
		find_package(skia CONFIG REQUIRED)

		target_link_libraries(codepad_core
			PUBLIC
				skia skia::skia)
	endif()
elseif(UNIX)
	target_sources(codepad_core
		PUBLIC
			"include/codepad/os/linux/window.h"
		PRIVATE
			"src/os/linux/dynamic_library.cpp"
			"src/os/linux/filesystem.cpp"
			"src/os/linux/linux.cpp")
	target_compile_definitions(codepad_core
		PUBLIC CP_PLATFORM_UNIX)
	target_link_libraries(codepad_core
		PUBLIC dl pthread) # TODO why link pthread?
	if(CODEPAD_ENABLE_BACKTRACE)
		target_link_options(codepad_core
			PRIVATE -rdynamic) # enables detailed stacktrace
	endif()

	find_package(PkgConfig REQUIRED)

	if(CODEPAD_USE_GTK)
		target_sources(codepad_core
			PUBLIC
				"include/codepad/os/linux/gtk/cairo_renderer.h"
				"include/codepad/os/linux/gtk/misc.h"
				"include/codepad/os/linux/gtk/window.h"
			PRIVATE
				"src/os/linux/gtk/linux.cpp"
				"src/os/linux/gtk/misc.cpp"
				"src/os/linux/gtk/window.cpp")
		target_compile_definitions(codepad_core
			PUBLIC CP_USE_GTK)
		find_package(GTK3 REQUIRED)
		target_link_libraries(codepad_core
			PUBLIC Gtk3::Gtk)
	else()
		target_sources(codepad_core
			PUBLIC
				"include/codepad/os/linux/x11/misc.h"
				"include/codepad/os/linux/x11/window.h"
			PRIVATE
				"src/os/linux/x11/linux.cpp")
		find_package(X11 REQUIRED)
		target_link_libraries(codepad_core
			PUBLIC ${X11_LIBRARIES})
	endif()

	if(CODEPAD_USE_CAIRO)
		find_package(Cairo REQUIRED)
		find_package(Pango REQUIRED)

		pkg_check_modules(harfbuzz REQUIRED IMPORTED_TARGET harfbuzz)

		add_library(Cairo::Cairo INTERFACE IMPORTED)
		target_include_directories(Cairo::Cairo INTERFACE ${CAIRO_INCLUDE_DIRS})
		target_link_libraries(Cairo::Cairo INTERFACE ${CAIRO_LIBRARIES})

		add_library(Pango::Pango INTERFACE IMPORTED)
		target_include_directories(Pango::Pango INTERFACE ${Pango_INCLUDE_DIR})
		target_link_libraries(Pango::Pango INTERFACE ${Pango_LIBRARY} ${PangoCairo_LIBRARY})

		target_link_libraries(codepad_core
			PUBLIC
				Cairo::Cairo Pango::Pango PkgConfig::harfbuzz)
	endif()
else()
	message(FATAL_ERROR "unrecognized platform")
endif()
# cairo libraries that can be found using find_package()
if (CODEPAD_USE_CAIRO)
	find_package(Freetype REQUIRED)
	find_package(Fontconfig REQUIRED)

	target_link_libraries(codepad_core
		PUBLIC
			Freetype::Freetype Fontconfig::Fontconfig)
endif()

if(CMAKE_COMPILER_IS_GNUCXX) # gcc needs this for filesystem to work
	target_link_libraries(codepad_core
		PUBLIC stdc++fs)
endif()


# codegen
if(${CODEPAD_ENABLE_APIGEN})
	if(NOT EXISTS "${CODEPAD_APIGEN_PATH}")
		message(
			FATAL_ERROR
				"Build cannot continue because a valid apigen executable is not found. Either specify a valid path "
				"to apigen or use -DCODEPAD_ENABLE_APIGEN=NO to build codepad_core without plugin support.")
	endif()

	target_compile_definitions(codepad_core
		PUBLIC CP_ENABLE_APIGEN)

	get_target_property(APIGEN_INCLUDES codepad_core INCLUDE_DIRECTORIES)
	list(TRANSFORM APIGEN_INCLUDES PREPEND "-I")
	get_target_property(APIGEN_DEFINES codepad_core COMPILE_DEFINITIONS)
	list(TRANSFORM APIGEN_DEFINES PREPEND "-D")
	get_target_property(APIGEN_COMPILE_OPTIONS codepad_core COMPILE_OPTIONS)

	set(API_HEADER_NAME api_header.h)
	set(HOST_HEADER_NAME host.h)
	set(SIZES_ALIGNMENTS_HEADER_NAME sizes_alignments.h)

	set(API_STRUCT_NAME codepad_api)
	set(API_STRUCT_INIT_NAME codepad_api_init)

	set(EXPORT_FILE_PATH "src/api_export.h")

	set(APIGEN_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/apigen_out")

	set(API_HEADER_PATH "${APIGEN_OUT_DIR}/${API_HEADER_NAME}")
	set(HOST_HEADER_PATH "${APIGEN_OUT_DIR}/${HOST_HEADER_NAME}")
	set(SIZES_ALIGNMENTS_PATH "${APIGEN_OUT_DIR}/${SIZES_ALIGNMENTS_HEADER_NAME}")
	set(HOST_SOURCE_PATH "${APIGEN_OUT_DIR}/host.cpp")
	set(COLLECTOR_PATH "${APIGEN_OUT_DIR}/collect.cpp")

	get_target_property(CODEPAD_SOURCES codepad_core SOURCES)

	add_custom_command(
		OUTPUT "${API_HEADER_PATH}" "${HOST_HEADER_PATH}" "${HOST_SOURCE_PATH}" "${COLLECTOR_PATH}"
		COMMAND
			"${CODEPAD_APIGEN_PATH}"
			"-api_header_file=${API_HEADER_PATH}"
			"-host_header_file=${HOST_HEADER_PATH}"
			"-host_source_file=${HOST_SOURCE_PATH}"
			"-collect_source_file=${COLLECTOR_PATH}"
			"-additional_host_include=${EXPORT_FILE_PATH}"
			-api_struct_name=${API_STRUCT_NAME}
			-api_initializer_name=${API_STRUCT_INIT_NAME}
			--
			-std=c++2a
			${APIGEN_INCLUDES}
			${APIGEN_DEFINES}
			${APIGEN_COMPILE_OPTIONS}
			"${EXPORT_FILE_PATH}"
		DEPENDS ${CODEPAD_SOURCES} "${EXPORT_FILE_PATH}" "${CODEPAD_APIGEN_PATH}"
		COMMENT "Invoking apigen..."
		VERBATIM)

	target_sources(codepad_core
		PRIVATE "${HOST_HEADER_PATH}" "${HOST_SOURCE_PATH}")
	target_include_directories(codepad_core
		PRIVATE "${APIGEN_OUT_DIR}")
	target_compile_definitions(codepad_core
		PRIVATE
			CP_API_STRUCT=${API_STRUCT_NAME}
			CP_API_STRUCT_INIT=${API_STRUCT_INIT_NAME})

	# collect executable
	add_executable(collect)
	target_compile_features(collect
		PRIVATE cxx_std_20)
	target_include_directories(collect
		PRIVATE $<TARGET_PROPERTY:codepad_core,INCLUDE_DIRECTORIES>)
	target_compile_definitions(collect
		PRIVATE $<TARGET_PROPERTY:codepad_core,COMPILE_DEFINITIONS>)
	target_link_libraries(collect
		PRIVATE $<TARGET_PROPERTY:codepad_core,LINK_LIBRARIES>)
	target_sources(collect
		PRIVATE "${COLLECTOR_PATH}")

	# add all source files except main.cpp to collect
	set(CODEPAD_COLLECT_SOURCES ${CODEPAD_SOURCES})
	list(REMOVE_ITEM CODEPAD_COLLECT_SOURCES "src/main.cpp")
	target_sources(collect
		PRIVATE ${CODEPAD_COLLECT_SOURCES} "${HOST_SOURCE_PATH}")

	if(WIN32)
		# add necessary compile options on windows
		if(CMAKE_COMPILER_IS_GNUCXX)
			target_compile_options(collect
				PRIVATE -Wa,-mbig-obj)
		elseif(MSVC)
			target_compile_options(collect
				PRIVATE /bigobj)
		endif()
	endif()

	# run collect & generate sizes_alignments.h
	add_custom_command(
		OUTPUT "${SIZES_ALIGNMENTS_PATH}"
		COMMAND $<TARGET_FILE:collect> > "${SIZES_ALIGNMENTS_PATH}"
		DEPENDS collect ${CODEPAD_SOURCES} "${EXPORT_FILE_PATH}"
		COMMENT "Invoking collect..."
		VERBATIM)

	# set include paths
	target_compile_definitions(codepad_core
		PRIVATE
			"CP_APIGEN_SIZES_ALIGNMENTS_H=<${SIZES_ALIGNMENTS_HEADER_NAME}>"
			"CP_APIGEN_API_H=<${API_HEADER_NAME}>"
			"CP_APIGEN_HOST_H=<${HOST_HEADER_NAME}>")

	# dependency target for plugins
	add_custom_target(codepad_api_header
		DEPENDS "${API_HEADER_PATH}" "${SIZES_ALIGNMENTS_PATH}")
endif()


# import plugin targets
add_subdirectory("plugins/editors/")
add_subdirectory("plugins/command_pack/")
add_subdirectory("plugins/python_plugin_host_pybind11/")
add_subdirectory("plugins/tree_sitter/")
add_subdirectory("plugins/lsp/")


# main executable
add_executable(codepad)
target_link_libraries(codepad
	PRIVATE codepad_core)
target_sources(codepad
	PRIVATE
		"src/main.cpp")


# tests
add_subdirectory("test/")
