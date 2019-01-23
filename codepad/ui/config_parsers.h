#pragma once

/// \file
/// Parsers of JSON configuration files.

#include "element_classes.h"
#include "element.h"
#include "manager.h"

namespace codepad::ui {
	/// A table of textures that is to be loaded later.
	///
	/// \remark Using absolute paths with this class should be avoided. The table may cause actually
	///         different paths to be treated as the same since the parent folder is specified afterwards.
	struct texture_table {
	public:
		/// Returns a \p shared_ptr to a texture corresponding to a given path name.
		/// If no such pointer exists, one will be created.
		const std::shared_ptr<texture> &get(const std::filesystem::path &s) {
			auto found = _tbl.find(s);
			if (found != _tbl.end()) {
				return found->second;
			}
			return _tbl.emplace(s, std::make_shared<texture>()).first->second;
		}

		/// Loads all textures from the specified folder.
		void load_all(renderer_base &r, const std::filesystem::path &folder) {
			for (auto &pair : _tbl) {
				*pair.second = load_image(r, folder / pair.first);
			}
		}
	protected:
		/// The mapping from paths to pointer to textures.
		std::map<std::filesystem::path, std::shared_ptr<texture>> _tbl;
	};

	/// Parses visuals from JSON objects.
	class ui_config_json_parser {
	public:
		/// Initializes the class with the given \ref manager.
		ui_config_json_parser(manager &man) : _manager(man) {
		}

