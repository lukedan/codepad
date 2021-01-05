// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Parsers of JSON configuration files.

#include <vector>

#include "hotkey_registry.h"
#include "element_classes.h"
#include "element.h"
#include "animation_path_parser.h"

namespace codepad::ui {
	class manager;

	/// Parses visuals from JSON objects.
	template <typename ValueType> class arrangements_parser {
	public:
		using value_t = ValueType; ///< The type that holds JSON values.
		using object_t = typename ValueType::object_type; ///< The type that holds JSON objects.
		using array_t = typename ValueType::array_type; ///< The type that holds JSON arrays.

		/// Initializes the class with the given \ref manager.
		explicit arrangements_parser(manager &man) : _manager(man) {
		}

		/// Parses a dictionary of colors.
		void parse_color_scheme(const object_t &val, std::map<std::u8string, colord, std::less<>> &scheme) {
			for (auto it = val.member_begin(); it != val.member_end(); ++it) {
				if (auto color = it.value().template parse<colord>()) {
					scheme.emplace(it.name(), color.value());
				}
			}
		}

		/// Parses a \ref element_configuration from the given JSON object, and adds it to \p value. If one for the
		/// specified states already exists in \p value, it is kept if the inheritance is not overriden with
		/// \p inherit_from.
		void parse_configuration(const object_t &val, element_configuration &value) {
			if (auto attrobj = val.template parse_optional_member<object_t>(u8"attributes")) {
				for (auto it = attrobj->member_begin(); it != attrobj->member_end(); ++it) {
					json::value_storage attr_value = json::store(it.value());
					// FIXME here emplace() moves the json value, while try_emplace requires a new string to be
					//       constructed
					auto [attr_iter, inserted] = value.attributes.try_emplace(
						std::u8string(it.name()), std::move(attr_value)
					);
					if (!inserted) { // override base value
						attr_iter->second = std::move(attr_value);
					}
				}
			}

			if (auto ani_from = val.template parse_optional_member<std::u8string_view>(u8"inherit_animations_from")) {
				if (auto *ancestor = get_manager().get_class_arrangements().get(ani_from.value())) {
					value.event_triggers = ancestor->configuration.event_triggers;
				} else {
					val.template log<log_level::error>(CP_HERE) << "invalid animation inheritance";
				}
			}
			if (auto animations = val.template parse_optional_member<object_t>(u8"animations")) {
				for (auto sbit = animations->member_begin(); sbit != animations->member_end(); ++sbit) {
					if (auto ani_obj = sbit.value().template cast<object_t>()) {
						element_configuration::event_trigger trigger;
						trigger.identifier = element_configuration::event_identifier::parse_from_string(sbit.name());
						for (auto aniit = ani_obj->member_begin(); aniit != ani_obj->member_end(); ++aniit) {
							animation_path::component_list components;
							auto res = animation_path::parser::parse(aniit.name(), components);
							if (res != animation_path::parser::result::completed) {
								aniit.value().template log<log_level::error>(CP_HERE) <<
									"failed to segment animation path, skipping";
								continue;
							}
							element_configuration::animation_parameters aniparams;
							aniparams.subject = std::move(components);
							if (auto ani = aniit.value().template parse<generic_keyframe_animation_definition>(
								managed_json_parser<generic_keyframe_animation_definition>(get_manager())
								)) {
								aniparams.definition = ani.value();
							} else {
								continue;
							}
							trigger.animations.emplace_back(std::move(aniparams));
						}
						value.event_triggers.emplace_back(std::move(trigger));
					}
				}
			}
		}

		/// Parses additional attributes of a \ref class_arrangements::child from the given JSON object.
		void parse_additional_arrangement_attributes(
			const object_t &val, class_arrangements::child &child
		) {
			if (auto type = val.template parse_member<std::u8string_view>(u8"type")) {
				child.type = type.value();
				child.element_class = val.template parse_optional_member<std::u8string_view>(u8"class").value_or(child.type);
				child.name = val.template parse_optional_member<std::u8string_view>(u8"name").value_or(child.name);
			}
		}
		/// Parses the metrics and children arrangements of either a composite element or one of its children, given
		/// a JSON object.
		template <typename T> void parse_class_arrangements(const object_t &val, T &obj) {
			parse_configuration(val, obj.configuration);
			if (auto children = val.template parse_optional_member<array_t>(u8"children")) {
				for (auto &&elem : children.value()) {
					if (auto child = elem.template cast<object_t>()) {
						class_arrangements::child ch;
						parse_additional_arrangement_attributes(child.value(), ch);
						if (auto *cls = get_manager().get_class_arrangements().get(ch.element_class)) {
							// provide default values for its element configuration
							// TODO animations may be invalidated by additional parsing
							ch.configuration = cls->configuration;
						}
						parse_class_arrangements(child.value(), ch);
						obj.children.emplace_back(std::move(ch));
					}
				}
			}
		}
		/// Parses the whole set of arrangements for \ref _manager from the given JSON object. The original list is
		/// not emptied so configs can be appended to one another.
		void parse_arrangements_config(const object_t &val) {
			for (auto i = val.member_begin(); i != val.member_end(); ++i) {
				if (auto obj = i.value().template cast<object_t>()) {
					class_arrangements arr;
					if (auto name = obj->template parse_optional_member<std::u8string_view>(u8"name")) {
						arr.name = std::u8string(name.value());
					}
					parse_class_arrangements(obj.value(), arr);
					auto [it, inserted] = get_manager().get_class_arrangements().mapping.emplace(
						i.name(), std::move(arr)
					);
					if (!inserted) {
						logger::get().log_warning(CP_HERE) << "duplicate class arrangements";
					}
				}
			}
		}

		/// Returns the associated \ref manager.
		manager &get_manager() const {
			return _manager;
		}
	protected:
		manager &_manager; ///< The \ref manager associated with this parser.
	};

