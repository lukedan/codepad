// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementations of all JSON parsers in \ref codepad::ui.

#include "../core/json_parsers.h"
#include "element_parameters.h"
#include "hotkey_registry.h"

namespace codepad::json {
	template <typename Value> std::optional<ui::relative_vec2d> default_parser<ui::relative_vec2d>::operator()(
		const Value &val
	) const {
		if (auto arr = val.template try_cast<typename Value::array_type>()) {
			if (arr->size() >= 2) {
				if (arr->size() > 2) {
					val.template log<log_level::warning>(CP_HERE) <<
						"redundant members in relative vec2d definition";
				}
				if (auto x = arr->at(0).template try_cast<double>()) { // only absolute component
					if (auto y = arr->at(1).template cast<double>()) {
						return ui::relative_vec2d(vec2d(), vec2d(x.value(), y.value()));
					}
				} else if (auto rel_vec = arr->at(0).template try_parse<vec2d>()) { // array representation
					if (auto abs_vec = arr->at(1).template parse<vec2d>()) {
						return ui::relative_vec2d(rel_vec.value(), abs_vec.value());
					}
				} else {
					val.template log<log_level::error>(CP_HERE) << "invalid relative vec2d component format";
				}
			} else {
				val.template log<log_level::error>(CP_HERE) << "not enough entries in relative vec2d definition";
			}
		} else if (auto full = val.template try_cast<typename Value::object_type>()) { // full representation
			if (auto abs = full->template parse_member<vec2d>(u8"absolute")) {
				if (auto rel = full->template parse_member<vec2d>(u8"relative")) {
					return ui::relative_vec2d(rel.value(), abs.value());
				}
			}
		} else {
			val.template log<log_level::error>(CP_HERE) << "invalid relative vec2d format";
		}
		return std::nullopt;
	}


	template <typename Value> std::optional<ui::relative_double> default_parser<ui::relative_double>::operator()(
		const Value &val
	) const {
		if (auto full = val.template try_cast<typename Value::object_type>()) { // full representation
			if (auto abs = full->template parse_member<double>(u8"absolute")) {
				if (auto rel = full->template parse_member<double>(u8"relative")) {
					if (full->size() > 2) {
						val.template log<log_level::warning>(CP_HERE) << "redundant fields in relative double";
					}
					return ui::relative_double(rel.value(), abs.value());
				}
			}
		} else if (auto arr = val.template try_cast<typename Value::array_type>()) { // a list of two doubles
			if (arr->size() >= 2) {
				if (arr->size() > 2) {
					val.template log<log_level::warning>(CP_HERE) << "redundant elements in relative double";
				}
				auto
					rel = arr->at(0).template cast<double>(),
					abs = arr->at(1).template cast<double>();
				if (rel && abs) {
					return ui::relative_double(rel.value(), abs.value());
				}
			} else {
				val.template log<log_level::error>(CP_HERE) << "too few elements in relative double";
			}
		} else if (auto abs = val.template try_cast<double>()) { // absolute only
			return ui::relative_double(0.0, abs.value());
		} else {
			val.template log<log_level::error>(CP_HERE) << "invalid relative double format";
		}
		return std::nullopt;
	}


	template <
		typename Value
	> std::optional<ui::transforms::generic> default_parser<ui::transforms::generic>::operator()(
		const Value &val
	) const {
		if (val.template is<null_t>()) {
			return ui::transforms::generic::make<ui::transforms::identity>();
		}
		std::optional<typename Value::array_type> group;
		if (auto obj = val.template try_cast<typename Value::object_type>()) {
			if (auto offset = obj->template parse_optional_member<ui::relative_vec2d>(u8"translation")) {
				return ui::transforms::generic::make<ui::transforms::translation>(offset.value());
			}
			if (auto scale = obj->template parse_optional_member<vec2d>(u8"scale")) {
				if (auto center = obj->template parse_member<ui::relative_vec2d>(u8"center")) {
					return ui::transforms::generic::make<ui::transforms::scale>(center.value(), scale.value());
				}
			}
			if (auto rotation = obj->template parse_optional_member<double>(u8"rotation")) {
				if (auto center = obj->template parse_member<ui::relative_vec2d>(u8"center")) {
					return ui::transforms::generic::make<ui::transforms::rotation>(
						center.value(), rotation.value()
						);
				}
			}
			group = obj->template parse_optional_member<typename Value::array_type>(u8"children");
		} else { // try to parse transform collection
			group = val.template try_cast<typename Value::array_type>();
		}
		if (group) {
			ui::transforms::collection res;
			for (auto &&trans : group.value()) {
				if (auto child = trans.template parse<ui::transforms::generic>()) {
					res.components.emplace_back(std::move(child.value()));
				}
			}
			return ui::transforms::generic::make<ui::transforms::collection>(std::move(res));
		} else {
			val.template log<log_level::error>(CP_HERE) << "invalid transform format";
		}
		return std::nullopt;
	}


