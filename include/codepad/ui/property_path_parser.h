// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Parser for property paths.

#include <string_view>

#include "property_path.h"

/// Parser for property paths.
namespace codepad::ui::property_path::parser {
	// type = name
	// property = name
	// index = '[' number ']'
	//
	// typed_property = type '.' property
	// typed_component = '(' typed_property ')' | '(' typed_property index ')' |
	//                   '(' typed_property ')' index
	// untyped_component = property | property index
	// component = typed_component | untyped_component
	//
	// path = component | path '.' component

	/// The result of parsing a part of the path.
	enum class result {
		completed, ///< Success.
		not_found, ///< The path does not match the format at all.
		error ///< The path matches partially but is not complete.
	};

	/// Functions used to parse each individual component of the path.
	namespace components {
		/// Parses a string that contains only a to z, 0 to 9, or underscores.
		result parse_string(std::u8string_view::const_iterator &it, std::u8string_view::const_iterator end);
		/// Parses an index.
		result parse_index(
			std::u8string_view::const_iterator &it, std::u8string_view::const_iterator end, std::size_t&
		);

		/// Parses a typed component.
		result parse_typed_component(
			std::u8string_view::const_iterator &it, std::u8string_view::const_iterator end, component&
		);

		/// Parses a typed component.
		result parse_untyped_component(
			std::u8string_view::const_iterator &it, std::u8string_view::const_iterator end, component&
		);

		/// Parses a component.
		result parse_component(
			std::u8string_view::const_iterator &it, std::u8string_view::const_iterator end, component&
		);
	}

	/// Splits an animation target path into components. See the comment in <tt>namespace parsing</tt>.
	result parse(std::u8string_view path, component_list &list);
}