	/// Parses hotkeys from JSON objects.
	template <typename ValueType> class hotkey_json_parser {
	public:
		constexpr static char key_delim = '+'; ///< The delimiter for keys.

		using value_t = ValueType; ///< The type for JSON values.
		using object_t = typename ValueType::object_type; ///< The type for JSON objects.
		using array_t = typename ValueType::array_type; ///< The type for JSON arrays.

		/// Parses a JSON object for a list of \ref key_gesture "key_gestures" and the corresponding command.
		inline static bool parse_hotkey_entry(
			std::vector<key_gesture> &gests, hotkey_group::action &action, const object_t &obj
		) {
			if (auto act = obj.template parse_member<hotkey_group::action>(u8"action")) {
				action = std::move(act.value());
			} else {
				obj.template log<log_level::error>(CP_HERE) << "failed to parse action";
				return false;
			}
			if (auto gestures = obj.find_member(u8"gestures"); gestures != obj.member_end()) {
				if (auto gstr = gestures.value().template try_cast<std::u8string_view>()) {
					if (auto gestval = key_gesture::parse(gstr.value())) {
						gests.emplace_back(gestval.value());
					} else {
						return false;
					}
				} else if (auto garr = gestures.value().template try_cast<array_t>()) {
					for (auto &&g : garr.value()) {
						if (auto gpart = g.template try_cast<std::u8string_view>()) {
							if (auto gestval = key_gesture::parse(gpart.value())) {
								gests.emplace_back(gestval.value());
							}
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
		/// Parses an \ref hotkey_group from an JSON array.
		inline static bool parse_class_hotkey(hotkey_group &gp, const array_t &arr) {
			for (auto &&cls : arr) {
				if (auto obj = cls.template try_cast<object_t>()) {
					std::vector<key_gesture> gs;
					hotkey_group::action act;
					if (parse_hotkey_entry(gs, act, obj.value())) {
						gp.register_hotkey(gs, std::move(act));
					} else {
						logger::get().log_warning(CP_HERE) << "invalid hotkey entry";
					}
				}
			}
			return true;
		}
		/// Parses a set of \ref hotkey_group "element_hotkey_groups" from a given JSON object.
		template <typename Map> inline static void parse_config(
			Map &mapping, const object_t &obj
		) {
			for (auto i = obj.member_begin(); i != obj.member_end(); ++i) {
				hotkey_group gp;
				std::optional<array_t> key_array;
				if (auto arr = i.value().template try_cast<array_t>()) {
					key_array = arr.value();
				} else if (auto complex = i.value().template try_cast<object_t>()) {
					if (auto inherit = complex->find_member(u8"inherit_from"); inherit != complex->member_end()) {
						if (auto inherit_name = inherit.value().template cast<std::u8string_view>()) {
							if (auto iter = mapping.find(inherit_name.value()); iter != mapping.end()) {
								gp = iter->second;
							} else {
								inherit.value().template log<log_level::error>(CP_HERE) << "invalid inherit group name";
							}
						}
					}
					if (auto hotkeys = complex->template parse_optional_member<array_t>(u8"hotkeys")) {
						key_array = hotkeys.value();
					}
				} else {
					i.value().template log<log_level::error>(CP_HERE) << "invalid class hotkey group format";
					continue;
				}
				if (key_array) {
					parse_class_hotkey(gp, key_array.value());
				}
				mapping.emplace(i.name(), std::move(gp));
			}
		}
	};
}