	template <
		typename Value
	> std::optional<ui::brushes::solid_color> default_parser<ui::brushes::solid_color>::operator()(
		const Value &val
	) const {
		if (auto obj = val.template cast<typename Value::object_type>()) {
			if (auto color = obj->template parse_member<colord>(u8"color")) {
				return ui::brushes::solid_color(color.value());
			}
		}
		return std::nullopt;
	}


	template <
		typename Value
	> std::optional<ui::brushes::linear_gradient> default_parser<ui::brushes::linear_gradient>::operator()(
		const Value &val
	) const {
		if (auto obj = val.template cast<typename Value::object_type>()) {
			if (auto from = obj->template parse_member<ui::relative_vec2d>(u8"from")) {
				if (auto to = obj->template parse_member<ui::relative_vec2d>(u8"to")) {
					if (auto stops = obj->template parse_member<ui::gradient_stop_collection>(
						u8"gradient_stops", array_parser<ui::gradient_stop>()
						)) {
						return ui::brushes::linear_gradient(from.value(), to.value(), std::move(stops.value()));
					}
				}
			}
		}
		return std::nullopt;
	}


	template <
		typename Value
	> std::optional<ui::brushes::radial_gradient> default_parser<ui::brushes::radial_gradient>::operator()(
		const Value &val
	) const {
		if (auto obj = val.template cast<typename Value::object_type>()) {
			if (auto center = obj->template parse_member<ui::relative_vec2d>(u8"center")) {
				if (auto radius = obj->template parse_member<double>(u8"radius")) {
					if (auto stops = obj->template parse_member<ui::gradient_stop_collection>(
						u8"gradient_stops", array_parser<ui::gradient_stop>()
						)) {
						return ui::brushes::radial_gradient(
							center.value(), radius.value(), std::move(stops.value())
						);
					}
				}
			}
		}
		return std::nullopt;
	}


	template <
		typename Value
	> std::optional<ui::geometries::rectangle> default_parser<ui::geometries::rectangle>::operator()(
		const Value &val
	) const {
		if (auto obj = val.template cast<typename Value::object_type>()) {
			if (auto top_left = obj->template parse_member<ui::relative_vec2d>(u8"top_left")) {
				if (auto bottom_right = obj->template parse_member<ui::relative_vec2d>(u8"bottom_right")) {
					return ui::geometries::rectangle(top_left.value(), bottom_right.value());
				}
			}
		}
		return std::nullopt;
	}


	template <
		typename Value
	> std::optional<ui::geometries::rounded_rectangle> default_parser<ui::geometries::rounded_rectangle>::operator()(
		const Value &val
	) const {
		if (auto obj = val.template cast<typename Value::object_type>()) {
			if (auto top_left = obj->template parse_member<ui::relative_vec2d>(u8"top_left")) {
				if (auto bottom_right = obj->template parse_member<ui::relative_vec2d>(u8"bottom_right")) {
					if (auto rx = obj->template parse_member<ui::relative_double>(u8"radiusx")) {
						if (auto ry = obj->template parse_member<ui::relative_double>(u8"radiusy")) {
							return ui::geometries::rounded_rectangle(
								top_left.value(), bottom_right.value(), rx.value(), ry.value()
							);
						}
					}
				}
			}
		}
		return std::nullopt;
	}


	template <
		typename Value
	> std::optional<ui::geometries::ellipse> default_parser<ui::geometries::ellipse>::operator()(
		const Value &val
	) const {
		if (auto obj = val.template cast<typename Value::object_type>()) {
			if (auto top_left = obj->template parse_member<ui::relative_vec2d>(u8"top_left")) {
				if (auto bottom_right = obj->template parse_member<ui::relative_vec2d>(u8"bottom_right")) {
					return ui::geometries::ellipse(top_left.value(), bottom_right.value());
				}
			}
		}
		return std::nullopt;
	}