		/// Parses an \ref animated_property from a given JSON object. If the JSON object
		/// is not an object but an array, number, or any other invalid format, the
		/// \ref animated_property will be reset to its default state, with only its
		/// animated_property::to parsed from the given value. As a consequence, inheritance
		/// will be ignored. Otherwise, It parses and sets all available properties. It is
		/// sometimes useful to set a value for animated_property::from and also set
		/// animated_property::has_from to \p false, in order to have a custom start value
		/// when there are no previously set values, to avoid `popping' animations.
		template <typename T> void parse_animation(
			const json::node_t &obj, animated_property<T> &ani
		) {
			if (obj.IsObject()) {
				json::node_t::ConstMemberIterator mem;
				mem = obj.FindMember(CP_STRLIT("to"));
				if (mem != obj.MemberEnd()) {
					ani.to = json_object_parsers::parse<T>(mem->value);
				} else {
					logger::get().log_warning(CP_HERE, "no \"to\" property found in animation");
				}
				mem = obj.FindMember(CP_STRLIT("from"));
				if (mem != obj.MemberEnd()) {
					ani.has_from = true;
					ani.from = json_object_parsers::parse<T>(mem->value);
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
						transition_function
							fptr = _manager.try_get_transition_func(json::get_as_string(mem->value));
						if (fptr == nullptr) {
							ani.transition_func = transition_functions::linear;
							logger::get().log_warning(
								CP_HERE, "invalid transition function: ", json::get_as_string(mem->value)
							);
						} else {
							ani.transition_func = fptr;
						}
					} else {
						logger::get().log_warning(CP_HERE, "invalid transition function");
					}
				}
			} else {
				ani = animated_property<T>();
				ani.to = json_object_parsers::parse<T>(obj);
			}
		}
		/// Parses an \ref animated_property<texture> from a given JSON object, and registers all used
		/// textures to the given \ref texture_table. If JSON object is a string, inheritance is ignored and
		/// the specified texture is used. Otherwise, it parses all properties accordingly. The `frames' property
		/// contains a series of texture file names and durations, which will be displayed in the order specified.
		void parse_animation(const json::node_t &obj, animated_property<texture> &ani) {
			using ani_t = animated_property<texture>;

			bool good = true;
			if (obj.IsString()) {
				ani = animated_property<texture>();
				ani.frames.emplace_back(_textures.get(json::get_as_string(obj)), 0.0);
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
									ani.frames.emplace_back(_textures.get(json::get_as_string((*i)[0])), frametime);
									lastframetime = frametime;
								} else {
									good = false;
								}
							} else if (i->IsString()) {
								ani.frames.emplace_back(_textures.get(json::get_as_string(*i)), lastframetime);
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

		/// Parses a \ref visual_layer from the given JSON object, and registers all required textures to the given
		/// \ref texture_table. If the layer contains only a single string, then it is treated as the file name of
		/// the texture, and interpreted as an \ref animated_property<texture>.
		void parse_layer(const json::node_t &val, visual_layer &layer) {
			if (val.IsObject()) {
				_find_and_parse_ani(val, CP_STRLIT("texture"), layer.texture_animation);
				_find_and_parse_ani(val, CP_STRLIT("color"), layer.color_animation);
				_try_find_and_parse(val, CP_STRLIT("type"), layer.layer_type);
				_find_and_parse_ani(val, CP_STRLIT("margin"), layer.margin_animation);
				_try_find_and_parse(val, CP_STRLIT("anchor"), layer.rect_anchor);
				_parse_size(val, layer.size_animation, layer.width_alloc, layer.height_alloc);
			} else if (val.IsString()) {
				layer = visual_layer();
				parse_animation(val, layer.texture_animation);
				return;
			} else {
				logger::get().log_warning(CP_HERE, "invalid layer info");
			}
		}
		/// Parses a \ref visual_state from the given JSON object, and registers all required textures to the
		/// given \ref texture_table. The JSON object must be an array, whose elements will be parsed by
		/// \ref parse_layer.
		void parse_visual_state(const json::node_t &val, visual_state &vs) {
			if (val.IsArray()) {
				if (vs.layers.size() < val.Size()) {
					vs.layers.resize(val.Size());
				}
				for (size_t i = 0; i < val.Size(); ++i) {
					parse_layer(val[static_cast<rapidjson::SizeType>(i)], vs.layers[i]);
				}
			} else {
				logger::get().log_warning(CP_HERE, "state format incorrect");
			}
		}
		/// Parses a \ref class_visual from the given JSON object, and registers all required textures to the given
		/// \ref texture_table. Inheritance is implemented by duplicating the \ref visual_state that it inherits
		/// from and then parse the new state without clearing the old one. Therefore, the state that's inherited
		/// from must occur before the state that inherits from it.
		void parse_visual(const json::node_t &val, class_visual &provider) {
			if (val.IsArray()) {
				for (auto i = val.Begin(); i != val.End(); ++i) {
					visual_state vps;
					state_pattern pattern;
					if (i->IsObject()) {
						pattern = _parse_state_pattern(*i);
						json::node_t::ConstMemberIterator fmem;
						fmem = i->FindMember(CP_STRLIT("inherit_from"));
						if (fmem != i->MemberEnd()) {
							state_pattern tgpat = _parse_state_pattern(fmem->value);
							visual_state *st = provider.try_get_state(tgpat);
							if (st != nullptr) {
								vps = *st;
							} else {
								logger::get().log_warning(CP_HERE, "invalid inheritance");
							}
						}
						fmem = i->FindMember(CP_STRLIT("layers"));
						if (fmem != i->MemberEnd()) {
							parse_visual_state(fmem->value, vps);
						}
					} else {
						parse_visual_state(*i, vps);
					}
					provider.register_state(pattern, std::move(vps));
				}
			} else {
				logger::get().log_warning(CP_HERE, "element visuals must be declared as an array");
			}
		}
		/// Parses the whole set of visuals for \ref _manager from the given JSON object. The original list is not
		/// emptied so configs can be appended to one another.
		void parse_visual_config(const json::node_t &val) {
			if (val.IsObject()) {
				for (auto i = val.MemberBegin(); i != val.MemberEnd(); ++i) {
					class_visual vp;
					parse_visual(i->value, vp);
					auto[it, inserted] = _manager.get_class_visuals().mapping.try_emplace(
						json::get_as_string(i->name), std::move(vp)
					);
					if (!inserted) {
						logger::get().log_warning(CP_HERE, "duplicate class visuals");
					}
				}
			} else {
				logger::get().log_warning(CP_HERE, "invalid visual config format");
			}
		}

		/// Parses a \ref metrics_state from the given JSON object, and adds it to \p value. If one for the specified
		/// state already exists in \p value, it is kept if the inheritance is not overriden with \p inherit_from.
		void parse_metrics_state(const json::node_t &val, element_metrics &value) {
			if (val.IsObject()) {
				metrics_state mst, *dest = &mst;
				state_pattern pattern = _parse_state_pattern(val);
				json::node_t::ConstMemberIterator fmem;
				fmem = val.FindMember(CP_STRLIT("inherit_from"));
				if (fmem != val.MemberEnd()) {
					state_pattern frompat = _parse_state_pattern(fmem->value);
					metrics_state *st = value.try_get_state(frompat);
					if (st != nullptr) {
						mst = *st;
					} else {
						logger::get().log_warning(CP_HERE, "invalid inheritance");
					}
				} else {
					metrics_state *present = value.try_get_state(pattern);
					if (present != nullptr) {
						dest = present;
					}
				}
				fmem = val.FindMember(CP_STRLIT("value"));
				if (fmem != val.MemberEnd() && fmem->value.IsObject()) {
					_find_and_parse_ani(fmem->value, CP_STRLIT("padding"), dest->padding_animation);
					_find_and_parse_ani(fmem->value, CP_STRLIT("margin"), dest->margin_animation);
					_try_find_and_parse(fmem->value, CP_STRLIT("anchor"), dest->elem_anchor);
					_parse_size(fmem->value, dest->size_animation, dest->width_alloc, dest->height_alloc);
				} else {
					logger::get().log_warning(CP_HERE, "cannot find metrics value");
				}
				if (dest == &mst) {
					value.register_state(pattern, std::move(mst));
				}
			} else {
				logger::get().log_warning(CP_HERE, "invalid metrics state format");
			}
		}
		/// Parses a \ref element_metrics from the given JSON object. Inheritance is implemented in a way similar to
		/// that of \ref parse_visual(). Note that inheritance of \ref metrics_state will override that of
		/// \ref element_metrics.
		void parse_metrics(const json::node_t &val, element_metrics &metrics) {
			if (val.IsArray()) {
				for (auto it = val.Begin(); it != val.End(); ++it) {
					parse_metrics_state(*it, metrics);
				}
			} else if (val.IsObject()) {
				parse_metrics_state(val, metrics);
			} else {
				logger::get().log_warning(CP_HERE, "element metrics must be declared as an array");
			}
		}
		/// Parses additional attributes of a \ref class_arrangements::child from the given JSON object.
		void parse_additional_arrangement_attributes(
			const json::node_t &val, class_arrangements::child &child
		) {
			if (val.IsObject()) {
				if (!json::try_get(val, CP_STRLIT("type"), child.type)) {
					logger::get().log_warning(CP_HERE, "missing type for child");
				}
				if (!json::try_get(val, CP_STRLIT("class"), child.element_class)) {
					child.element_class = child.type;
				}
				json::try_get(val, CP_STRLIT("role"), child.role);
				auto fmem = val.FindMember(CP_STRLIT("set_states"));
				if (fmem != val.MemberEnd()) {
					child.set_states = _parse_state_id(fmem->value, true);
				}
			} else {
				logger::get().log_warning(CP_HERE, "invalid child arrangement format");
			}
		}
		/// Parses the metrics and children arrangements of either a composite element or one of its children, given
		/// a JSON object.
		template <typename T> void parse_class_arrangements(const json::node_t &val, T &obj) {
			if (val.IsObject()) {
				bool has_metrics = false;
				str_t metrics_inheritance;
				auto &registry = _manager.get_class_arrangements();
				if (json::try_get(val, CP_STRLIT("inherit_metrics_from"), metrics_inheritance)) {
					auto src = registry.mapping.find(metrics_inheritance);
					if (src != registry.mapping.end()) {
						has_metrics = true;
						obj.metrics = src->second.metrics;
					} else {
						logger::get().log_warning(CP_HERE, "invalid metrics inheritance");
					}
				}
				json::node_t::ConstMemberIterator fmem;
				fmem = val.FindMember(CP_STRLIT("metrics"));
				if (fmem != val.MemberEnd()) {
					has_metrics = true;
					parse_metrics(fmem->value, obj.metrics);
				}
				if (!has_metrics) {
					logger::get().log_warning(CP_HERE, "missing metrics for child");
				}
				fmem = val.FindMember(CP_STRLIT("children"));
				if (fmem != val.MemberEnd() && fmem->value.IsArray()) {
					for (auto elem = fmem->value.Begin(); elem != fmem->value.End(); ++elem) {
						class_arrangements::child ch;
						parse_additional_arrangement_attributes(*elem, ch);
						parse_class_arrangements(*elem, ch);
						obj.children.emplace_back(std::move(ch));
					}
				}
			} else {
				logger::get().log_warning(CP_HERE, "invalid child arrangement format");
			}
		}
		/// Parses the whole set of arrangements for \ref _manager from the given JSON object. The original list is
		/// not emptied so configs can be appended to one another.
		void parse_arrangements_config(const json::node_t &val) {
			if (val.IsObject()) {
				for (auto i = val.MemberBegin(); i != val.MemberEnd(); ++i) {
					class_arrangements arr;
					parse_class_arrangements(i->value, arr);
					auto[it, inserted] = _manager.get_class_arrangements().mapping.try_emplace(
						json::get_as_string(i->name), std::move(arr)
					);
					if (!inserted) {
						logger::get().log_warning(CP_HERE, "duplicate class arrangements");
					}
				}
			} else {
				logger::get().log_warning(CP_HERE, "invalid arrangements config format");
			}
		}

		/// Returns the list of textures.
		texture_table &get_texture_table() {
			return _textures;
		}
		/// \overload
		const texture_table &get_texture_table() const {
			return _textures;
		}
	protected:
		texture_table _textures; ///< Stores the list of textures to be loaded.
		manager &_manager; ///< The \ref manager associated with this parser.

		/// Finds the animation with the given name within the given JSON object, and parses it if there is one.
		template <typename T> void _find_and_parse_ani(
			const json::node_t &val, const str_t &s, animated_property<T> &p
		) {
			auto found = val.FindMember(s.c_str());
			if (found != val.MemberEnd()) {
				parse_animation(found->value, p);
			}
		}
		/// Finds the attribute with the given name within the given JSON object, and parses it with a corresponding
		/// parser.
		///
		/// \return Whether the attribute has been found.
		template <typename T> inline static bool _try_find_and_parse(const json::node_t &val, const str_t &s, T &v) {
			auto found = val.FindMember(s.c_str());
			if (found != val.MemberEnd()) {
				v = json_object_parsers::parse<T>(found->value);
				return true;
			}
			return false;
		}
		/// Finds the attribute with the given name within the given JSON object, and parses it with a corresponding
		/// parser. If no such attribute is found, the given default value is returned.
		template <typename T> inline static T _find_and_parse(const json::node_t &val, const str_t &s, T dflt) {
			T v = dflt;
			_try_find_and_parse(val, s, v);
			return v;
		}

		/// Parses the `width' or `height' field that specifies the size of an object in one direction.
		inline static void _parse_size_component(const json::node_t &val, double &v, size_allocation_type &ty) {
			if (val.IsString()) {
				str_view_t view = json::get_as_string_view(val);
				if (view == "auto" || view == "Auto") {
					v = 0.0;
					ty = size_allocation_type::automatic;
					return;
				}
			}
			auto alloc = json_object_parsers::parse<size_allocation>(val);
			v = alloc.value;
			ty = alloc.is_pixels ? size_allocation_type::fixed : size_allocation_type::proportion;
		}
		/// Parses the size of an object. This can either be a size animation with two allocation types if there's
		/// animation, or two \ref size_allocation that specify the static size of the object.
		void _parse_size(
			const json::node_t &val, animated_property<vec2d> &size,
			size_allocation_type &wtype, size_allocation_type &htype
		) {
			// first try to parse width_alloc and height_alloc, which may be overriden later
			_try_find_and_parse(val, CP_STRLIT("width_alloc"), wtype);
			_try_find_and_parse(val, CP_STRLIT("height_alloc"), htype);
			// try to find width and height
			auto w = val.FindMember("width"), h = val.FindMember("height");
			// allows partial specification, but not mixing with size
			if (w != val.MemberEnd() || h != val.MemberEnd()) {
				vec2d sz;
				if (w != val.MemberEnd()) {
					_parse_size_component(w->value, sz.x, wtype);
				}
				if (h != val.MemberEnd()) {
					_parse_size_component(h->value, sz.y, htype);
				}
				size = animated_property<vec2d>();
				size.to = sz;
			} else { // parse size
				_find_and_parse_ani(val, CP_STRLIT("size"), size);
			}
		}

		/// Finds the corresponding fields in \p val and parses them as a \ref state_pattern. If the given JSON
		/// object is an object, then this function searches for the fields `states' and `states_mask'. Otherwise,
		/// it directly passes the object to \ref _parse_state_id(), and no mask is used. All calls to
		/// \ref _parse_state_id() will have the \p configonly parameter set to \p false.
		state_pattern _parse_state_pattern(const json::node_t &val) {
			state_pattern pat;
			if (val.IsObject()) {
				json::node_t::ConstMemberIterator fmem = val.FindMember(CP_STRLIT("states"));
				if (fmem != val.MemberEnd()) {
					pat.target = _parse_state_id(fmem->value, false);
				}
				fmem = val.FindMember(CP_STRLIT("states_mask"));
				if (fmem != val.MemberEnd()) {
					pat.mask = _parse_state_id(fmem->value, false);
				}
			} else {
				pat.target = _parse_state_id(val, false);
			}
			return pat;
		}
		/// Parses a visual state ID. The given JSON object can either be a single state name, or an array of state
		/// names.
		///
		/// \param val The JSON object, either a string or an array of strings, each denoting a state bit.
		/// \param configonly If \p true, only config states will be loaded; encountering non-config states will
		///                   result in a warning message.
		/// \sa element_state_type
		element_state_id _parse_state_id(const json::node_t &val, bool configonly) {
			element_state_id id = normal_element_state_id;
			if (val.IsString()) {
				id = _manager.get_state_info(json::get_as_string(val)).id;
			} else if (val.IsArray()) {
				for (auto j = val.Begin(); j != val.End(); ++j) {
					if (j->IsString()) {
						element_state_info st = _manager.get_state_info(json::get_as_string(*j));
						if (configonly && st.type != element_state_type::configuration) {
							logger::get().log_warning(CP_HERE, "non-config state bit encountered in config-only state");
						} else {
							id |= st.id;
						}
					}
				}
			} else {
				logger::get().log_warning(CP_HERE, "invalid state ID format");
			}
			return id;
		}
	};

