cmake_minimum_required(VERSION 3.7)
project(codepad)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/modules)
set(CMAKE_CXX_STANDARD 17)

if(MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
elseif(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()

find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED)
include_directories(${FREETYPE_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR})

if(WIN32)
	set(PLATFORM_SOURCE
		codepad/os/windows/filesystem.cpp
		codepad/os/windows/font.h
		codepad/os/windows/misc.h
		codepad/os/windows/renderer.h
		codepad/os/windows/window.h
		codepad/os/windows/windows.cpp
		codepad/os/windows.h)
	set(PLATFORM_DEFINITIONS -DCP_PLATFORM_WINDOWS -DNOMINMAX -DOEMRESOURCE)
	set(PLATFORM_LIBRARIES d2d1 dwrite gdi32 windowscodecs imm32)
	if(CMAKE_COMPILER_IS_GNUCXX)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,-mbig-obj")
	endif()
elseif(UNIX)
	set(USE_GTK "Yes")

	set(PLATFORM_SOURCE
		codepad/os/linux/filesystem.cpp
		codepad/os/linux/font.h
		codepad/os/linux/misc.h
		codepad/os/linux/renderer.h
		codepad/os/linux/window.h
		codepad/os/linux.h)
	find_package(Fontconfig REQUIRED)
	include_directories(${FONTCONFIG_INCLUDE_DIRS})
	set(PLATFORM_DEFINITIONS -DCP_PLATFORM_UNIX)
	set(PLATFORM_LIBRARIES ${FONTCONFIG_LIBRARIES} pthread)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic")
	if(USE_GTK)
		find_package(GTK3 REQUIRED)
		include_directories(${GTK3_INCLUDE_DIRS})
		set(PLATFORM_DEFINITIONS ${PLATFORM_DEFINITIONS} -DCP_USE_GDK)
		set(PLATFORM_LIBRARIES ${PLATFORM_LIBRARIES} ${GTK3_LIBRARIES})
		set(PLATFORM_SOURCE ${PLATFORM_SOURCE}
			codepad/os/linux/gtk/linux.cpp
			codepad/os/linux/gtk/misc.h
			codepad/os/linux/gtk/renderer.h
			codepad/os/linux/gtk/window.h)
	else()
		find_package(X11 REQUIRED)
		include_directories(${X11_INCLUDE_DIR})
		set(PLATFORM_LIBRARIES ${PLATFORM_LIBRARIES} ${X11_LIBRARIES})
		set(PLATFORM_SOURCE ${PLATFORM_SOURCE}
			codepad/os/linux/x11/linux.cpp
			codepad/os/linux/x11/misc.h
			codepad/os/linux/x11/renderer.h
			codepad/os/linux/x11/window.h)
	endif()
else()
	message(FATAL_ERROR "unrecognized platform")
endif()

set(SOURCE_FILES
	codepad/core/bst.h
	codepad/core/encodings.h
	codepad/core/event.h
	codepad/core/globals.cpp
	codepad/core/hotkey_registry.h
	codepad/core/misc.h
	codepad/core/tasks.h
	codepad/editors/code/buffer.h
	codepad/editors/code/codebox.cpp
	codepad/editors/code/codebox.h
	codepad/editors/code/components.h
	codepad/editors/code/document.cpp
	codepad/editors/code/document.h
	codepad/editors/code/document_manager.h
	codepad/editors/code/editor.cpp
	codepad/editors/code/editor.h
	codepad/editors/code/rendering.h
	codepad/editors/code/view.h
	codepad/editors/tabs.cpp
	codepad/editors/tabs.h
	codepad/os/current/all.h
	codepad/os/current/font.h
	codepad/os/current/misc.h
	codepad/os/current/renderer.h
	codepad/os/current/window.h
	codepad/os/current.h
	codepad/os/filesystem.h
	codepad/os/font.h
	codepad/os/misc.h
	codepad/os/opengl_renderer_base.cpp
	codepad/os/opengl_renderer_base.h
	codepad/os/renderer.h
	codepad/os/software_renderer_base.cpp
	codepad/os/software_renderer_base.h
	codepad/os/window.cpp
	codepad/os/window.h
	codepad/ui/commands.h
	codepad/ui/common_elements.h
	codepad/ui/common_elements.cpp
	codepad/ui/draw.h
	codepad/ui/element.cpp
	codepad/ui/element.h
	codepad/ui/element_classes.h
	codepad/ui/font_family.h
	codepad/ui/manager.cpp
	codepad/ui/manager.h
	codepad/ui/native_commands.cpp
	codepad/ui/native_commands.h
	codepad/ui/panel.cpp
	codepad/ui/panel.h
	codepad/ui/visual.cpp
	codepad/ui/visual.h
	codepad/main.cpp
	${PLATFORM_SOURCE})

if(NOT DEFINED USE_SSE4)
	set(USE_SSE4 Yes)
endif()
if(USE_SSE4)
	add_definitions(-DCP_USE_SSE4)
	if(CMAKE_COMPILER_IS_GNUCXX)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.1")
	endif()
endif()

add_definitions(-DUNICODE -D_UNICODE -DCP_LOG_STACKTRACE ${PLATFORM_DEFINITIONS})
add_executable(codepad ${SOURCE_FILES})
target_link_libraries(codepad ${OPENGL_gl_LIBRARY} ${FREETYPE_LIBRARIES} ${PLATFORM_LIBRARIES})
if(CMAKE_COMPILER_IS_GNUCXX) # TODO rid of this after filesystem support is complete
	target_link_libraries(codepad stdc++fs)
endif()
