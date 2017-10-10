cmake_minimum_required(VERSION 3.7)
project(codepad)

set(CMAKE_CXX_STANDARD 14)

if(MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -g")
endif()

find_package(Freetype REQUIRED)
include_directories(${FREETYPE_INCLUDE_DIRS})
find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

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
	find_package(X11 REQUIRED)
	include_directories(${X11_INCLUDE_DIR})
	set(PLATFORM_SOURCE
			codepad/os/linux/font.h
			codepad/os/linux/misc.h
			codepad/os/linux/window.h
			codepad/os/linux/renderer.h
			codepad/os/linux/linux.cpp
			codepad/os/linux.h)
	set(PLATFORM_DEFINITIONS -DCP_PLATFORM_UNIX)
	set(PLATFORM_LIBRARIES ${X11_LIBRARIES} pthread fontconfig)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic")
else()
	message(FATAL_ERROR "unrecognized platform")
endif()

set(SOURCE_FILES
		codepad/editors/code/buffer.h
		codepad/editors/code/codebox.h
		codepad/editors/code/components.h
		codepad/editors/code/context.h
		codepad/editors/code/editor.h
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
		codepad/ui/theme_providers.h
		codepad/ui/window_hotkey_manager.h
		codepad/utilities/bst.h
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
target_link_libraries(codepad ${OPENGL_gl_LIBRARY} ${FREETYPE_LIBRARIES} ${PLATFORM_LIBRARIES})
