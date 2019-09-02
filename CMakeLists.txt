cmake_minimum_required(VERSION 3.8)
project(codepad)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/modules)

if(NOT DEFINED USE_SSE4)
	set(USE_SSE4 Yes)
endif()

if(UNIX)
	find_package(Freetype REQUIRED)
	set(USE_GTK Yes) # TODO
	find_package(Fontconfig REQUIRED)
	if(USE_GTK)
		find_package(GTK3 REQUIRED)
	else()
		find_package(X11 REQUIRED)
	endif()
endif()


add_executable(codepad)

target_compile_features(codepad
	PRIVATE
		cxx_std_17)
target_sources(codepad
	PRIVATE
		codepad/core/json/misc.h
		codepad/core/json/parsing.h
		codepad/core/json/rapidjson.h
		codepad/core/json/storage.h

		codepad/core/assert.h
		codepad/core/bst.h
		codepad/core/encodings.h
		codepad/core/event.h
		codepad/core/globals.cpp
		codepad/core/logger_sinks.h
		codepad/core/logging.h
		codepad/core/math.h
		codepad/core/misc.h
		codepad/core/profiling.h
		codepad/core/settings.h

		codepad/editors/binary/components.h
		codepad/editors/binary/contents_region.h

		codepad/editors/code/caret_set.cpp
		codepad/editors/code/caret_set.h
		codepad/editors/code/components.h
		codepad/editors/code/contents_region.cpp
		codepad/editors/code/contents_region.h
		codepad/editors/code/interpretation.h
		codepad/editors/code/linebreak_registry.h
		codepad/editors/code/rendering.h
		codepad/editors/code/theme.h
		codepad/editors/code/view.h
		codepad/editors/code/view_caching.h

		codepad/editors/buffer.cpp
		codepad/editors/buffer.h
		codepad/editors/buffer_manager.h
		codepad/editors/caret_set.h
		codepad/editors/editor.h
		codepad/editors/generic_settings.cpp
		codepad/editors/interaction_modes.h

		codepad/os/current/all.h
		codepad/os/current/misc.h
		codepad/os/current/window.h
		codepad/os/filesystem.h
		codepad/os/misc.h

		codepad/ui/tabs/animated_tab_button_panel.h
		codepad/ui/tabs/host.cpp
		codepad/ui/tabs/host.h
		codepad/ui/tabs/manager.h
		codepad/ui/tabs/split_panel.h
		codepad/ui/tabs/tab.cpp
		codepad/ui/tabs/tab.h

		codepad/ui/animation.h
		codepad/ui/animation_path.cpp
		codepad/ui/animation_path.h
		codepad/ui/arrangements.cpp
		codepad/ui/arrangements.h
		codepad/ui/commands.h
		codepad/ui/common_elements.cpp
		codepad/ui/common_elements.h
		codepad/ui/config_parsers.h
		codepad/ui/element.cpp
		codepad/ui/element.h
		codepad/ui/element_classes.h
		codepad/ui/element_parameters.h
		codepad/ui/hotkey_registry.h
		codepad/ui/manager.cpp
		codepad/ui/manager.h
		codepad/ui/misc.h
		codepad/ui/native_commands.cpp
		codepad/ui/native_commands.h
		codepad/ui/panel.cpp
		codepad/ui/panel.h
		codepad/ui/renderer.cpp
		codepad/ui/renderer.h
		codepad/ui/scheduler.h
		codepad/ui/text_rendering.h
		codepad/ui/window.cpp
		codepad/ui/window.h

		codepad/main.cpp)
target_compile_definitions(codepad
	PRIVATE
		UNICODE _UNICODE CP_LOG_STACKTRACE)

if(MSVC)
	target_compile_options(codepad
		PRIVATE
			/W3)
elseif(CMAKE_COMPILER_IS_GNUCXX)
	target_compile_options(codepad
		PRIVATE
			-Wall -Wextra -Wconversion)
	# TODO following 2 lines necessary?
#	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
#	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()

if(WIN32)
	target_sources(codepad
		PRIVATE
			codepad/os/windows/direct2d_renderer.h
			codepad/os/windows/filesystem.cpp
			codepad/os/windows/misc.h
			codepad/os/windows/window.h
			codepad/os/windows/windows.cpp
			codepad/os/windows.h)
	target_compile_definitions(codepad
		PRIVATE
			CP_PLATFORM_WINDOWS NOMINMAX OEMRESOURCE)
	target_link_libraries(codepad
		PRIVATE
			d2d1 dwrite gdi32 windowscodecs imm32)
	if(CMAKE_COMPILER_IS_GNUCXX)
		target_compile_options(codepad
			PRIVATE
				-Wa,-mbig-obj)
	endif()
elseif(UNIX)
	target_sources(codepad
		PRIVATE
			codepad/os/linux/filesystem.cpp
			codepad/os/linux/font.h
			codepad/os/linux/misc.h
			codepad/os/linux/renderer.h
			codepad/os/linux/window.h
			codepad/os/linux.h)
	target_compile_definitions(codepad
		PRIVATE
			CP_PLATFORM_UNIX)
	target_compile_options(codepad
		PRIVATE
			-rdynamic)
	target_link_libraries(codepad
		PRIVATE
			Fontconfig::Fontconfig pthread) # TODO why link pthread?
	if(USE_GTK)
		target_sources(codepad
			PRIVATE
				codepad/os/linux/gtk/linux.cpp
				codepad/os/linux/gtk/misc.h
				codepad/os/linux/gtk/renderer.h
				codepad/os/linux/gtk/window.h)
		target_compile_definitions(codepad
			PRIVATE
				CP_USE_GTK)
		target_link_libraries(codepad
			PRIVATE
				Gtk3::Gtk)
	else()
		target_sources(codepad
			PRIVATE
				codepad/os/linux/x11/linux.cpp
				codepad/os/linux/x11/misc.h
				codepad/os/linux/x11/renderer.h
				codepad/os/linux/x11/window.h)
		target_link_libraries(codepad
			PRIVATE
				${X11_LIBRARIES})
	endif()
else()
	message(FATAL_ERROR "unrecognized platform")
endif()

if(USE_SSE4)
	target_compile_definitions(codepad
		PRIVATE
			CP_USE_SSE4)
	if(CMAKE_COMPILER_IS_GNUCXX)
		target_compile_options(codepad
			PRIVATE
				-msse4.1)
	endif()
endif()

if(CMAKE_COMPILER_IS_GNUCXX) # TODO rid of this after filesystem support is complete
	target_link_libraries(codepad
		PRIVATE
			stdc++fs)
endif()