	template <
		typename Value
	> std::optional<ui::geometries::path::part> default_parser<ui::geometries::path::part>::operator()(
		const Value &val
	) const {
		if (auto obj = val.template cast<typename Value::object_type>()) {
			if (obj->size() > 0) {
				if (obj->size() > 1) {
					val.template log<log_level::warning>(CP_HERE) << "too many fields in subpath part";
				}
				auto member = obj->member_begin();
				if (member.name() == u8"line_to") {
					if (auto to = member.value().template parse<ui::relative_vec2d>()) {
						ui::geometries::path::part res;
						res.value.emplace<ui::geometries::path::segment>().to = to.value();
						return res;
					}
				} else if (member.name() == u8"arc") {
					if (auto part_obj = member.value().template cast<typename Value::object_type>()) {
						if (auto to = part_obj->template parse_member<ui::relative_vec2d>(u8"to")) {
							if (auto it = part_obj->find_member(u8"radius"); it != part_obj->member_end()) {
								ui::geometries::path::part res;
								auto &arc = res.value.emplace<ui::geometries::path::arc>();
								if (auto r = it.value().template try_cast<double>()) {
									arc.radius.absolute = vec2d(r.value(), r.value());
								} else if (auto rval = it.value().template parse<ui::relative_vec2d>()) {
									arc.radius = rval.value();
								} else {
									it.value().template log<log_level::error>(CP_HERE) <<
										"invalid radius format";
									return std::nullopt;
								}
								arc.direction =
									part_obj->template parse_member<bool>(u8"clockwise").value_or(false) ?
									ui::sweep_direction::clockwise : ui::sweep_direction::counter_clockwise;
								arc.type =
									part_obj->template parse_member<bool>(u8"major").value_or(false) ?
									ui::arc_type::major : ui::arc_type::minor;
								if (
									auto rot = part_obj->template parse_optional_member<double>(u8"rotation")
									) {
									arc.rotation = rot.value();
								}
								return res;
							}
						}
					}
				} else if (member.name() == u8"bezier") {
					if (auto part_obj = member.value().template cast<typename Value::object_type>()) {
						if (auto to = part_obj->template parse_member<ui::relative_vec2d>(u8"to")) {
							if (auto c1 = part_obj->template parse_member<ui::relative_vec2d>(
								u8"control1"
								)) {
								if (auto c2 = part_obj->template parse_member<ui::relative_vec2d>(
									u8"control2"
									)) {
									ui::geometries::path::part res;
									auto &bezier = res.value.emplace<ui::geometries::path::cubic_bezier>();
									bezier.to = to.value();
									bezier.control1 = c1.value();
									bezier.control2 = c2.value();
									return res;
								}
							}
						}
					}
				} else {
					val.template log<log_level::error>(CP_HERE) << "invalid subpath part type";
				}
			} else {
				val.template log<log_level::error>(CP_HERE) << "empty subpath part";
			}
		}
		return std::nullopt;
	}


	template <
		typename Value
	> std::optional<ui::geometries::path::subpath> default_parser<ui::geometries::path::subpath>::operator()(
		const Value &val
	) const {
		if (auto obj = val.template try_cast<typename Value::object_type>()) {
			if (auto start = obj->template parse_member<ui::relative_vec2d>(u8"start")) {
				if (auto parts = obj->template parse_member<std::vector<ui::geometries::path::part>>(
					u8"parts", array_parser<ui::geometries::path::part>()
					)) {
					if (auto closed = obj->template parse_member<bool>(u8"closed")) {
						ui::geometries::path::subpath res;
						res.starting_point = start.value();
						res.parts = std::move(parts.value());
						res.closed = closed.value();
						return res;
					}
				}
			}
		} else if (auto arr = val.template try_cast<typename Value::array_type>()) {
			if (arr->size() > 2) {
				auto beg = arr->begin(), end = arr->end() - 1;
				if (auto start = beg->template parse<ui::relative_vec2d>()) {
					if (auto final_obj = end->template cast<typename Value::object_type>()) {
						if (auto closed = final_obj->template parse_member<bool>(u8"closed")) {
							ui::geometries::path::subpath res;
							res.starting_point = start.value();
							res.closed = closed.value();
							for (++beg; beg != end; ++beg) {
								if (auto part = beg->template parse<ui::geometries::path::part>()) {
									res.parts.emplace_back(part.value());
								}
							}
							return res;
						}
					}
				}
			} else {
				val.template log<log_level::error>(CP_HERE) << "too few elements in subpath";
			}
		} else {
			val.template log<log_level::error>(CP_HERE) << "invalid subpath format";
		}
		return std::nullopt;
	}


