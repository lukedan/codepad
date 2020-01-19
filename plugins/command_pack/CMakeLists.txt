cmake_minimum_required(VERSION 3.8)
project(command_pack)

add_library(command_pack MODULE)

set(SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/src")
target_sources(command_pack
	PRIVATE
		"${SOURCE_PATH}/main.cpp")
target_include_directories(command_pack
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/../common/include"
		"${APIGEN_OUT_DIR}")
target_compile_definitions(command_pack
	PRIVATE
		PLUGIN_API_STRUCT=${API_STRUCT_NAME}
		"PLUGIN_API_HEADER_H=<${API_HEADER_NAME}>"
		"PLUGIN_SIZES_ALIGNMENTS_H=<${SIZES_ALIGNMENTS_PATH}>")
add_dependencies(command_pack codepad_api_header)
