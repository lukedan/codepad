#pragma once

/// \file element_classes.h
/// Classes that parse and handle different element classes.

#include "../core/hotkey_registry.h"
#include "visual.h"

namespace codepad::ui {
	/// A table of textures that is to be loaded later.
	///
	/// \remark Using absolute paths with this class should be avoided. The table may cause actually
	///         different paths to be treated as the same since the parent folder is specified afterwards.
	struct texture_table {
	public:
		/// Returns a \p shared_ptr to a texture corresponding to a given path name.
		/// If no such pointer exists, one will be created.
		const std::shared_ptr<os::texture> &get(const std::filesystem::path &s) {
			auto found = _tbl.find(s);
			if (found != _tbl.end()) {
				return found->second;
			}
			return _tbl.emplace(s, std::make_shared<os::texture>()).first->second;
		}

		/// Loads all textures from the specified folder.
		void load_all(const std::filesystem::path &folder) {
			for (auto i = _tbl.begin(); i != _tbl.end(); ++i) {
				*i->second = os::load_image(folder / i->first);
			}
		}
	protected:
		/// The mapping from paths to pointer to textures.
		std::map<std::filesystem::path, std::shared_ptr<os::texture>> _tbl;
	};

	/// Parses a object from a JSON object. This class will be specialized
	/// to support different types of objects.
	///
	/// \tparam T The type of the object.
	template <typename T> struct json_object_parser {
		/// Parses the given object.
		static T parse(const json::value_t&);
	};
	/// Specialization for \ref colord.
	template <> struct json_object_parser<colord> {
		/// Parses the given object. The object can take the following formats:
		/// \code ["hsl", <h>, <s>, <l>(, <a>)] \endcode for HSL format colors, and
		/// \code [<r>, <g>, <b>(, <a>)] \endcode for RGB format colors.
		///
		/// \todo Add more formats (e.g., "FF00FF").
		inline static colord parse(const json::value_t &obj) {
			if (obj.IsArray()) { // must be an array
				if (obj.Size() > 3 && obj[0].IsString() && json::get_as_string(obj[0]) == CP_STRLIT("hsl")) {
					colord c = colord::from_hsl(obj[1].GetDouble(), obj[2].GetDouble(), obj[3].GetDouble());
					if (obj.Size() > 4) {
						c.a = obj[4].GetDouble();
					}
					return c;
				}
				if (obj.Size() > 3) {
					return colord(
						obj[0].GetDouble(), obj[1].GetDouble(),
						obj[2].GetDouble(), obj[3].GetDouble()
					);
				} else if (obj.Size() == 3) {
					return colord(
						obj[0].GetDouble(), obj[1].GetDouble(), obj[2].GetDouble(), 1.0
					);
				}
			}
			logger::get().log_warning(CP_HERE, "invalid color representation");
			return colord();
		}
	};
	/// Specialization for \ref thickness.
	template <> struct json_object_parser<thickness> {
		/// Parses the given object. The object can take the following formats:
		/// \code [<left>, <top>, <right>, <bottom>] \endcode or a single number
		/// specifying the value for all four directions.
		inline static thickness parse(const json::value_t &obj) {
			if (obj.IsArray() && obj.Size() > 3) {
				return thickness(
					obj[0].GetDouble(), obj[1].GetDouble(),
					obj[2].GetDouble(), obj[3].GetDouble()
				);
			} else if (obj.IsNumber()) {
				return thickness(obj.GetDouble());
			}
			logger::get().log_warning(CP_HERE, "invalid thickness representation");
			return thickness();
		}
	};
	/// Specialization for \ref vec2d.
	template <> struct json_object_parser<vec2d> {
		/// Parses the given object. The object must be of the following format:
		/// \code [<x>, <y>] \endcode
		inline static vec2d parse(const json::value_t &obj) {
			if (obj.IsArray() && obj.Size() > 1) {
				return vec2d(obj[0].GetDouble(), obj[1].GetDouble());
			}
			logger::get().log_warning(CP_HERE, "invalid vec2 representation");
			return vec2d();
		}
	};
	/// JSON parser for visual definitions.
	class visual_json_parser {
	public:
		constexpr static char_t
			anchor_top_char = 't', ///< The character that stands for anchor::top.
			anchor_bottom_char = 'b', ///< The character that stands for anchor::bottom.
			anchor_left_char = 'l', ///< The character that stands for anchor::left.
			anchor_right_char = 'r'; ///< The character that stands for anchor::right.

