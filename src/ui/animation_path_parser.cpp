// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/animation_path_parser.h"

/// \file
/// Implementation of the animation path parser.

namespace codepad::ui::animation_path::parser {
	namespace components {
		result parse_string(std::u8string_view::const_iterator &it, std::u8string_view::const_iterator end) {
			std::size_t nchars = 0;
			while (it != end) {
				if (*it != '_' && !(*it >= 'a' && *it <= 'z') && !(*it >= 'A' && *it <= 'Z')) {
					break;
				}
				++nchars;
				++it;
			}
			return nchars > 0 ? result::completed : result::not_found;
		}

		result parse_index(
			std::u8string_view::const_iterator &it, std::u8string_view::const_iterator end, std::size_t &v
		) {
			if (it == end || *it != '[') {
				return result::not_found;
			}
			++it;
			if (it == end || !(*it >= '0' && *it <= '9')) {
				return result::error;
			}
			v = *(it++) - '0';
			while (it != end) {
				if (*it == ']') {
					++it;
					return result::completed;
				}
				if (!(*it >= '0' && *it <= '9')) {
					return result::error;
				}
				v = v * 10 + (*(it++) - '0');
			}
			return result::error; // no closing bracket
		}


		result parse_typed_component(
			std::u8string_view::const_iterator &it, std::u8string_view::const_iterator end, component &v
		) {
			if (it == end || *it != '(') {
				return result::not_found;
			}
			// type
			auto beg = ++it;
			if (parse_string(it, end) != result::completed) {
				return result::error;
			}
			v.type = std::u8string(&*beg, it - beg);
			// dot
			if (it == end || *it != '.') {
				return result::error;
			}
			// property
			beg = ++it;
			if (parse_string(it, end) != result::completed) {
				return result::error;
			}
			v.property = std::u8string(&*beg, it - beg);
			// index & closing parenthesis
			bool closed = false;
			if (it != end && *it == ')') {
				++it;
				closed = true;
			}
			std::size_t id = 0;
			result res = parse_index(it, end, id);
			if (res == result::error) {
				return result::error;
			}
			if (res == result::completed) {
				v.index.emplace(id);
			}
			if (it != end && *it == ')') {
				if (closed) {
					return result::error;
				}
				++it;
				closed = true;
			}
			return closed ? result::completed : result::error;
		}

		result parse_untyped_component(
			std::u8string_view::const_iterator &it, std::u8string_view::const_iterator end, component &v
		) {
			auto beg = it;
			if (parse_string(it, end) != result::completed) {
				return result::not_found;
			}
			v.property = std::u8string_view(&*beg, it - beg);
			std::size_t id;
			result res = parse_index(it, end, id);
			if (res == result::error) {
				return result::error;
			}
			if (res == result::completed) {
				v.index.emplace(id);
			}
			return result::completed;
		}

		result parse_component(
			std::u8string_view::const_iterator &it, std::u8string_view::const_iterator end, component &v
		) {
			result res = parse_typed_component(it, end, v);
			if (res != result::not_found) {
				return res;
			}
			return parse_untyped_component(it, end, v);
		}
	}


	result parse(std::u8string_view path, component_list &list) {
		if (path.empty()) {
			return result::not_found;
		}
		auto it = path.begin();
		list.emplace_back();
		result res = components::parse_component(it, path.end(), list.back());
		if (res != result::completed) {
			return result::error;
		}
		while (it != path.end()) {
			if (*(it++) != '.') {
				return result::error; // technically completed, but it's a single path
			}
			list.emplace_back();
			res = components::parse_component(it, path.end(), list.back());
			if (res != result::completed) {
				return result::error;
			}
		}
		return result::completed;
	}
}
