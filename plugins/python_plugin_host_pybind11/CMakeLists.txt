add_library(python_plugin_host_pybind11 MODULE)

target_sources(python_plugin_host_pybind11
	PRIVATE "src/main.cpp" "src/modules.cpp")
target_link_libraries(python_plugin_host_pybind11 PRIVATE codepad)
target_include_directories(python_plugin_host_pybind11 PRIVATE "../common/include")
set_target_properties(python_plugin_host_pybind11
	PROPERTIES
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

find_package(pybind11 CONFIG REQUIRED)
target_link_libraries(python_plugin_host_pybind11
	PRIVATE pybind11::embed pybind11::module pybind11::pybind11)