		/// Parses an \ref animated_property from a given JSON object. If the JSON object
		/// is not an object but an array, number, or any other valid format, the
		/// \ref animated_property will be reset to its default state, with only its
		/// animated_property::to parsed from the given value. As a consequence, inheritance
		/// will be ignored. Otherwise, It parses and sets all available properties. It is
		/// sometimes useful to set a value for animated_property::from and also set
		/// animated_property::has_from to \p false, in order to have a custom start value
		/// when there are no previously set values, to avoid `popping' animations.
		template <typename T> inline static void parse_animation(animated_property<T> &ani, const json::value_t &obj) {
			if (obj.IsObject()) {
				json::value_t::ConstMemberIterator mem;
				mem = obj.FindMember(CP_STRLIT("to"));
				if (mem != obj.MemberEnd()) {
					ani.to = json_object_parser<T>::parse(mem->value);
				} else {
					logger::get().log_warning(CP_HERE, "no \"to\" property found in animation");
				}
				mem = obj.FindMember(CP_STRLIT("from"));
				if (mem != obj.MemberEnd()) {
					ani.has_from = true;
					ani.from = json_object_parser<T>::parse(mem->value);
				} else {
					ani.has_from = false;
				}
				json::try_get(obj, CP_STRLIT("has_from"), ani.has_from);
				json::try_get(obj, CP_STRLIT("auto_reverse"), ani.auto_reverse);
				json::try_get(obj, CP_STRLIT("repeat"), ani.repeat);
				json::try_get(obj, CP_STRLIT("duration"), ani.duration);
				json::try_get(obj, CP_STRLIT("reverse_duration_scale"), ani.reverse_duration_scale);
				mem = obj.FindMember(CP_STRLIT("transition"));
				if (mem != obj.MemberEnd()) {
					if (mem->value.IsString()) {
						const std::function<double(double)>
							*fptr = visual::try_get_transition_func(json::get_as_string(mem->value));
						if (fptr == nullptr) {
							ani.transition_func = linear_transition_func;
							logger::get().log_warning(
								CP_HERE, "invalid transition function: ", json::get_as_string(mem->value)
							);
						} else {
							ani.transition_func = *fptr;
						}
					} else {
						logger::get().log_warning(CP_HERE, "invalid transition function");
					}
				}
			} else {
				ani = animated_property<T>();
				ani.to = json_object_parser<T>::parse(obj);
			}
		}
		/// Parses an \ref animated_property<os::texture> from a given JSON object, and registers all used
		/// textures to the given \ref texture_table. If JSON object is a string, inheritance is ignored and
		/// the specified texture is used. Otherwise, it parses all properties accordingly. The `frames' property
		/// contains a series of texture file names and durations, which will be displayed in the order specified.
		inline static void parse_animation(
			animated_property<os::texture> &ani, const json::value_t &obj, texture_table &table
		) {
			using ani_t = animated_property<os::texture>;

			bool good = true;
			if (obj.IsString()) {
				ani = animated_property<os::texture>();
				ani.frames.emplace_back(table.get(json::get_as_string(obj)), 0.0);
			} else if (obj.IsObject()) {
				auto fs = obj.FindMember(CP_STRLIT("frames"));
				if (fs != obj.MemberEnd()) {
					if (fs->value.IsArray()) {
						ani.frames.clear();
						double lastframetime = ani_t::default_frametime;
						auto beg = fs->value.Begin(), end = fs->value.End();
						for (auto i = beg; i != end; ++i) {
							if (i->IsArray()) {
								if ((*i).Size() >= 2 && (*i)[0].IsString() && (*i)[1].IsNumber()) {
									double frametime = (*i)[1].GetDouble();
									ani.frames.emplace_back(table.get(json::get_as_string((*i)[0])), frametime);
									lastframetime = frametime;
								} else {
									good = false;
								}
							} else if (i->IsString()) {
								ani.frames.emplace_back(table.get(json::get_as_string(*i)), lastframetime);
							} else {
								good = false;
							}
						}
					} else {
						good = false;
					}
				}
				json::try_get(obj, CP_STRLIT("auto_reverse"), ani.auto_reverse);
				json::try_get(obj, CP_STRLIT("repeat"), ani.repeat);
				json::try_get(obj, CP_STRLIT("reverse_duration_scale"), ani.reverse_duration_scale);
			} else {
				good = false;
			}
			if (!good) {
				logger::get().log_warning(CP_HERE, "invalid texture animation format");
			}
		}
		/// Parses a \ref visual_layer from the given JSON object, and registers all required textures to the
		/// given \ref texture_table. Calls \ref _find_and_parse to parse the properties of the layer.
		/// If the layer contains only a single string, then it is treated as the file name of the texture,
		/// and interpreted as an \ref animated_property<os::texture>.
		inline static void parse_layer(visual_layer &layer, const json::value_t &val, texture_table &table) {
			if (val.IsObject()) {
				str_t typestr;
				if (json::try_get(val, CP_STRLIT("type"), typestr)) {
					if (typestr == CP_STRLIT("solid")) {
						layer.layer_type = visual_layer::type::solid;
					} else {
						layer.layer_type = visual_layer::type::grid;
					}
				}
				_find_and_parse(val, CP_STRLIT("texture"), layer.texture_animation, table);
				_find_and_parse(val, CP_STRLIT("color"), layer.color_animation);
				_find_and_parse(val, CP_STRLIT("size"), layer.size_animation);
				_find_and_parse(val, CP_STRLIT("margins"), layer.margin_animation);
				str_t anc;
				if (json::try_get(val, CP_STRLIT("anchor"), anc)) {
					layer.rect_anchor = get_bitset_from_string<anchor>({
						{CP_STRLIT('l'), anchor::left},
						{CP_STRLIT('t'), anchor::top},
						{CP_STRLIT('r'), anchor::right},
						{CP_STRLIT('b'), anchor::bottom}
						}, anc);
				}
			} else if (val.IsString()) {
				layer = visual_layer();
				parse_animation(layer.texture_animation, val, table);
				return;
			} else {
				logger::get().log_warning(CP_HERE, "invalid layer info");
			}
		}
		/// Parses a \ref visual_state from the given JSON object, and registers all required textures to the
		/// given \ref texture_table. The JSON object must be an array, whose elements will be parsed by
		/// \ref parse_layer.
		inline static void parse_state(visual_state &vs, const json::value_t &val, texture_table &table) {
			if (val.IsArray()) {
				if (vs.layers.size() < val.Size()) {
					vs.layers.resize(val.Size());
				}
				for (size_t i = 0; i < val.Size(); ++i) {
					parse_layer(vs.layers[i], val[static_cast<rapidjson::SizeType>(i)], table);
				}
			} else {
				logger::get().log_warning(CP_HERE, "state format incorrect");
			}
		}
		/// Parses a \ref visual from the given JSON object, and registers all required textures to the given
		/// \ref texture_table. Inheritance is implemented by duplicating the \ref visual_state that it inherits
		/// from and then parse the new state without clearing the old one. Therefore, the state that's inherited
		/// from must occur before the state that inherits from it.
		inline static void parse_visual(visual &provider, const json::value_t &val, texture_table &table) {
			if (val.IsArray()) {
				for (auto i = val.Begin(); i != val.End(); ++i) {
					visual_state vps;
					visual_state::id_t id = visual_state::normal_id;
					if (i->IsObject()) {
						json::value_t::ConstMemberIterator fmem;
						fmem = i->FindMember(CP_STRLIT("states"));
						if (fmem != i->MemberEnd()) {
							id = _parse_vid(fmem->value);
						}
						fmem = i->FindMember(CP_STRLIT("inherit_from"));
						if (fmem != i->MemberEnd()) {
							visual_state::id_t pid = _parse_vid(fmem->value);
							auto found = provider._states.find(pid);
							if (found != provider._states.end()) {
								vps = found->second;
							} else {
								logger::get().log_warning(CP_HERE, "invalid inheritance");
							}
						}
						fmem = i->FindMember(CP_STRLIT("layers"));
						if (fmem != i->MemberEnd()) {
							parse_state(vps, fmem->value, table);
						}
					} else {
						parse_state(vps, *i, table);
					}
					if (!provider._states.insert(std::make_pair(id, std::move(vps))).second) {
						logger::get().log_warning(CP_HERE, "state registration failed");
					}
				}
			} else {
				logger::get().log_warning(CP_HERE, "unrecognized skin format");
			}
		}
		/// Parses the whole set of visuals from the given JSON object. The original list is not emptied so
		/// configs can be appended to one another.
		inline static void parse_config(
			std::map<str_t, visual> &providers, const json::value_t &val, texture_table &table
		) {
			for (auto i = val.MemberBegin(); i != val.MemberEnd(); ++i) {
				visual vp;
				visual_json_parser::parse_visual(vp, i->value, table);
				if (!providers.insert(
					std::make_pair(json::get_as_string(i->name), std::move(vp))
				).second) {
					logger::get().log_warning(CP_HERE, "duplicate visual provider");
				}
			}
		}
	protected:
		/// Finds the animation with the given name within the given JSON object, and parses it if there is one.
		template <typename T, typename ...Args> inline static void _find_and_parse(
			const json::value_t &val, const str_t &s, animated_property<T> &p, Args &&...args
		) {
			auto found = val.FindMember(s.c_str());
			if (found != val.MemberEnd()) {
				parse_animation(p, found->value, std::forward<Args>(args)...);
			}
		}
		/// Parses a visual state ID. The given JSON object must be an array of strings,
		/// each string of which denotes a bit that is to be set.
		inline static visual_state::id_t _parse_vid(const json::value_t &val) {
			if (!val.IsArray()) {
				return visual_state::normal_id;
			}
			visual_state::id_t id = visual_state::normal_id;
			for (auto j = val.Begin(); j != val.End(); ++j) {
				id |= visual::get_or_register_state_id(json::get_as_string(*j));
			}
			return id;
		}
	};

