// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementations of all JSON parsers in \ref codepad.

#include "json/misc.h"
#include "color.h"

namespace codepad::json {
	template <
		typename Rep, typename Period
	> template <typename Value> std::optional<std::chrono::duration<Rep, Period>> default_parser<
		std::chrono::duration<Rep, Period>
	>::operator()(const Value &val) const {
		if (auto secs = val.template cast<double>()) {
			return std::chrono::duration_cast<std::chrono::duration<Rep, Period>>(
				std::chrono::duration<double>(secs.value())
			);
		}
		return std::nullopt;
	}


	template <typename Value> std::optional<vec2d> default_parser<vec2d>::operator()(const Value &val) const {
		std::optional<double> x, y;
		if (auto arr = val.template try_cast<typename Value::array_type>()) {
			if (arr->size() >= 2) {
				if (arr->size() > 2) {
					val.template log<log_level::warning>(CP_HERE) << u8"too many elements in vec2";
				}
				x = arr->at(0).template parse<double>();
				y = arr->at(1).template parse<double>();
			} else {
				val.template log<log_level::error>(CP_HERE) << u8"too few elements in vec2";
			}
		} else if (auto obj = val.template try_cast<typename Value::object_type>()) {
			if (obj->size() > 2) {
				val.template log<log_level::warning>(CP_HERE) << u8"redundant fields in vec2 definition";
			}
			x = obj->template parse_member<double>(u8"x");
			y = obj->template parse_member<double>(u8"y");
		} else {
			val.template log<log_level::error>(CP_HERE) << u8"invalid vec2 format";
		}
		if (x && y) {
			return vec2d(x.value(), y.value());
		}
		return std::nullopt;
	}


	template <typename Value> std::optional<colord> default_parser<colord>::operator()(const Value &val) const {
		if (auto arr = val.template cast<typename Value::array_type>()) { // must be an array
			if (arr->size() >= 3) {
				colord result;
				if (arr->size() > 3) {
					if (auto format = arr->at(0).template try_cast<std::u8string_view>()) {
						if (format.value() == u8"hsl") {
							auto
								h = arr->at(1).template cast<double>(),
								s = arr->at(2).template cast<double>(),
								l = arr->at(3).template cast<double>();
							result = colord::from_hsl(h.value(), s.value(), l.value());
							if (arr->size() > 4) {
								result.a = arr->at(3).template cast<double>().value_or(1.0);
								if (arr->size() > 5) {
									val.template log<log_level::error>(CP_HERE) <<
										"redundant fields in color definition";
								}
							}
							return result;
						}
					} else {
						result.a = arr->at(3).template cast<double>().value_or(1.0);
					}
					if (arr->size() > 4) {
						val.template log<log_level::error>(CP_HERE) <<
							"redundant fields in color definition";
					}
				}
				result.r = arr->at(0).template cast<double>().value_or(0.0);
				result.g = arr->at(1).template cast<double>().value_or(0.0);
				result.b = arr->at(2).template cast<double>().value_or(0.0);
				return result;
			} else {
				val.template log<log_level::error>(CP_HERE) << "too few elements in color definition";
			}
		}
		return std::nullopt;
	}
}