	/// Parses hotkeys from JSON objects.
	class hotkey_json_parser {
	public:
		constexpr static char key_delim = '+'; ///< The delimiter for keys.

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
		inline static key parse_key(const str_t &str) {
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
			if (str == CP_STRLIT("left")) {
				return key::left;
			}
			if (str == CP_STRLIT("right")) {
				return key::right;
			}
			if (str == CP_STRLIT("up")) {
				return key::up;
			}
			if (str == CP_STRLIT("down")) {
				return key::down;
			}
			if (str == CP_STRLIT("space")) {
				return key::space;
			}
			if (str == CP_STRLIT("insert")) {
				return key::insert;
			}
			if (str == CP_STRLIT("delete")) {
				return key::del;
			}
			if (str == CP_STRLIT("backspace")) {
				return key::backspace;
			}
			if (str == CP_STRLIT("home")) {
				return key::home;
			}
			if (str == CP_STRLIT("end")) {
				return key::end;
			}
			return key::max_value;
		}
		/// Parses a \ref key_gesture from the given object. The object must be a string,
		/// which will be broken into parts with \ref key_delim. Each part is then parsed separately
		/// by either \ref parse_modifier, or \ref parse_key for the last part.
		inline static bool parse_hotkey_gesture(key_gesture &gest, const json::node_t &obj) {
			if (!obj.IsString()) {
				logger::get().log_warning(CP_HERE, "invalid key gesture");
				return false;
			}
			const char *last = obj.GetString(), *cur = last;
			gest.mod_keys = modifier_keys::none;
			for (size_t i = 0; i < obj.GetStringLength(); ++i, ++cur) {
				if (*cur == key_delim && cur != last) {
					gest.mod_keys |= parse_modifier(str_t(last, cur - last));
					last = cur + 1;
				}
			}
			gest.primary = parse_key(str_t(last, cur - last));
			return true;
		}
		/// Parses a JSON object for a list of \ref key_gesture "key_gestures" and the corresponding command.
		inline static bool parse_hotkey_entry(
			std::vector<key_gesture> &gests, str_t &cmd, const json::node_t &obj
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
		/// Parses an \ref class_hotkey_group from an JSON array.
		inline static bool parse_class_hotkey(class_hotkey_group &gp, const json::node_t &obj) {
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
		/// Parses a set of \ref class_hotkey_group "element_hotkey_groups" from a given JSON object.
		inline static void parse_config(
			std::map<str_t, class_hotkey_group> &mapping, const json::node_t &obj
		) {
			if (obj.IsObject()) {
				for (auto i = obj.MemberBegin(); i != obj.MemberEnd(); ++i) {
					class_hotkey_group gp;
					if (parse_class_hotkey(gp, i->value)) {
						mapping.emplace(json::get_as_string(i->name), std::move(gp));
					}
				}
			}
		}
	};
}