	/// Registry of the visual of classes.
	class class_visuals {
	public:
		std::map<str_t, visual> visuals; ///< The mapping between class names and visuals.
		unsigned timestamp = 1; ///< The timestamp indicating when the mapping was loaded.

		/// Returns the visual corresponding to the given class name. If no such visual is present,
		/// the default visual (the visual corresponding to an empty class name) is returned.
		visual &get_visual_or_default(const str_t &cls) {
			auto found = visuals.find(cls);
			if (found != visuals.end()) {
				return found->second;
			}
			return visuals[str_t()];
		}
		/// Returns the visual state corresponding to the given class name and visual state ID.
		/// Simply calls \ref get_visual_or_default and visual::get_state_or_default.
		const visual_state &get_state_or_create(const str_t &cls, visual_state::id_t id) {
			return get_visual_or_default(cls).get_state_or_default(id);
		}

		/// Loads the visual config from the given JSON object.
		texture_table load_json(const json::value_t &val) {
			++timestamp;
			visuals.clear();
			texture_table texs;
			visual_json_parser::parse_config(visuals, val, texs);
			return texs;
		}
	};

	/// Hotkey groups that are used with elements. Each hotkey corresponds to a command name.
	using element_hotkey_group = hotkey_group<str_t>;

	/// Parses hotkeys from JSON objects.
	class hotkey_json_parser {
	public:
		constexpr static char_t key_delim = '+'; ///< The delimiter for keys.

