cmake_minimum_required(VERSION 3.8)
project(codepad)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")

# options
set(ENABLE_PLUGINS NO CACHE BOOL "Whether to generate and build the plugin interface.")
set(APIGEN_PATH "./apigen" CACHE FILEPATH "Path to the apigen executable.")
if(UNIX)
	set(USE_GTK YES CACHE BOOL "Whether or not to use GTK on Linux")
endif()

# packages
find_package(RapidJSON CONFIG REQUIRED)
if(UNIX)
	find_package(Freetype REQUIRED)
	find_package(Fontconfig REQUIRED)
	if(USE_GTK)
		find_package(GTK3 REQUIRED)
	else()
		find_package(X11 REQUIRED)
	endif()
endif()

# source files
set(SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/codepad")
set(INTERPLATFORM_SOURCES
	"${SOURCE_PATH}/core/json/misc.h"
	"${SOURCE_PATH}/core/json/parsing.h"
	"${SOURCE_PATH}/core/json/rapidjson.h"
	"${SOURCE_PATH}/core/json/storage.h"

	"${SOURCE_PATH}/core/assert.h"
	"${SOURCE_PATH}/core/bst.h"
	"${SOURCE_PATH}/core/encodings.h"
	"${SOURCE_PATH}/core/event.h"
	"${SOURCE_PATH}/core/globals.cpp"
	"${SOURCE_PATH}/core/logger_sinks.h"
	"${SOURCE_PATH}/core/logging.h"
	"${SOURCE_PATH}/core/math.h"
	"${SOURCE_PATH}/core/misc.h"
	"${SOURCE_PATH}/core/profiling.h"
	"${SOURCE_PATH}/core/settings.h"

	"${SOURCE_PATH}/editors/binary/components.h"
	"${SOURCE_PATH}/editors/binary/contents_region.h"

	"${SOURCE_PATH}/editors/code/caret_set.cpp"
	"${SOURCE_PATH}/editors/code/caret_set.h"
	"${SOURCE_PATH}/editors/code/components.h"
	"${SOURCE_PATH}/editors/code/contents_region.cpp"
	"${SOURCE_PATH}/editors/code/contents_region.h"
	"${SOURCE_PATH}/editors/code/interpretation.h"
	"${SOURCE_PATH}/editors/code/linebreak_registry.h"
	"${SOURCE_PATH}/editors/code/rendering.h"
	"${SOURCE_PATH}/editors/code/theme.h"
	"${SOURCE_PATH}/editors/code/view.h"
	"${SOURCE_PATH}/editors/code/view_caching.h"

	"${SOURCE_PATH}/editors/buffer.cpp"
	"${SOURCE_PATH}/editors/buffer.h"
	"${SOURCE_PATH}/editors/buffer_manager.h"
	"${SOURCE_PATH}/editors/caret_set.h"
	"${SOURCE_PATH}/editors/editor.h"
	"${SOURCE_PATH}/editors/generic_settings.cpp"
	"${SOURCE_PATH}/editors/interaction_modes.h"

	"${SOURCE_PATH}/os/current/all.h"
	"${SOURCE_PATH}/os/current/misc.h"
	"${SOURCE_PATH}/os/current/window.h"
	"${SOURCE_PATH}/os/filesystem.h"
	"${SOURCE_PATH}/os/misc.h"

	"${SOURCE_PATH}/ui/tabs/animated_tab_button_panel.h"
	"${SOURCE_PATH}/ui/tabs/host.cpp"
	"${SOURCE_PATH}/ui/tabs/host.h"
	"${SOURCE_PATH}/ui/tabs/manager.h"
	"${SOURCE_PATH}/ui/tabs/split_panel.h"
	"${SOURCE_PATH}/ui/tabs/tab.cpp"
	"${SOURCE_PATH}/ui/tabs/tab.h"

	"${SOURCE_PATH}/ui/animation.h"
	"${SOURCE_PATH}/ui/animation_path.cpp"
	"${SOURCE_PATH}/ui/animation_path.h"
	"${SOURCE_PATH}/ui/arrangements.cpp"
	"${SOURCE_PATH}/ui/arrangements.h"
	"${SOURCE_PATH}/ui/commands.h"
	"${SOURCE_PATH}/ui/common_elements.cpp"
	"${SOURCE_PATH}/ui/common_elements.h"
	"${SOURCE_PATH}/ui/config_parsers.h"
	"${SOURCE_PATH}/ui/element.cpp"
	"${SOURCE_PATH}/ui/element.h"
	"${SOURCE_PATH}/ui/element_classes.h"
	"${SOURCE_PATH}/ui/element_parameters.h"
	"${SOURCE_PATH}/ui/hotkey_registry.h"
	"${SOURCE_PATH}/ui/manager.cpp"
	"${SOURCE_PATH}/ui/manager.h"
	"${SOURCE_PATH}/ui/misc.h"
	"${SOURCE_PATH}/ui/native_commands.cpp"
	"${SOURCE_PATH}/ui/native_commands.h"
	"${SOURCE_PATH}/ui/panel.cpp"
	"${SOURCE_PATH}/ui/panel.h"
	"${SOURCE_PATH}/ui/renderer.cpp"
	"${SOURCE_PATH}/ui/renderer.h"
	"${SOURCE_PATH}/ui/scheduler.h"
	"${SOURCE_PATH}/ui/text_rendering.h"
	"${SOURCE_PATH}/ui/window.cpp"
	"${SOURCE_PATH}/ui/window.h"

	"${SOURCE_PATH}/main.cpp")

set(WINDOWS_SOURCES
	"${SOURCE_PATH}/os/windows/direct2d_renderer.h"
	"${SOURCE_PATH}/os/windows/filesystem.cpp"
	"${SOURCE_PATH}/os/windows/misc.h"
	"${SOURCE_PATH}/os/windows/window.h"
	"${SOURCE_PATH}/os/windows/windows.cpp"
	"${SOURCE_PATH}/os/windows.h")

set(UNIX_SOURCES
	"${SOURCE_PATH}/os/linux/filesystem.cpp"
	"${SOURCE_PATH}/os/linux/font.h"
	"${SOURCE_PATH}/os/linux/misc.h"
	"${SOURCE_PATH}/os/linux/renderer.h"
	"${SOURCE_PATH}/os/linux/window.h"
	"${SOURCE_PATH}/os/linux.h")

# main executable
add_executable(codepad)

target_compile_features(codepad
	PRIVATE cxx_std_17)
target_sources(codepad
	PRIVATE "${INTERPLATFORM_SOURCES}")
target_compile_definitions(codepad
	PRIVATE UNICODE _UNICODE CP_LOG_STACKTRACE)

# set warning level
if(MSVC)
	target_compile_options(codepad
		PRIVATE /W3)
elseif(CMAKE_COMPILER_IS_GNUCXX)
	target_compile_options(codepad
		PRIVATE -Wall -Wextra -Wconversion)
endif()

# libraries
target_include_directories(codepad
	PRIVATE "${RAPIDJSON_INCLUDE_DIRS}")
# platform dependent stuff
if(WIN32)
	target_sources(codepad
		PRIVATE "${WINDOWS_SOURCES}")
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
	endif()
elseif(UNIX)
	target_sources(codepad
		PRIVATE "${UNIX_SOURCES}")
	target_compile_definitions(codepad
		PRIVATE CP_PLATFORM_UNIX)
	target_compile_options(codepad
		PRIVATE -rdynamic)
	target_link_libraries(codepad
		PRIVATE Fontconfig::Fontconfig pthread) # TODO why link pthread?
	if(USE_GTK)
		target_sources(codepad
			PRIVATE
				${SOURCE_PATH}/os/linux/gtk/linux.cpp
				${SOURCE_PATH}/os/linux/gtk/misc.h
				${SOURCE_PATH}/os/linux/gtk/renderer.h
				${SOURCE_PATH}/os/linux/gtk/window.h)
		target_compile_definitions(codepad
			PRIVATE CP_USE_GTK)
		target_link_libraries(codepad
			PRIVATE Gtk3::Gtk)
	else()
		target_sources(codepad
			PRIVATE
				${SOURCE_PATH}/os/linux/x11/linux.cpp
				${SOURCE_PATH}/os/linux/x11/misc.h
				${SOURCE_PATH}/os/linux/x11/renderer.h
				${SOURCE_PATH}/os/linux/x11/window.h)
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
if(${ENABLE_PLUGINS})
	if(NOT EXISTS "${APIGEN_PATH}")
		message(
			FATAL_ERROR
				"Build cannot continue because a valid apigen executable is not found. Either specify a valid path "
				"to apigen or use -DENABLE_PLUGINS=NO to build codepad without plugin support.")
	endif()

	get_target_property(APIGEN_INCLUDES codepad INCLUDE_DIRECTORIES)
	list(TRANSFORM APIGEN_INCLUDES PREPEND "-I")
	get_target_property(APIGEN_DEFINES codepad COMPILE_DEFINITIONS)
	list(TRANSFORM APIGEN_DEFINES PREPEND "-D")
	get_target_property(APIGEN_COMPILE_OPTIONS codepad COMPILE_OPTIONS)

	set(EXPORT_FILE_PATH "${SOURCE_PATH}/api_export.h")
	set(APIGEN_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/apigen_out")
	set(API_HEADER_PATH "${APIGEN_OUT_DIR}/api_header.h")
	set(HOST_HEADER_PATH "${APIGEN_OUT_DIR}/host.h")
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
			--
			-std=c++17
			${APIGEN_INCLUDES}
			${APIGEN_DEFINES}
			${APIGEN_COMPILE_OPTIONS}
			"${EXPORT_FILE_PATH}"
		DEPENDS "${CODEPAD_SOURCES}" "${EXPORT_FILE_PATH}" "${APIGEN_PATH}"
		COMMENT "Invoking apigen..."
		VERBATIM)

	target_sources(codepad
		PRIVATE "${HOST_HEADER_PATH}" "${HOST_SOURCE_PATH}")
	target_include_directories(codepad
		PRIVATE "${APIGEN_OUT_DIR}")
	target_compile_definitions(codepad
		PRIVATE CP_ENABLE_PLUGINS)

	# collect executable
	add_executable(collect)
	target_compile_features(collect
		PRIVATE cxx_std_17)
	target_include_directories(collect
		PRIVATE $<TARGET_PROPERTY:codepad,INCLUDE_DIRECTORIES>)
	target_compile_definitions(collect
		PRIVATE $<TARGET_PROPERTY:codepad,COMPILE_DEFINITIONS>)
	target_sources(collect
		PRIVATE "${COLLECTOR_PATH}")
	# TODO
endif()
