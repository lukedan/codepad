add_library(python_plugin_host_pybind11 SHARED)

target_sources(python_plugin_host_pybind11
	PRIVATE "src/main.cpp" "src/core.cpp" "src/ui.cpp")
target_link_libraries(python_plugin_host_pybind11 PRIVATE codepad_core)
target_include_directories(python_plugin_host_pybind11 PRIVATE "../common/include")
set_target_properties(python_plugin_host_pybind11
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)
target_link_libraries(python_plugin_host_pybind11 PRIVATE pybind11::embed)