		/// Parses a modifier key from a given string.
		inline static modifier_keys parse_modifier(const str_t &str) {
			if (str == CP_STRLIT("ctrl")) {
				return modifier_keys::control;
			}
			if (str == CP_STRLIT("alt")) {
				return modifier_keys::alt;
			}
			if (str == CP_STRLIT("shift")) {
				return modifier_keys::shift;
			}
			if (str == CP_STRLIT("super")) {
				return modifier_keys::super;
			}
			logger::get().log_warning(CP_HERE, "invalid modifier");
			return modifier_keys::none;
		}
		/// Parses the given key.
		///
		/// \todo Currently incomplete.
		inline static os::input::key parse_key(const str_t &str) {
			if (str.length() == 1) {
				if (str[0] >= 'a' && str[0] <= 'z') {
					return static_cast<os::input::key>(
						static_cast<size_t>(os::input::key::a) + (str[0] - 'a')
						);
				}
				switch (str[0]) {
				case ' ':
					return os::input::key::space;
				case '+':
					return os::input::key::add;
				case '-':
					return os::input::key::subtract;
				case '*':
					return os::input::key::multiply;
				case '/':
					return os::input::key::divide;
				}
			}
			if (str == CP_STRLIT("left")) {
				return os::input::key::left;
			}
			if (str == CP_STRLIT("right")) {
				return os::input::key::right;
			}
			if (str == CP_STRLIT("up")) {
				return os::input::key::up;
			}
			if (str == CP_STRLIT("down")) {
				return os::input::key::down;
			}
			if (str == CP_STRLIT("space")) {
				return os::input::key::space;
			}
			if (str == CP_STRLIT("insert")) {
				return os::input::key::insert;
			}
			if (str == CP_STRLIT("delete")) {
				return os::input::key::del;
			}
			if (str == CP_STRLIT("backspace")) {
				return os::input::key::backspace;
			}
			if (str == CP_STRLIT("home")) {
				return os::input::key::home;
			}
			if (str == CP_STRLIT("end")) {
				return os::input::key::end;
			}
			return os::input::key::max_value;
		}
		/// Parses a \ref key_gesture from the given object. The object must be a string,
		/// which will be broken into parts with \ref key_delim. Each part is then parsed separately
		/// by either \ref parse_modifier, or \ref parse_key for the last part.
		inline static bool parse_hotkey_gesture(key_gesture &gest, const json::value_t &obj) {
			if (!obj.IsString()) {
				logger::get().log_warning(CP_HERE, "invalid key gesture");
				return false;
			}
			const char_t *last = obj.GetString(), *cur = last;
			gest.mod_keys = modifier_keys::none;
			for (size_t i = 0; i < obj.GetStringLength(); ++i, ++cur) {
				if (*cur == key_delim && cur != last) {
					set_bit(gest.mod_keys, parse_modifier(str_t(last, cur - last)));
					last = cur + 1;
				}
			}
			gest.primary = parse_key(str_t(last, cur - last));
			return true;
		}
		/// Parses a JSON object for a list of \ref key_gesture "key_gestures" and the corresponding command.
		inline static bool parse_hotkey_entry(
			std::vector<key_gesture> &gests, str_t &cmd, const json::value_t &obj
		) {
			if (!obj.IsObject()) {
				return false;
			}
			auto command = obj.FindMember(CP_STRLIT("command"));
			if (command == obj.MemberEnd() || !command->value.IsString()) {
				return false;
			}
			cmd = json::get_as_string(command->value);
			auto gestures = obj.FindMember(CP_STRLIT("gestures"));
			if (gestures == obj.MemberEnd()) {
				return false;
			}
			if (gestures->value.IsString()) {
				key_gesture g;
				if (!parse_hotkey_gesture(g, gestures->value)) {
					return false;
				}
				gests.push_back(g);
			} else if (gestures->value.IsArray()) {
				for (auto i = gestures->value.Begin(); i != gestures->value.End(); ++i) {
					key_gesture g;
					if (!parse_hotkey_gesture(g, *i)) {
						continue;
					}
					gests.push_back(g);
				}
			} else {
				return false;
			}
			return true;
		}
		/// Parses an \ref element_hotkey_group from an JSON array.
		inline static bool parse_class_hotkey(element_hotkey_group &gp, const json::value_t &obj) {
			if (!obj.IsArray()) {
				return false;
			}
			for (auto i = obj.Begin(); i != obj.End(); ++i) {
				std::vector<key_gesture> gs;
				str_t name;
				if (parse_hotkey_entry(gs, name, *i)) {
					gp.register_hotkey(gs, std::move(name));
				} else {
					logger::get().log_warning(CP_HERE, "invalid hotkey entry");
				}
			}
			return true;
		}
		/// Parses a set of \ref element_hotkey_group "element_hotkey_groups" from a given JSON object.
		inline static void parse_config(
			std::map<str_t, element_hotkey_group> &mapping, const json::value_t &obj
		) {
			if (obj.IsObject()) {
				for (auto i = obj.MemberBegin(); i != obj.MemberEnd(); ++i) {
					element_hotkey_group gp;
					if (parse_class_hotkey(gp, i->value)) {
						mapping.emplace(json::get_as_string(i->name), std::move(gp));
					}
				}
			}
		}
	};

	/// Registry of \ref element_hotkey_group "element_hotkey_groups".
	class class_hotkeys {
	public:
		std::map<str_t, element_hotkey_group> mapping; ///< The mapping between class names and hotkey groups.

		/// Finds the hotkey group corresponding to the class name given.
		///
		/// \return Pointer to the hotkey group. \p nullptr if none is found.
		const element_hotkey_group *find(const str_t &cls) const {
			auto it = mapping.find(cls);
			if (it != mapping.end()) {
				return &it->second;
			}
			return nullptr;
		}

		/// Loads the whole set of hotkeys from the given JSON object.
		void load_json(const json::value_t &val) {
			mapping.clear();
			hotkey_json_parser::parse_config(mapping, val);
		}
	};

	/// Manages the visuals and hotkeys of classes.
	class class_manager {
	public:
		class_visuals visuals; ///< All visuals.
		class_hotkeys hotkeys; ///< All hotkeys.

		/// Returns the global \ref class_manager.
		static class_manager &get();
	};
}