	template <typename Value> std::optional<ui::geometries::path> default_parser<ui::geometries::path>::operator()(
		const Value &val
	) const {
		if (auto obj = val.template cast<typename Value::object_type>()) {
			if (auto subpaths = obj->template parse_member<std::vector<ui::geometries::path::subpath>>(
				u8"subpaths", array_parser<ui::geometries::path::subpath>()
				)) {
				ui::geometries::path result;
				result.subpaths = std::move(subpaths.value());
				return result;
			}
		}
		return std::nullopt;
	}


	template <typename ValueType> std::optional<ui::element_layout> default_parser<ui::element_layout>::operator()(
		const ValueType &val
	) const {
		if (auto obj = val.template cast<typename ValueType::object_type>()) {
			ui::element_layout result;
			result.margin =
				obj->template parse_optional_member<ui::thickness>(u8"margin").value_or(result.margin);
			result.padding =
				obj->template parse_optional_member<ui::thickness>(u8"padding").value_or(result.padding);
			result.elem_anchor =
				obj->template parse_optional_member<ui::anchor>(u8"anchor").value_or(result.elem_anchor);
			// parse size
			result.width_alloc = obj->template parse_optional_member<ui::size_allocation_type>(
				u8"width_alloc"
				).value_or(result.width_alloc);
			result.height_alloc = obj->template parse_optional_member<ui::size_allocation_type>(
				u8"height_alloc"
				).value_or(result.height_alloc);
			auto w = obj->find_member(u8"width"), h = obj->find_member(u8"height");
			if (w != obj->member_end() || h != obj->member_end()) { // parse both components
				if (w != obj->member_end()) {
					_parse_size_component(w.value(), result.size.x, result.width_alloc);
				}
				if (h != obj->member_end()) {
					_parse_size_component(h.value(), result.size.y, result.height_alloc);
				}
			} else { // parse a single size
				result.size = obj->template parse_optional_member<vec2d>(u8"size").value_or(result.size);
			}
			return result;
		}
		return std::nullopt;
	}

	template <typename ValueType> void default_parser<ui::element_layout>::_parse_size_component(
		const ValueType &val, double &v, ui::size_allocation_type &ty
	) {
		if (auto str = val.template try_cast<std::u8string_view>()) {
			if (str.value() == u8"auto" || str.value() == u8"Auto") {
				v = 0.0;
				ty = ui::size_allocation_type::automatic;
				return;
			}
		}
		if (auto alloc = val.template parse<ui::size_allocation>()) {
			v = alloc->value;
			ty = alloc->is_pixels ? ui::size_allocation_type::fixed : ui::size_allocation_type::proportion;
			return;
		}
		val.template log<log_level::error>(CP_HERE) << "failed to parse size component";
	}


	template <typename Value> std::optional<ui::orientation> default_parser<ui::orientation>::operator()(
		const Value &obj
	) const {
		if (auto opt_str = obj.template cast<std::u8string_view>()) {
			std::u8string_view str = opt_str.value();
			if (str == u8"h" || str == u8"hori" || str == u8"horizontal") {
				return ui::orientation::horizontal;
			} else if (str == u8"v" || str == u8"vert" || str == u8"vertical") {
				return ui::orientation::vertical;
			} else {
				obj.template log<log_level::error>(CP_HERE) << "invalid orientation string";
			}
		}
		return std::nullopt;
	}


	template <typename Value> std::optional<ui::visibility> default_parser<ui::visibility>::operator()(
		const Value &val
	) const {
		if (val.template is<json::null_t>()) {
			return ui::visibility::none;
		} else if (auto str = val.template try_cast<std::u8string_view>()) {
			return get_bitset_from_string_with_negate<ui::visibility, ui::visibility::full>({
				{ u8'v', ui::visibility::visual },
				{ u8'i', ui::visibility::interact },
				{ u8'l', ui::visibility::layout },
				{ u8'f', ui::visibility::focus }
				}, str.value());
		} else {
			val.template log<log_level::error>(CP_HERE) << "invalid visibility format";
		}
		return std::nullopt;
	}


