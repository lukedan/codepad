// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include <type_traits>

#include PLUGIN_API_HEADER_H
#include PLUGIN_SIZES_ALIGNMENTS_H
#include "plugin_defs.h"

#include <Windows.h>

const PLUGIN_API_STRUCT *codepad = nullptr; ///< The API interface.

extern "C" {
	PLUGIN_INITIALIZE(api) {
		codepad = api;
	}
	PLUGIN_GET_NAME() {
		return "command_pack";
	}
	PLUGIN_ENABLE() {
		codepad_ui_manager *man = codepad->codepad_get_manager();
		codepad_ui_command_registry *reg = codepad->codepad_ui_manager_get_command_registry_void(man);

		// create string
		std::aligned_storage_t<
			std_basic_string_char_std_char_traits_char_std_allocator_char_size,
			std_basic_string_char_std_char_traits_char_std_allocator_char_align
		> str_storage;
		auto *str = reinterpret_cast<std_basic_string_char_std_char_traits_char_std_allocator_char*>(&str_storage);
		codepad->codepad_std_string_from_c_string("test_command", str);

		// create function
		std::aligned_storage_t<
			std_function_func_void_pcodepad_ui_element_size,
			std_function_func_void_pcodepad_ui_element_align
		> func_storage;
		auto *func = reinterpret_cast<std_function_func_void_pcodepad_ui_element*>(&func_storage);
		codepad->std_function_func_void_pcodepad_ui_element_from_raw(
			[](codepad_ui_element*, void*) {
				MessageBox(nullptr, TEXT("hello plugin!"), TEXT("Test"), MB_OK);
			},
			func, nullptr
		);

		codepad->codepad_ui_command_registry_register_command(reg, str, func);

		// call destructors
		codepad->std_basic_string_char_std_char_traits_char_std_allocator_char_dtor(str);
		codepad->std_function_func_void_pcodepad_ui_element_dtor(func);
	}
	PLUGIN_DISABLE() {
	}
}
