cmake_minimum_required(VERSION 3.8)
project(codepad)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/modules)

if(NOT DEFINED USE_SSE4)
	set(USE_SSE4 Yes)
endif()

find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED)
if(UNIX)
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
		codepad/core/bst.h
		codepad/core/encodings.h
		codepad/core/event.h
		codepad/core/globals.cpp
		codepad/core/json.h
		codepad/core/misc.h

		codepad/editors/code/caret_set.cpp
		codepad/editors/code/caret_set.h
		codepad/editors/code/codebox.cpp
		codepad/editors/code/codebox.h
		codepad/editors/code/components.h
		codepad/editors/code/document_formatting_cache.h
		codepad/editors/code/editor.cpp
		codepad/editors/code/editor.h
		codepad/editors/code/interpretation.h
		codepad/editors/code/linebreak_registry.h
		codepad/editors/code/rendering.h
		codepad/editors/code/theme.h
		codepad/editors/code/view.h
		codepad/editors/buffer.cpp
		codepad/editors/buffer.h
		codepad/editors/buffer_manager.h
		codepad/editors/tabs.cpp
		codepad/editors/tabs.h

		codepad/os/current/all.h
		codepad/os/current/font.h
		codepad/os/current/misc.h
		codepad/os/current/renderer.h
		codepad/os/current/window.h
		codepad/os/filesystem.h
		codepad/os/freetype_font_base.h
		codepad/os/misc.h
		codepad/os/opengl_renderer_base.cpp
		codepad/os/opengl_renderer_base.h
		codepad/os/software_renderer_base.cpp
		codepad/os/software_renderer_base.h

		codepad/ui/arrangements.cpp
		codepad/ui/arrangements.h
		codepad/ui/atlas.h
		codepad/ui/commands.h
		codepad/ui/common_elements.cpp
		codepad/ui/common_elements.h
		codepad/ui/config_parsers.h
		codepad/ui/draw.h
		codepad/ui/element.cpp
		codepad/ui/element.h
		codepad/ui/element_classes.h
		codepad/ui/font.h
		codepad/ui/font_family.h
		codepad/ui/hotkey_registry.h
		codepad/ui/manager.cpp
		codepad/ui/manager.h
		codepad/ui/misc.h
		codepad/ui/native_commands.cpp
		codepad/ui/native_commands.h
		codepad/ui/panel.cpp
		codepad/ui/panel.h
		codepad/ui/renderer.h
		codepad/ui/scheduler.h
		codepad/ui/text_rendering.cpp
		codepad/ui/text_rendering.h
		codepad/ui/visual.cpp
		codepad/ui/visual.h
		codepad/ui/window.cpp
		codepad/ui/window.h

		codepad/main.cpp)
target_compile_definitions(codepad
	PRIVATE
		UNICODE _UNICODE CP_LOG_STACKTRACE)
target_link_libraries(codepad
	PRIVATE
		Freetype::Freetype OpenGL::GL)

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
			codepad/os/windows/filesystem.cpp
			codepad/os/windows/font.h
			codepad/os/windows/misc.h
			codepad/os/windows/renderer.h
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
