cmake_minimum_required(VERSION 3.15)
project(codepad)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")


# options

# plugin interface
set(ENABLE_APIGEN NO CACHE BOOL "Whether to generate and build the plugin interface using apigen.")
set(APIGEN_PATH "./apigen" CACHE FILEPATH "Path to the apigen executable.")

# graphics backend
set(USE_CAIRO YES CACHE BOOL "Whether or not to include the Cairo graphics backend.")
if(UNIX)
	set(USE_GTK YES CACHE BOOL "Whether or not to use GTK3 on Linux.")
endif()

# backtrace
set(ENABLE_BACKTRACE YES CACHE BOOL "Whether or not to support printing backtraces.")


cmake_policy(SET CMP0092 NEW)


# packages
find_package(RapidJSON CONFIG REQUIRED)

if(USE_CAIRO)
	if(WIN32)
		# on windows, manually find cairo since there's no pkgconfig
		# https://github.com/Microsoft/vcpkg/issues/2563
		find_library(CAIRO_LIB_DEBUG cairod)
		find_library(CAIRO_LIB_RELEASE cairo)
		# TODO are these necessary?
		find_library(CAIRO_GOBJECT_LIB_DEBUG cairo-gobjectd)
		find_library(CAIRO_GOBJECT_LIB_RELEASE cairo-gobject)

		find_path(CAIRO_INCLUDE_DIRS cairo/cairo.h)

		if(
			CAIRO_LIB_DEBUG AND CAIRO_LIB_RELEASE AND
			CAIRO_GOBJECT_LIB_DEBUG AND CAIRO_GOBJECT_LIB_RELEASE AND
			CAIRO_INCLUDE_DIRS)

			if (CMAKE_BUILD_TYPE STREQUAL "Debug")
				set(CAIRO_LIBRARIES "${CAIRO_LIB_DEBUG}")
			else()
				set(CAIRO_LIBRARIES "${CAIRO_LIB_RELEASE}")
			endif()
			set(Cairo_FOUND YES)
		endif()
	else()
		find_package(Cairo)
	endif()

	if(Cairo_FOUND)
		# export targets
		if(NOT TARGET Cairo::Cairo)
			add_library(Cairo::Cairo INTERFACE IMPORTED)

			set_property(TARGET Cairo::Cairo PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${CAIRO_INCLUDE_DIRS})
			set_property(TARGET Cairo::Cairo PROPERTY INTERFACE_LINK_LIBRARIES ${CAIRO_LIBRARIES})
		endif()

		# TODO the cairo renderer depends on Pango for text shaping
		# find_package(Pango REQUIRED)
	else()
		message("USE_CAIRO is set to true, but Cairo has not been found.")
		set(USE_CAIRO NO)
	endif()
endif()

if(UNIX)
	find_package(Freetype REQUIRED)
	find_package(Fontconfig REQUIRED)
	if(USE_GTK)
		find_package(GTK3 REQUIRED)
	else()
		find_package(X11 REQUIRED)
	endif()
endif()


# main executable
add_executable(codepad)

target_compile_features(codepad
	PRIVATE cxx_std_20)
target_sources(codepad
	PRIVATE
		"src/core/globals.cpp"
		"src/core/logger_sinks.cpp"
		"src/core/plugins.cpp"
		"src/core/profiling.cpp"
		"src/core/setting_entries.cpp"
		"src/core/settings.cpp"

		"src/editors/binary/components.cpp"
		"src/editors/binary/contents_region.cpp"

		"src/editors/code/caret_set.cpp"
		"src/editors/code/contents_region.cpp"
		"src/editors/code/line_number_display.cpp"
		"src/editors/code/minimap.cpp"

		"src/editors/buffer.cpp"
		"src/editors/editor.cpp"

		"src/ui/elements/tabs/host.cpp"
		"src/ui/elements/tabs/tab.cpp"

		"src/ui/elements/button.cpp"
		"src/ui/elements/label.cpp"
		"src/ui/elements/scrollbar.cpp"
		"src/ui/elements/stack_panel.cpp"

		"src/ui/animation_path.cpp"
		"src/ui/animation_path_parser.cpp"
		"src/ui/arrangements.cpp"
		"src/ui/cairo_renderer_base.cpp"
		"src/ui/commands.cpp"
		"src/ui/element.cpp"
		"src/ui/element_parameters.cpp" 
		"src/ui/hotkey_registry.cpp"
		"src/ui/manager.cpp"
		"src/ui/misc.cpp"
		"src/ui/native_commands.cpp"
		"src/ui/panel.cpp"
		"src/ui/renderer.cpp"
		"src/ui/scheduler.cpp"
		"src/ui/window.cpp"

		"src/main.cpp")
target_compile_definitions(codepad
	PRIVATE UNICODE _UNICODE)
if(ENABLE_BACKTRACE)
	target_compile_definitions(codepad
		PRIVATE CP_LOG_STACKTRACE)
endif()


# set warning level
if(MSVC)
	target_compile_options(codepad
		PRIVATE /W4 /permissive-)
elseif(CMAKE_COMPILER_IS_GNUCXX)
	target_compile_options(codepad
		PRIVATE -Wall -Wextra -Wconversion)
endif()


# libraries
target_include_directories(codepad
	PRIVATE ${RAPIDJSON_INCLUDE_DIRS})
if(USE_CAIRO)
	target_compile_definitions(codepad
		PRIVATE CP_USE_CAIRO)
	target_link_libraries(codepad
		PRIVATE Cairo::Cairo)
