// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Parsers of JSON configuration files.

#include <vector>

#include "hotkey_registry.h"
#include "element_classes.h"
#include "element.h"
#include "manager.h"

namespace codepad::ui {
	/// Parses visuals from JSON objects.
	///
	/// \todo Warn invalid config entries.
	template <typename ValueType> class ui_config_json_parser {
	public:
		using value_t = ValueType; ///< The type that holds JSON values.
		using object_t = typename ValueType::object_t; ///< The type that holds JSON objects.
		using array_t = typename ValueType::array_t; ///< The type that holds JSON arrays.

		/// Initializes the class with the given \ref manager.
		explicit ui_config_json_parser(manager &man) : _manager(man) {
		}

		/// Parses a \ref element_configuration from the given JSON object, and adds it to \p value. If one for the
		/// specified states already exists in \p value, it is kept if the inheritance is not overriden with
		/// \p inherit_from.
		void parse_configuration(const object_t &val, element_configuration &value) {
			parse_parameters(val, value.default_parameters);

			if (object_t extraobj; json::try_cast_member(val, u8"extras", extraobj)) {
				for (auto it = extraobj.member_begin(); it != extraobj.member_end(); ++it) {
					value.additional_attributes.emplace(
						it.name(), json::store(it.value())
					);
				}
			}

			if (str_view_t from; json::try_cast_member(val, u8"inherit_animations_from", from)) {
				if (auto * ancestor = _manager.get_class_arrangements().get(from)) {
					value.event_triggers = ancestor->configuration.event_triggers;
				}
			}
			if (object_t triggers; json::try_cast_member(val, u8"animations", triggers)) {
				for (auto sbit = triggers.member_begin(); sbit != triggers.member_end(); ++sbit) {
					if (object_t sbobj; json::try_cast(sbit.value(), sbobj)) {
						element_configuration::event_trigger trigger;
						trigger.identifier = element_configuration::event_identifier::parse_from_string(sbit.name());
						for (auto aniit = sbobj.member_begin(); aniit != sbobj.member_end(); ++aniit) {
							animation_path::component_list components;
							auto res = animation_path::parser::parse(aniit.name(), components);
							if (res != animation_path::parser::result::completed) {
								logger::get().log_warning(CP_HERE, "failed to segment animation path: ", aniit.name());
								continue;
							}
							element_configuration::animation_parameters aniparams;
							aniparams.subject = std::move(components);
							_parse_keyframe_animation(aniit.value(), aniparams.definition);
							trigger.animations.emplace_back(std::move(aniparams));
						}
						value.event_triggers.emplace_back(std::move(trigger));
					}
				}
			}
		}
		/// Parses a \ref element_parameters from the given JSON object.
		void parse_parameters(const object_t &val, element_parameters &value) {
			if (str_view_t cls; json::try_cast_member(val, u8"inherit_layout_from", cls)) {
				if (auto * ancestor = _manager.get_class_arrangements().get(cls)) {
					value.layout_parameters = ancestor->configuration.default_parameters.layout_parameters;
				}
			}
			if (object_t obj; json::try_cast_member(val, u8"layout", obj)) {
				parse_layout_parameters(obj, value.layout_parameters);
			}

			if (str_view_t cls; json::try_cast_member(val, u8"inherit_visuals_from", cls)) {
				if (auto * ancestor = _manager.get_class_arrangements().get(cls)) {
					value.visual_parameters = ancestor->configuration.default_parameters.visual_parameters;
				}
			}
			if (object_t obj; json::try_cast_member(val, u8"visuals", obj)) {
				parse_visual_parameters(obj, value.visual_parameters);
			}

			json::object_parsers::try_parse_member(val, u8"visibility", value.visibility);
			json::object_parsers::try_parse_member(val, u8"cursor", value.custom_cursor);
		}
		/// Parses a \ref element_layout_parameters from the given JSON object.
		void parse_layout_parameters(const object_t &val, element_layout_parameters &value) {
			_parse_size(val, value.size, value.width_alloc, value.height_alloc);
			json::object_parsers::try_parse_member(val, u8"anchor", value.elem_anchor);
			json::object_parsers::try_parse_member(val, u8"margin", value.margin);
			json::object_parsers::try_parse_member(val, u8"padding", value.padding);
		}
		/// Parses a \ref element_visual_parameters from the given JSON object.
		void parse_visual_parameters(const object_t &val, element_visual_parameters &value) {
			json::object_parsers::try_parse_member(val, u8"transform", value.transform);
			if (array_t arr; json::try_cast_member(val, u8"geometries", arr)) {
				for (auto &&geom : arr) {
					if (object_t gobj; json::try_cast(geom, gobj)) {
						_parse_geometry(gobj, value.geometries.emplace_back());
					}
				}
			}
		}
		/// Parses additional attributes of a \ref class_arrangements::child from the given JSON object.
		void parse_additional_arrangement_attributes(
			const object_t &val, class_arrangements::child &child
		) {
			if (!json::try_cast_member(val, u8"type", child.type)) {
				logger::get().log_warning(CP_HERE, "missing type for child");
			}
			child.element_class = json::cast_member_or_default(val, u8"class", child.type);
			json::try_cast_member(val, u8"name", child.name);
		}
		/// Parses the metrics and children arrangements of either a composite element or one of its children, given
		/// a JSON object.
		template <typename T> void parse_class_arrangements(const object_t &val, T &obj) {
			parse_configuration(val, obj.configuration);
			if (array_t arr; json::try_cast_member(val, u8"children", arr)) {
				for (auto &&elem : arr) {
					if (object_t child; json::try_cast(elem, child)) {
						class_arrangements::child ch;
						parse_additional_arrangement_attributes(child, ch);
						if (auto *cls = get_manager().get_class_arrangements().get(ch.element_class)) {
							// provide default values for its element configuration
							// TODO animations may be invalidated by additional parsing
							ch.configuration = cls->configuration;
						}
						parse_class_arrangements(child, ch);
						obj.children.emplace_back(std::move(ch));
					}
				}
			}
		}
		/// Parses the whole set of arrangements for \ref _manager from the given JSON object. The original list is
		/// not emptied so configs can be appended to one another.
		void parse_arrangements_config(const object_t &val) {
			for (auto i = val.member_begin(); i != val.member_end(); ++i) {
				if (object_t obj; json::try_cast(i.value(), obj)) {
					class_arrangements arr;
					json::try_cast_member(obj, u8"name", arr.name);
					parse_class_arrangements(obj, arr);
					auto [it, inserted] = _manager.get_class_arrangements().mapping.emplace(
						i.name(), std::move(arr)
					);
					if (!inserted) {
						logger::get().log_warning(CP_HERE, "duplicate class arrangements");
					}
				}
			}
		}

		/// Parses a \ref generic_brush from the given JSON object.
		void parse_brush(const value_t &val, generic_brush &value) {
			if (object_t obj; json::try_cast(val, obj)) {
				if (str_view_t type; json::try_cast_member(obj, u8"type", type)) {
					if (type == u8"solid") {
						auto &brush = value.value.emplace<brushes::solid_color>();
						json::object_parsers::try_parse_member(obj, u8"color", brush.color);
					} else if (type == u8"linear_gradient") {
						auto &brush = value.value.emplace<brushes::linear_gradient>();
						json::object_parsers::try_parse_member(obj, u8"from", brush.from);
						json::object_parsers::try_parse_member(obj, u8"to", brush.to);
						if (array_t stops; json::try_cast_member(obj, u8"gradient_stops", stops)) {
							_parse_gradient_stop_collection(stops, brush.gradient_stops);
						}
					} else if (type == u8"radial_gradient") {
						auto &brush = value.value.emplace<brushes::radial_gradient>();
						json::object_parsers::try_parse_member(obj, u8"center", brush.center);
						json::try_cast_member(obj, u8"radius", brush.radius);
						if (array_t stops; json::try_cast_member(obj, u8"gradient_stops", stops)) {
							_parse_gradient_stop_collection(stops, brush.gradient_stops);
						}
					} else if (type == u8"bitmap") {
						auto &brush = value.value.emplace<brushes::bitmap_pattern>();
						if (str_view_t img; json::try_cast_member(obj, u8"image", img)) {
							brush.image = get_manager().get_texture(img);
						}
					} else if (type != u8"none") {
						logger::get().log_warning(CP_HERE, "invalid brush type string");
					}
				} else {
					logger::get().log_warning(CP_HERE, "invalid brush type");
				}
				json::object_parsers::try_parse_member(obj, u8"transform", value.transform);
			} else {
				auto &brush = value.value.emplace<brushes::solid_color>();
				json::object_parsers::try_parse(val, brush.color);
			}
		}
		/// Parses a \ref generic_pen from the given JSON object.
		void parse_pen(const value_t &val, generic_pen &value) {
			if (object_t obj; json::try_cast(val, obj)) {
				json::try_cast_member(obj, u8"thickness", value.thickness);
			}
			parse_brush(val, value.brush);
		}

		/// Returns the associated \ref manager.
		manager &get_manager() const {
			return _manager;
		}
	protected:
		manager &_manager; ///< The \ref manager associated with this parser.

		// below are utility functions for parsing parts of the configuration
		/// Parses the `width' or `height' field that specifies the size of an object in one direction.
		inline static void _parse_size_component(const value_t &val, double &v, size_allocation_type &ty) {
			if (str_view_t str; json::try_cast(val, str)) {
				if (str == u8"auto" || str == u8"Auto") {
					v = 0.0;
					ty = size_allocation_type::automatic;
					return;
				}
			}
			if (size_allocation alloc; json::object_parsers::try_parse(val, alloc)) {
				v = alloc.value;
				ty = alloc.is_pixels ? size_allocation_type::fixed : size_allocation_type::proportion;
				return;
			}
			logger::get().log_warning(CP_HERE, "failed to parse size component");
		}
		/// Parses the size of an element. This can either be a \ref vec2d with two \ref size_allocation, or two
		/// strings that specify the width and height, together with their \ref size_allocation.
		void _parse_size(
			const object_t &val, vec2d &size, size_allocation_type &walloc, size_allocation_type &halloc
		) {
			// first try to parse width_alloc and height_alloc, which may be overriden later
			json::object_parsers::try_parse_member(val, u8"width_alloc", walloc);
			json::object_parsers::try_parse_member(val, u8"height_alloc", halloc);
			// try to find width and height
			auto w = val.find_member(u8"width"), h = val.find_member(u8"height");
			// allows partial specification, but not mixed
			if (w != val.member_end() || h != val.member_end()) {
				if (w != val.member_end()) {
					_parse_size_component(w.value(), size.x, walloc);
				}
				if (h != val.member_end()) {
					_parse_size_component(h.value(), size.y, halloc);
				}
			} else { // parse size
				json::object_parsers::try_parse_member(val, u8"size", size);
			}
		}


		/// Parses a \ref gradient_stop_collection from the given JSON object.
		void _parse_gradient_stop_collection(const array_t &arr, gradient_stop_collection &stops) {
			for (auto &&stopdef : arr) {
				auto &stop = stops.emplace_back();
				if (object_t stopobj; json::try_cast(stopdef, stopobj)) {
					json::try_cast_member(stopobj, u8"position", stop.position);
					json::object_parsers::try_parse_member(stopobj, u8"color", stop.color);
				} else if (array_t stoparr; json::try_cast(stopdef, stoparr) && stoparr.size() >= 2) {
					if (stoparr.size() > 2) {
						logger::get().log_warning(CP_HERE, "too many items in gradient stop definition");
					}
					json::try_cast(stoparr[0], stop.position);
					json::object_parsers::try_parse(stoparr[1], stop.color);
				} else {
					logger::get().log_warning(CP_HERE, "invalid gradient stop format");
				}
			}
		}

		/// Parses a \ref geometries::path::part.
		void _parse_subpath_part(const object_t &obj, geometries::path::part &value) {
			if (obj.size() > 0) {
				if (obj.size() > 1) {
					logger::get().log_warning(CP_HERE, "too many members for a path part");
				}
				auto member = obj.member_begin();
				str_view_t op = member.name();
				if (op == u8"line_to") {
					auto &part = value.value.emplace<geometries::path::segment>();
					json::object_parsers::try_parse(member.value(), part.to);
				} else if (op == u8"arc") {
					if (object_t partobj; json::try_cast(member.value(), partobj)) {
						auto &part = value.value.emplace<geometries::path::arc>();

						bool clockwise = false, major = false;
						json::try_cast_member(partobj, u8"clockwise", clockwise);
						json::try_cast_member(partobj, u8"major", major);

						json::object_parsers::try_parse_member(partobj, u8"to", part.to);
						part.direction = clockwise ? sweep_direction::clockwise : sweep_direction::counter_clockwise;
						part.type = major ? arc_type::major : arc_type::minor;
						json::try_cast_member(partobj, u8"rotation", part.rotation);
						if (auto it = partobj.find_member(u8"radius"); it != partobj.member_end()) {
							if (double rad; json::try_cast(it.value(), rad)) { // first try parsing a single value
								part.radius.relative = vec2d();
								part.radius.absolute = vec2d(rad, rad);
							} else {
								json::object_parsers::try_parse_member(partobj, u8"radius", part.radius);
							}
						}
					}
				} else if (op == u8"bezier") {
					if (object_t partobj; json::try_cast(member.value(), partobj)) {
						auto &part = value.value.emplace<geometries::path::cubic_bezier>();
						json::object_parsers::try_parse_member(partobj, u8"to", part.to);
						json::object_parsers::try_parse_member(partobj, u8"control1", part.control1);
						json::object_parsers::try_parse_member(partobj, u8"control2", part.control2);
					}
				} else {
					logger::get().log_warning(CP_HERE, "invalid path part operation name");
				}
			} else {
				logger::get().log_warning(CP_HERE, "empty path part");
			}
		}
		/// Parses a \ref generic_visual_geometry from the given JSON object.
		void _parse_geometry(const object_t &obj, generic_visual_geometry &value) {
			if (str_view_t type; json::try_cast_member(obj, u8"type", type)) {
				if (type == u8"rectangle") {
					auto &geom = value.value.emplace<geometries::rectangle>();
					json::object_parsers::try_parse_member(obj, u8"top_left", geom.top_left);
					json::object_parsers::try_parse_member(obj, u8"bottom_right", geom.bottom_right);
				} else if (type == u8"rounded_rectangle") {
					auto &geom = value.value.emplace<geometries::rounded_rectangle>();
					json::object_parsers::try_parse_member(obj, u8"top_left", geom.top_left);
					json::object_parsers::try_parse_member(obj, u8"bottom_right", geom.bottom_right);
					json::object_parsers::try_parse_member(obj, u8"radiusx", geom.radiusx);
					json::object_parsers::try_parse_member(obj, u8"radiusy", geom.radiusy);
				} else if (type == u8"ellipse") {
					auto &geom = value.value.emplace<geometries::ellipse>();
					json::object_parsers::try_parse_member(obj, u8"top_left", geom.top_left);
					json::object_parsers::try_parse_member(obj, u8"bottom_right", geom.bottom_right);
				} else if (type == u8"path") {
					auto &geom = value.value.emplace<geometries::path>();
					if (array_t paths; json::try_cast_member(obj, u8"subpaths", paths)) {
						for (auto &&spdef : paths) {
							geometries::path::subpath &sp = geom.subpaths.emplace_back();
							if (object_t spobj; json::try_cast(spdef, spobj)) {
								json::object_parsers::try_parse_member(spobj, u8"start", sp.starting_point);
								if (array_t parts; json::try_cast_member(spobj, u8"parts", parts)) {
									for (auto &&partdef : parts) {
										if (object_t partobj; json::try_cast(partdef, partobj)) {
											_parse_subpath_part(partobj, sp.parts.emplace_back());
										}
									}
								}
								json::try_cast_member(spobj, u8"closed", sp.closed);
							} else if (array_t sparr; json::try_cast(spdef, sparr) && sparr.size() >= 2) {
								auto partit = sparr.begin(), partend = sparr.end() - 1;
								json::object_parsers::try_parse(*partit, sp.starting_point);
								for (++partit; partit != partend; ++partit) {
									if (object_t partobj; json::try_cast(*partit, partobj)) {
										_parse_subpath_part(partobj, sp.parts.emplace_back());
									}
								}
								if (object_t endobj; json::try_cast(*partend, endobj)) {
									json::try_cast_member(endobj, u8"closed", sp.closed);
								}
							} else {
								logger::get().log_warning(CP_HERE, "invalid subpath format");
							}
						}
					}
				}
			} else {
				logger::get().log_warning(CP_HERE, "invalid geometry type");
			}
			json::object_parsers::try_parse_member(obj, u8"transform", value.transform);
			if (auto fmem = obj.find_member(u8"fill"); fmem != obj.member_end()) {
				parse_brush(fmem.value(), value.fill);
			}
			if (auto fmem = obj.find_member(u8"stroke"); fmem != obj.member_end()) {
				parse_pen(fmem.value(), value.stroke);
			}
		}


		/// Parses a \ref generic_keyframe_animation_definition::keyframe from the given JSON object.
		void _parse_keyframe(const object_t obj, generic_keyframe_animation_definition::keyframe &value) {
			json::object_parsers::try_parse_member(obj, u8"duration", value.duration);
			if (auto fmem = obj.find_member(u8"to"); fmem != obj.member_end()) { // target
				value.target = json::store(fmem.value());
			}
			if (str_view_t str; json::try_cast_member(obj, u8"transition", str)) { // transition function
				if (transition_function f = get_manager().try_get_transition_func(str)) {
					value.transition_func = f;
				}
			}
		}
		/// Parses a \ref generic_keyframe_animation_definition from the given JSON value.
		void _parse_keyframe_animation(const value_t val, generic_keyframe_animation_definition &value) {
			std::optional<array_t> frames;
			if (object_t obj; json::try_cast(val, obj)) {
				if (auto fmem = obj.find_member(u8"repeat"); fmem != obj.member_end()) {
					value_t repeatval = fmem.value();
					if (repeatval.is<std::uint64_t>()) {
						value.repeat_times = static_cast<size_t>(repeatval.get<std::uint64_t>());
					} else if (repeatval.is<bool>()) { // repeat forever?
						value.repeat_times = repeatval.get<bool>() ? 0 : 1;
					} else {
						logger::get().log_warning(
							CP_HERE, "repeat must be either a non-negative integer or a boolean"
						);
					}
				}
				if (array_t arr; json::try_cast_member(obj, u8"frames", arr)) {
					frames.emplace(std::move(arr));
				} else {
					_parse_keyframe(obj, value.keyframes.emplace_back());
				}
			} else if (array_t arr; json::try_cast(val, arr)) {
				frames.emplace(std::move(arr));
			} else { // single value
				auto &kf = value.keyframes.emplace_back();
				kf.target = json::store(val);
			}
			if (frames) {
				for (auto &&kf : frames.value()) {
					if (object_t kfobj; json::try_cast(kf, kfobj)) {
						_parse_keyframe(kfobj, value.keyframes.emplace_back());
					}
				}
			}
		}
	};