	template <typename Value> std::optional<ui::cursor> default_parser<ui::cursor>::operator()(
		const Value &val
	) const {
		if (auto str = val.template cast<std::u8string_view>()) {
			if (str.value() == u8"normal") {
				return ui::cursor::normal;
			} else if (str.value() == u8"busy") {
				return ui::cursor::busy;
			} else if (str.value() == u8"crosshair") {
				return ui::cursor::crosshair;
			} else if (str.value() == u8"hand") {
				return ui::cursor::hand;
			} else if (str.value() == u8"help") {
				return ui::cursor::help;
			} else if (str.value() == u8"text_beam") {
				return ui::cursor::text_beam;
			} else if (str.value() == u8"denied") {
				return ui::cursor::denied;
			} else if (str.value() == u8"arrow_all") {
				return ui::cursor::arrow_all;
			} else if (str.value() == u8"arrow_northeast_southwest") {
				return ui::cursor::arrow_northeast_southwest;
			} else if (str.value() == u8"arrow_north_south") {
				return ui::cursor::arrow_north_south;
			} else if (str.value() == u8"arrow_northwest_southeast") {
				return ui::cursor::arrow_northwest_southeast;
			} else if (str.value() == u8"arrow_east_west") {
				return ui::cursor::arrow_east_west;
			} else if (str.value() == u8"invisible") {
				return ui::cursor::invisible;
			}
		}
		return std::nullopt;
	}


	template <typename Value> std::optional<ui::thickness> default_parser<ui::thickness>::operator()(
		const Value &val
	) const {
		if (auto arr = val.template try_cast<typename Value::array_type>()) {
			if (arr->size() >= 4) {
				if (arr->size() > 4) {
					val.template log<log_level::error>(CP_HERE) <<
						"redundant elements in thickness definition";
				}
				auto
					l = arr->at(0).template cast<double>(),
					t = arr->at(1).template cast<double>(),
					r = arr->at(2).template cast<double>(),
					b = arr->at(3).template cast<double>();
				if (l && t && r && b) {
					return ui::thickness(l.value(), t.value(), r.value(), b.value());
				}
			} else {
				val.template log<log_level::error>(CP_HERE) << "too few elements in thickness";
			}
		} else if (auto v = val.template try_cast<double>()) {
			return ui::thickness(v.value());
		} else {
			val.template log<log_level::error>(CP_HERE) << "invalid thickness format";
		}
		return std::nullopt;
	}


	template <typename Value> std::optional<ui::size_allocation> default_parser<ui::size_allocation>::operator()(
		const Value &val
	) const {
		if (auto pixels = val.template try_cast<double>()) { // number in pixels
			return ui::size_allocation::pixels(pixels.value());
		} else if (auto str = val.template try_cast<std::u8string_view>()) {
			ui::size_allocation res(0.0, true); // in pixels if no suffix is present
			// the value is additionally divided by 100 if it's a percentage
			bool percentage = str->ends_with(u8"%");
			if (percentage || str->ends_with(u8"*")) { // proportion
				res.is_pixels = false;
			}
			// it's not safe to use std::strtod here since it requires that the input string be null-terminated
#ifdef __GNUC__
			{ // TODO libstdc++ doesn't implement from_chars
				std::u8string str_cstr(str.value());
				res.value = std::stod(reinterpret_cast<const char*>(str_cstr.c_str()));
			}
#else
			{
				const char *str_data = reinterpret_cast<const char*>(str->data());
				std::from_chars(str_data, str_data + str->size(), res.value); // result ignored
			}
#endif
			if (percentage) {
				res.value *= 0.01;
			}
			return res;
		} else if (auto full = val.template try_cast<typename Value::object_type>()) {
			// full object representation
			auto value = full->template parse_member<double>(u8"value");
			auto is_pixels = full->template parse_member<bool>(u8"is_pixels");
			if (value && is_pixels) {
				if (full->size() > 2) {
					full->template log<log_level::error>(CP_HERE) << "redundant fields in size allocation";
				}
				return ui::size_allocation(value.value(), is_pixels.value());
			}
		} else {
			val.template log<log_level::error>(CP_HERE) << "invalid size allocation format";
		}
		return std::nullopt;
	}


	template <typename Value> std::optional<ui::anchor> default_parser<ui::anchor>::operator()(
		const Value &obj
	) const {
		if (auto str = obj.template cast<std::u8string_view>()) {
			return get_bitset_from_string<ui::anchor>({
				{ u8'l', ui::anchor::left },
				{ u8't', ui::anchor::top },
				{ u8'r', ui::anchor::right },
				{ u8'b', ui::anchor::bottom }
				}, str.value());
		}
		return std::nullopt;
	}


