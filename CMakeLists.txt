cmake_minimum_required(VERSION 3.7)
project(codepad)

set(CMAKE_CXX_STANDARD 14)

if(WIN32)
	set(PLATFORM_SOURCE
			codepad/os/windows/font.h
			codepad/os/windows/misc.h
			codepad/os/windows/renderer.h
			codepad/os/windows/window.h
			codepad/os/windows/windows.cpp
			codepad/os/windows.h)
	set(PLATFORM_DEFINITIONS -DCP_PLATFORM_WINDOWS)
	set(PLATFORM_LIBRARIES d2d1 dwrite gdi32 windowscodecs)
elseif(UNIX)
	set(PLATFORM_DEFINITIONS -DCP_PLATFORM_UNIX)
	# not supported right now
	message(FATAL_ERROR "unix-like platform not yet supported")
else()
	message(FATAL_ERROR "unrecognized platform")
endif()

set(SOURCE_FILES
		codepad/editors/code/components.h
		codepad/editors/code/context.h
		codepad/editors/code/editor.h
		codepad/editors/code/codebox.h
		codepad/editors/docking.h
		codepad/os/current.h
		codepad/os/misc.h
		codepad/os/font.h
		codepad/os/renderer.h
		codepad/os/window.h
		codepad/ui/commonelements.h
		codepad/ui/element.h
		codepad/ui/font_family.h
		codepad/ui/manager.h
		codepad/ui/panel.h
		codepad/ui/draw.h
		codepad/ui/visual.h
		codepad/ui/window_hotkey_manager.h
		codepad/utilities/event.h
		codepad/utilities/globals.cpp
		codepad/utilities/hotkey_registry.h
		codepad/utilities/misc.h
		codepad/utilities/tasks.h
		codepad/utilities/textconfig.h
		codepad/utilities/textproc.h
		codepad/main.cpp
		${PLATFORM_SOURCE})
add_definitions(-DUNICODE -D_UNICODE -DCP_LOG_STACKTRACE ${PLATFORM_DEFINITIONS})
add_executable(codepad ${SOURCE_FILES})
target_link_libraries(codepad opengl32 freetype ${PLATFORM_LIBRARIES})