	/// Parses hotkeys from JSON objects.
	template <typename ValueType> class hotkey_json_parser {
	public:
		constexpr static char key_delim = '+'; ///< The delimiter for keys.

		using value_t = ValueType; ///< The type for JSON values.
		using object_t = typename ValueType::object_t; ///< The type for JSON objects.
		using array_t = typename ValueType::array_t; ///< The type for JSON arrays.

		/// Parses a modifier key from a given string.
		inline static modifier_keys parse_modifier(str_view_t str) {
			if (str == u8"ctrl") {
				return modifier_keys::control;
			}
			if (str == u8"alt") {
				return modifier_keys::alt;
			}
			if (str == u8"shift") {
				return modifier_keys::shift;
			}
			if (str == u8"super") {
				return modifier_keys::super;
			}
			logger::get().log_warning(CP_HERE, "invalid modifier");
			return modifier_keys::none;
		}
		/// Parses the given key.
		///
		/// \todo Currently incomplete.
		inline static key parse_key(str_view_t str) {
			if (str.length() == 1) {
				if (str[0] >= 'a' && str[0] <= 'z') {
					return static_cast<key>(
						static_cast<size_t>(key::a) + (str[0] - 'a')
						);
				}
				switch (str[0]) {
				case ' ':
					return key::space;
				case '+':
					return key::add;
				case '-':
					return key::subtract;
				case '*':
					return key::multiply;
				case '/':
					return key::divide;
				}
			}
			if (str == u8"left") {
				return key::left;
			}
			if (str == u8"right") {
				return key::right;
			}
			if (str == u8"up") {
				return key::up;
			}
			if (str == u8"down") {
				return key::down;
			}
			if (str == u8"space") {
				return key::space;
			}
			if (str == u8"insert") {
				return key::insert;
			}
			if (str == u8"delete") {
				return key::del;
			}
			if (str == u8"backspace") {
				return key::backspace;
			}
			if (str == u8"home") {
				return key::home;
			}
			if (str == u8"end") {
				return key::end;
			}
			if (str == u8"enter") {
				return key::enter;
			}
			return key::max_value;
		}
		/// Parses a \ref key_gesture from the given object. The object must be a string,
		/// which will be broken into parts with \ref key_delim. Each part is then parsed separately
		/// by either \ref parse_modifier, or \ref parse_key for the last part.
		inline static bool parse_hotkey_gesture(key_gesture & gest, str_view_t val) {
			auto last = val.begin(), cur = last;
			gest.mod_keys = modifier_keys::none;
			for (; cur != val.end(); ++cur) {
				if (*cur == key_delim && cur != last) {
					gest.mod_keys |= parse_modifier(str_view_t(&*last, cur - last));
					last = cur + 1;
				}
			}
			gest.primary = parse_key(str_view_t(&*last, cur - last));
			return true;
		}
		/// Parses a JSON object for a list of \ref key_gesture "key_gestures" and the corresponding command.
		inline static bool parse_hotkey_entry(
			std::vector<key_gesture> & gests, str_t & cmd, const object_t & obj
		) {
			if (!json::try_cast_member(obj, u8"command", cmd)) {
				return false;
			}
			if (auto gestures = obj.find_member(u8"gestures"); gestures != obj.member_end()) {
				if (str_view_t gstr; json::try_cast(gestures.value(), gstr)) {
					key_gesture gestval;
					if (!parse_hotkey_gesture(gestval, gstr)) {
						return false;
					}
					gests.emplace_back(gestval);
				} else if (array_t garr; json::try_cast(gestures.value(), garr)) {
					for (auto &&g : garr) {
						if (str_view_t gpart; json::try_cast(g, gpart)) {
							key_gesture gval;
							if (!parse_hotkey_gesture(gval, gpart)) {
								continue;
							}
							gests.emplace_back(gval);
						}
					}
				} else {
					return false;
				}
			} else {
				return false;
			}
			return true;
		}
		/// Parses an \ref class_hotkey_group from an JSON array.
		inline static bool parse_class_hotkey(class_hotkey_group & gp, const array_t & arr) {
			for (auto &&cls : arr) {
				if (object_t obj; json::try_cast(cls, obj)) {
					std::vector<key_gesture> gs;
					str_t name;
					if (parse_hotkey_entry(gs, name, obj)) {
						gp.register_hotkey(gs, std::move(name));
					} else {
						logger::get().log_warning(CP_HERE, "invalid hotkey entry");
					}
				}
			}
			return true;
		}
		/// Parses a set of \ref class_hotkey_group "element_hotkey_groups" from a given JSON object.
		template <typename Map> inline static void parse_config(
			Map & mapping, const object_t & obj
		) {
			for (auto i = obj.member_begin(); i != obj.member_end(); ++i) {
				class_hotkey_group gp;
				if (array_t arr; json::try_cast(i.value(), arr)) {
					if (parse_class_hotkey(gp, arr)) {
						mapping.emplace(i.name(), std::move(gp));
					}
				}
			}
		}
	};
}
