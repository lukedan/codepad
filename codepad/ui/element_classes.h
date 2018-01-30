#pragma once

#include "../utilities/hotkey_registry.h"
#include "visual.h"

namespace codepad::ui {
	struct texture_table {
	public:
		const std::shared_ptr<os::texture> &get(const std::filesystem::path &s) {
			auto found = _tbl.find(s);
			if (found != _tbl.end()) {
				return found->second;
			}
			return _tbl.emplace(s, std::make_shared<os::texture>()).first->second;
		}

		void load_all(const std::filesystem::path &folder) {
			for (auto i = _tbl.begin(); i != _tbl.end(); ++i) {
				*i->second = os::load_image(folder / i->first);
			}
		}
	protected:
		std::map<std::filesystem::path, std::shared_ptr<os::texture>> _tbl;
	};

	template <typename T> struct json_object_parser {
		static T parse(const json::value_t&);
	};
	template <> struct json_object_parser<colord> {
		inline static colord parse(const json::value_t &obj) {
			if (obj.IsArray()) {
				if (obj.Size() > 3 && obj[0].IsString() && str_t(obj[0].GetString()) == CP_STRLIT("hsl")) {
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
	template <> struct json_object_parser<thickness> {
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
	template <> struct json_object_parser<vec2d> {
		inline static vec2d parse(const json::value_t &obj) {
			if (obj.IsArray() && obj.Size() > 1) {
				return vec2d(obj[0].GetDouble(), obj[1].GetDouble());
			}
			logger::get().log_warning(CP_HERE, "invalid vec2 representation");
			return vec2d();
		}
	};
	class visual_json_parser {
	public:
		constexpr static char_t
			anchor_top_char = 't', anchor_bottom_char = 'b',
			anchor_left_char = 'l', anchor_right_char = 'r';

		template <typename T> inline static void parse_animation(animation_params<T> &ani, const json::value_t &obj) {
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
					json::try_get(obj, CP_STRLIT("has_from"), ani.has_from);
				}
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
				ani = animation_params<T>();
				ani.to = json_object_parser<T>::parse(obj);
			}
		}
		inline static void parse_animation(animation_params<os::texture> &ani, const json::value_t &obj, texture_table &table) {
			using ani_t = animation_params<os::texture>;

			bool good = true;
			if (obj.IsString()) {
				ani.frames.push_back(ani_t::texture_keyframe(
					table.get(make_path(json::get_as_string(obj))), 0.0
				));
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
									ani.frames.push_back(ani_t::texture_keyframe(
										table.get(make_path(json::get_as_string((*i)[0]))), frametime
									));
									lastframetime = frametime;
								} else {
									good = false;
								}
							} else if (i->IsString()) {
								ani.frames.push_back(ani_t::texture_keyframe(
									table.get(make_path(json::get_as_string(*i))), lastframetime
								));
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
					layer.rect_anchor = static_cast<anchor>(get_bitset_from_string<unsigned, anchor>({
						{'l', anchor::left},
						{'t', anchor::top},
						{'r', anchor::right},
						{'b', anchor::bottom}
						}, anc));
				}
			} else if (val.IsString()) {
				layer = visual_layer();
				parse_animation(layer.texture_animation, val, table);
				return;
			} else {
				logger::get().log_warning(CP_HERE, "invalid layer info");
			}
		}
		inline static void parse_state(visual_state &vs, const json::value_t &val, texture_table &table) {
			if (val.IsArray()) {
				if (vs._layers.size() < val.Size()) {
					vs._layers.resize(val.Size());
				}
				for (size_t i = 0; i < val.Size(); ++i) {
					parse_layer(vs._layers[i], val[static_cast<rapidjson::SizeType>(i)], table);
				}
			} else {
				logger::get().log_warning(CP_HERE, "state format incorrect");
			}
		}
		inline static void parse_provider(visual &provider, const json::value_t &val, texture_table &table) {
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
		inline static void parse_config(
			std::map<str_t, visual> &providers, const json::value_t &val, texture_table &table
		) {
			for (auto i = val.MemberBegin(); i != val.MemberEnd(); ++i) {
				visual vp;
				visual_json_parser::parse_provider(vp, i->value, table);
				if (!providers.insert(
					std::make_pair(json::get_as_string(i->name), std::move(vp))
				).second) {
					logger::get().log_warning(CP_HERE, "duplicate visual provider");
				}
			}
		}
	protected:
		template <typename T, typename ...Args> inline static void _find_and_parse(
			const json::value_t &val, const str_t &s, animation_params<T> &p, Args &&...args
		) {
			auto found = val.FindMember(s.c_str());
			if (found != val.MemberEnd()) {
				parse_animation(p, found->value, std::forward<Args>(args)...);
			}
		}
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

	class class_visuals {
	public:
		std::map<str_t, visual> providers;
		unsigned timestamp = 0;

		visual &get_provider_or_default(const str_t &cls) {
			auto found = providers.find(cls);
			if (found != providers.end()) {
				return found->second;
			}
			return providers[str_t()];
		}
		const visual_state &get_state_or_create(const str_t &cls, visual_state::id_t id) {
			return get_provider_or_default(cls).get_state_or_default(id);
		}

		texture_table load_json(const json::value_t &val) {
			++timestamp;
			providers.clear();
			texture_table texs;
			visual_json_parser::parse_config(providers, val, texs);
			return texs;
		}
	};

	using element_hotkey_group = hotkey_group<str_t>;

	class hotkey_json_parser { // TODO better error info
	public:
		constexpr static char_t key_delim = '+';

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
			return modifier_keys::none;
		}
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
		}
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
		}
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
					if (!parse_hotkey_gesture(g, gestures->value)) {
						continue;
					}
					gests.push_back(g);
				}
			} else {
				return false;
			}
			return true;
		}
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

	class class_hotkeys {
	public:
		std::map<str_t, element_hotkey_group> mapping;

		const element_hotkey_group *find(const str_t &cls) const {
			auto it = mapping.find(cls);
			if (it != mapping.end()) {
				return &it->second;
			}
			return nullptr;
		}

		void load_json(const json::value_t &val) {
			mapping.clear();
			hotkey_json_parser::parse_config(mapping, val);
		}
	};

	class class_manager {
		friend struct globals;
	public:
		class_visuals visuals;
		class_hotkeys hotkeys;

		static class_manager &get();
	};
}