	template <typename Value> std::optional<ui::font_weight> default_parser<ui::font_weight>::operator()(
		const Value &val
	) const {
		if (auto str = val.template cast<std::u8string_view>()) {
			if (str.value() == u8"normal") {
				return ui::font_weight::normal;
			}
		}
		return std::nullopt;
	}


	template <typename Value> std::optional<ui::font_stretch> default_parser<ui::font_stretch>::operator()(
		const Value &val
	) const {
		if (auto str = val.template cast<std::u8string_view>()) {
			if (str.value() == u8"normal") {
				return ui::font_stretch::normal;
			}
		}
		return std::nullopt;
	}


	template <typename Value> std::optional<ui::font_parameters> default_parser<ui::font_parameters>::operator()(
		const Value &val
	) const {
		if (auto full = val.template try_cast<typename Value::object_type>()) { // full representation
			ui::font_parameters params;
			if (auto family = full->template parse_optional_member<std::u8string>(u8"family")) {
				params.family = std::move(family.value());
			}
			if (auto size = full->template parse_optional_member<double>(u8"size")) {
				params.size = size.value();
			}
			if (auto style = full->template parse_optional_member<ui::font_style>(u8"style")) {
				params.style = style.value();
			}
			if (auto weight = full->template parse_optional_member<ui::font_weight>(u8"weight")) {
				params.weight = weight.value();
			}
			if (auto stretch = full->template parse_optional_member<ui::font_stretch>(u8"stretch")) {
				params.stretch = stretch.value();
			}
			return params;
		} else if (auto family = val.template try_cast<std::u8string>()) {
			// TODO more parsing
			ui::font_parameters params;
			params.family = std::move(family.value());
			return params;
		} else {
			val.template log<log_level::error>(CP_HERE) << "invalid font parameter format";
		}
		return std::nullopt;
	}


	template <typename ValueType> std::optional<ui::gradient_stop> default_parser<ui::gradient_stop>::operator()(
		const ValueType &val
	) const {
		std::optional<double> pos;
		std::optional<colord> color;
		if (auto object = val.template try_cast<typename ValueType::object_type>()) {
			if (object->size() > 2) {
				val.template log<log_level::warning>(CP_HERE) << "redundant fields in gradient stop definition";
			}
			pos = object->template parse_member<double>(u8"position");
			color = object->template parse_member<colord>(u8"color");
		} else if (auto arr = val.template try_cast<typename ValueType::array_type>()) {
			if (arr->size() >= 2) {
				if (arr->size() > 2) {
					val.template log<log_level::warning>(CP_HERE) <<
						"redundant data in gradient stop definition";
				}
				pos = arr->at(0).template parse<double>();
				color = arr->at(1).template parse<colord>();
			} else {
				val.template log<log_level::error>(CP_HERE) <<
					"not enough information in gradient stop definition";
			}
		} else {
			val.template log<log_level::error>(CP_HERE) << "invalid gradient stop format";
		}
		if (pos && color) {
			return ui::gradient_stop(color.value(), pos.value());
		}
		return std::nullopt;
	}
}


namespace codepad::ui {
	template <typename Value> std::optional<generic_brush> managed_json_parser<generic_brush>::operator()(
		const Value &val
	) const {
		if (auto obj = val.template try_cast<typename Value::object_type>()) {
			generic_brush result;
			if (auto type = obj->template parse_member<std::u8string_view>(u8"type")) {
				if (type.value() == u8"solid") {
					if (auto brush = val.template parse<brushes::solid_color>()) {
						result.value.emplace<brushes::solid_color>(std::move(brush.value()));
					}
				} else if (type.value() == u8"linear_gradient") {
					if (auto brush = val.template parse<brushes::linear_gradient>()) {
						result.value.emplace<brushes::linear_gradient>(std::move(brush.value()));
					}
				} else if (type.value() == u8"radial_gradient") {
					if (auto brush = val.template parse<brushes::radial_gradient>()) {
						result.value.emplace<brushes::radial_gradient>(std::move(brush.value()));
					}
				} else if (type.value() == u8"radial_gradient") {
					if (auto brush = val.template parse<brushes::radial_gradient>()) {
						result.value.emplace<brushes::radial_gradient>(std::move(brush.value()));
					}
				} else if (type.value() == u8"bitmap") {
					if (auto brush = val.template parse<brushes::bitmap_pattern>(
						managed_json_parser<brushes::bitmap_pattern>(_manager)
						)) {
						result.value.emplace<brushes::bitmap_pattern>(std::move(brush.value()));
					}
				} else if (type.value() != u8"none") {
					val.template log<log_level::error>(CP_HERE) << "invalid brush type";
					return std::nullopt;
				}
			}
			if (auto trans = obj->template parse_optional_member<transforms::generic>(u8"transform")) {
				result.transform = std::move(trans.value());
			}
			return result;
		} else if (auto color = val.template parse<colord>()) {
			generic_brush result;
			result.value.emplace<brushes::solid_color>(color.value());
			return result;
		}
		return std::nullopt;
	}


