cmake_minimum_required(VERSION 3.7)
project(codepad)

set(CMAKE_CXX_STANDARD 14)

set(SOURCE_FILES
	codepad/editors/code/components.h
	codepad/editors/code/context.h
	codepad/editors/code/editor.h
	codepad/editors/code/codebox.h
	codepad/editors/docking.h
	codepad/os/current.h
	codepad/os/input.h
	codepad/os/renderer.h
	codepad/os/window.h
	codepad/os/windows.cpp
	codepad/os/windows.h
	codepad/ui/commonelements.h
	codepad/ui/element.h
	codepad/ui/font.h
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
	codepad/main.cpp)

add_definitions(-DUNICODE -D_UNICODE -DCP_LOG_STACKTRACE)

if(WIN32)
	add_definitions(-DCP_PLATFORM_WINDOWS)
elseif(UNIX)
	add_definitions(-DCP_PLATFORM_UNIX)
endif()

add_executable(codepad ${SOURCE_FILES})
target_link_libraries(codepad d2d1 dwrite opengl32 gdi32 freetype)