endif()
# platform dependent sources & settings
if(WIN32)
	target_sources(codepad
		PRIVATE
			"src/os/windows/direct2d_renderer.cpp"
			"src/os/windows/dynamic_library.cpp"
			"src/os/windows/filesystem.cpp"
			"src/os/windows/misc.cpp"
			"src/os/windows/window.cpp"
			"src/os/windows/windows.cpp")
	target_compile_definitions(codepad
		PRIVATE CP_PLATFORM_WINDOWS NOMINMAX OEMRESOURCE)
	target_link_libraries(codepad
		PRIVATE gdi32 dwrite d3d11 d2d1 shlwapi Dwmapi imm32 windowscodecs)

	if(CMAKE_COMPILER_IS_GNUCXX)
		target_compile_options(codepad
			PRIVATE -Wa,-mbig-obj)
	elseif(MSVC)
		target_compile_options(codepad
			PRIVATE /bigobj)
		# debug slow compilation times
#		target_compile_options(codepad
#			PRIVATE /Bt /d1reportTime /d2cgsummary)
	endif()
elseif(UNIX)
	target_sources(codepad
		PRIVATE
			"src/os/linux/filesystem.cpp"
			"src/os/linux/linux.cpp")
	target_compile_definitions(codepad
		PRIVATE CP_PLATFORM_UNIX)
	target_link_libraries(codepad
		PRIVATE pthread) # TODO why link pthread?
	if(ENABLE_BACKTRACE)
		target_link_options(codepad
			PRIVATE -rdynamic) # enables detailed stacktrace
	endif()

	if(USE_GTK)
		target_sources(codepad
			PRIVATE
				"src/os/linux/gtk/linux.cpp")
		target_compile_definitions(codepad
			PRIVATE CP_USE_GTK)
		target_link_libraries(codepad
			PRIVATE Gtk3::Gtk)
	else()
		target_sources(codepad
			PRIVATE
				"src/os/linux/x11/linux.cpp")
		target_link_libraries(codepad
			PRIVATE ${X11_LIBRARIES})
	endif()
else()
	message(FATAL_ERROR "unrecognized platform")
endif()

if(CMAKE_COMPILER_IS_GNUCXX) # TODO rid of this after filesystem support is complete
	target_link_libraries(codepad
		PRIVATE stdc++fs)
endif()


# codegen
if(${ENABLE_APIGEN})
	if(NOT EXISTS "${APIGEN_PATH}")
		message(
			FATAL_ERROR
				"Build cannot continue because a valid apigen executable is not found. Either specify a valid path "
				"to apigen or use -DENABLE_APIGEN=NO to build codepad without plugin support.")
	endif()

	target_compile_definitions(codepad
		PRIVATE CP_ENABLE_APIGEN)

	get_target_property(APIGEN_INCLUDES codepad INCLUDE_DIRECTORIES)
	list(TRANSFORM APIGEN_INCLUDES PREPEND "-I")
	get_target_property(APIGEN_DEFINES codepad COMPILE_DEFINITIONS)
	list(TRANSFORM APIGEN_DEFINES PREPEND "-D")
	get_target_property(APIGEN_COMPILE_OPTIONS codepad COMPILE_OPTIONS)

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

	get_target_property(CODEPAD_SOURCES codepad SOURCES)

	add_custom_command(
		OUTPUT "${API_HEADER_PATH}" "${HOST_HEADER_PATH}" "${HOST_SOURCE_PATH}" "${COLLECTOR_PATH}"
		COMMAND
			"${APIGEN_PATH}"
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
		DEPENDS ${CODEPAD_SOURCES} "${EXPORT_FILE_PATH}" "${APIGEN_PATH}"
		COMMENT "Invoking apigen..."
		VERBATIM)

	target_sources(codepad
		PRIVATE "${HOST_HEADER_PATH}" "${HOST_SOURCE_PATH}")
	target_include_directories(codepad
		PRIVATE "${APIGEN_OUT_DIR}")
	target_compile_definitions(codepad
		PRIVATE
			CP_API_STRUCT=${API_STRUCT_NAME}
			CP_API_STRUCT_INIT=${API_STRUCT_INIT_NAME})

	# collect executable
	add_executable(collect)
	target_compile_features(collect
		PRIVATE cxx_std_20)
	target_include_directories(collect
		PRIVATE $<TARGET_PROPERTY:codepad,INCLUDE_DIRECTORIES>)
	target_compile_definitions(collect
		PRIVATE $<TARGET_PROPERTY:codepad,COMPILE_DEFINITIONS>)
	target_link_libraries(collect
		PRIVATE $<TARGET_PROPERTY:codepad,LINK_LIBRARIES>)
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
	target_compile_definitions(codepad
		PRIVATE
			"CP_APIGEN_SIZES_ALIGNMENTS_H=<${SIZES_ALIGNMENTS_HEADER_NAME}>"
			"CP_APIGEN_API_H=<${API_HEADER_NAME}>"
			"CP_APIGEN_HOST_H=<${HOST_HEADER_NAME}>")

	# dependency target for plugins
	add_custom_target(codepad_api_header
		DEPENDS "${API_HEADER_PATH}" "${SIZES_ALIGNMENTS_PATH}")

	# import plugin targets
	include(plugins/command_pack/CMakeLists.txt)
	add_dependencies(codepad command_pack)
endif()