	template <typename Value> std::optional<generic_pen> managed_json_parser<generic_pen>::operator()(
		const Value &val
	) const {
		if (auto brush = val.template parse<generic_brush>(managed_json_parser<generic_brush>(_manager))) {
			generic_pen result;
			result.brush = brush.value();
			if (auto obj = val.template cast<typename Value::object_type>()) {
				result.thickness =
					obj->template parse_optional_member<double>(u8"thickness").value_or(result.thickness);
			}
			return result;
		}
		return std::nullopt;
	}


	template <
		typename Value
	> std::optional<generic_visual_geometry> managed_json_parser<generic_visual_geometry>::operator()(
		const Value &val
	) const {
		if (auto obj = val.template cast<typename Value::object_type>()) {
			if (auto type = obj->template parse_member<std::u8string_view>(u8"type")) {
				generic_visual_geometry result;
				if (type.value() == u8"rectangle") {
					if (auto rect = val.template parse<geometries::rectangle>()) {
						result.value.emplace<geometries::rectangle>(rect.value());
					} else {
						return std::nullopt;
					}
				} else if (type.value() == u8"rounded_rectangle") {
					if (auto rrect = val.template parse<geometries::rounded_rectangle>()) {
						result.value.emplace<geometries::rounded_rectangle>(rrect.value());
					} else {
						return std::nullopt;
					}
				} else if (type.value() == u8"ellipse") {
					if (auto ellip = val.template parse<geometries::ellipse>()) {
						result.value.emplace<geometries::ellipse>(ellip.value());
					} else {
						return std::nullopt;
					}
				} else if (type.value() == u8"path") {
					if (auto path = val.template parse<geometries::path>()) {
						result.value.emplace<geometries::path>(std::move(path.value()));
					} else {
						return std::nullopt;
					}
				} else {
					val.template log<log_level::error>(CP_HERE) << "invalid geometry type";
					return std::nullopt;
				}
				if (auto trans = obj->template parse_optional_member<transforms::generic>(u8"transform")) {
					result.transform = std::move(trans.value());
				}
				if (auto fill = obj->template parse_optional_member<generic_brush>(
					u8"fill", managed_json_parser<generic_brush>(_manager)
					)) {
					result.fill = fill.value();
				}
				if (auto stroke = obj->template parse_optional_member<generic_pen>(
					u8"stroke", managed_json_parser<generic_pen>(_manager)
					)) {
					result.stroke = stroke.value();
				}
				return result;
			}
		}
		return std::nullopt;
	}


	template <typename Value> std::optional<visuals> managed_json_parser<visuals>::operator()(
		const Value &val
	) const {
		auto geom_parser = json::array_parser<
			generic_visual_geometry, managed_json_parser<generic_visual_geometry>
		>(managed_json_parser<generic_visual_geometry>(_manager));
		if (auto obj = val.template try_cast<typename Value::object_type>()) {
			visuals res;
			if (auto geoms = obj->template parse_optional_member<std::vector<generic_visual_geometry>>(
				u8"geometries", geom_parser
				)) {
				res.geometries = std::move(geoms.value());
			}
			if (auto trans = obj->template parse_optional_member<transforms::generic>(u8"transform")) {
				res.transform = std::move(trans.value());
			}
			return res;
		} else if (val.template is<typename Value::array_type>()) {
			if (auto geoms = val.template parse<std::vector<generic_visual_geometry>>(geom_parser)) {
				visuals res;
				res.geometries = std::move(geoms.value());
				return res;
			}
		} else {
			val.template log<log_level::error>(CP_HERE) << "invalid visuals format";
		}
		return std::nullopt;
	}
}
