#pragma once

/// \file
/// Classes that parse and handle different element classes.

#include "../core/hotkey_registry.h"
#include "visual.h"
#include "arrangements.h"

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

	/// This struct is used to handle the transition between different \ref visual_state or \ref metrics_state of the
	/// same class.
	///
	/// \todo Add proper mechanism to detect when \ref _ctrl or \ref _registry is out of date.
	template <typename StateValue> struct configuration {
	public:
		/// Default constructor.
		configuration() = default;
		/// Initializes the configuration with the given \ref state_mapping and \ref element_state_id. If the
		/// \ref element_state_id is not given, the state is set to \ref normal_element_state_id.
		explicit configuration(
			const state_mapping<StateValue> &reg, element_state_id state = normal_element_state_id
		) : _registry(&reg) {
			_ctrl = &_registry->match_state_or_first(state);
			_state = typename StateValue::state(*_ctrl);
		}

		/// Returns the element's \p StateValue::state, i.e., its current state regarding to the value. Note that
		/// this is not used to obtain a \ref element_state_id.
		typename StateValue::state &get_state() {
			return _state;
		}
		/// \overload
		const typename StateValue::state &get_state() const {
			return _state;
		}
		/// Returns the value that controlls how \ref _state is obtained.
		const StateValue &get_controller() const {
			return *_ctrl;
		}

		/// Calls \p StateValue::render to render in the specified region. This function will successfully
		/// instantiate only if \p StateValue has a corresponding \p render() function.
		void render(rectd rgn) const {
			_ctrl->render(rgn, _state);
		}

		/// Updates \ref _state with \ref _registry if \ref _state is not stationary.
		///
		/// \return Whether \ref _state, after having been updated, is stationary.
		bool update(double dt) {
			if (_state.all_stationary) {
				return true;
			}
			_ctrl->update(_state, dt);
			return _state.all_stationary;
		}

		/// Called when the state has been changed to update \ref _state and \ref _ctrl properly.
		void on_state_changed(element_state_id s) {
			if (_registry) {
				_ctrl = &_registry->match_state_or_first(s);
				_state = typename StateValue::state(*_ctrl, _state);
			}
		}
	protected:
		/// The current state.
		typename StateValue::state _state;
		/// The \ref state_mapping that contains the \p StateValue used to initialize \ref _state.
		const state_mapping<StateValue> *_registry = nullptr;
		/// The value used to initialize \ref _state.
		const StateValue *_ctrl = nullptr;
	};
	/// Stores the state of an \ref element's visual.
	using visual_configuration = configuration<visual_state>;
	/// Stores the state of an \ref element's metrics.
	using metrics_configuration = configuration<metrics_state>;

	/// Hotkey groups that are used with elements. Each hotkey corresponds to a command name.
	using class_hotkey_group = hotkey_group<str_t>;


	/// Contains a collection of specializations of \ref parse() that parse objects from JSON objects.
	namespace json_object_parsers {
		/// The standard parser interface.
		template <typename T> T parse(const json::value_t&);

		/// Parses \ref colord. The object can take the following formats: <tt>["hsl", h, s, l(, a)]</tt> for
		/// HSL format colors, and <tt>[r, g, b(, a)]</tt> for RGB format colors.
		template <> inline colord parse<colord>(const json::value_t &obj) {
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
		/// Parses \ref thickness. The object can take the following formats:
		/// <tt>[left, top, right, bottom]</tt> or a single number specifying the value for all four
		/// directions.
		template <> inline thickness parse<thickness>(const json::value_t &obj) {
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
		/// Parses \ref vec2d. The object must be of the following format: <tt>[x, y]</tt>
		template <> inline vec2d parse<vec2d>(const json::value_t &obj) {
			if (obj.IsArray() && obj.Size() > 1) {
				return vec2d(obj[0].GetDouble(), obj[1].GetDouble());
			}
			logger::get().log_warning(CP_HERE, "invalid vec2 representation");
			return vec2d();
		}
		/// Parses \ref anchor. The object can be a string that contains any combination of characters `l', `t',
		/// `r', and `b', standing for \ref anchor::left, \ref anchor::top, \ref anchor::right, and
		/// \ref anchor::bottom, respectively
		template <> inline anchor parse<anchor>(const json::value_t &obj) {
			if (obj.IsString()) {
				return get_bitset_from_string<anchor>({
					{CP_STRLIT('l'), anchor::left},
					{CP_STRLIT('t'), anchor::top},
					{CP_STRLIT('r'), anchor::right},
					{CP_STRLIT('b'), anchor::bottom}
					}, json::get_as_string(obj));
			}
			return anchor::none;
		}
		/// Parses \ref size_allocation_type. Checks if the given object (which must be a string) is one of the
		/// constants and returns the corresponding value. If none matches, \ref size_allocation_type::automatic is
		/// returned.
		template <> inline size_allocation_type parse<size_allocation_type>(const json::value_t &obj) {
			if (obj.IsString()) {
				str_t s = json::get_as_string(obj);
				if (s == CP_STRLIT("fixed") || s == CP_STRLIT("pixels") || s == CP_STRLIT("px")) {
					return size_allocation_type::fixed;
				}
				if (s == CP_STRLIT("proportion") || s == CP_STRLIT("prop") || s == CP_STRLIT("*")) {
					return size_allocation_type::proportion;
				}
				if (s == CP_STRLIT("automatic") || s == CP_STRLIT("auto")) {
					return size_allocation_type::automatic;
				}
			}
			return size_allocation_type::automatic;
		}
		/// Parses \ref visual_layer::type. The value can either be \p "grid" or \p "solid".
		template <> inline visual_layer::type parse<visual_layer::type>(const json::value_t &obj) {
			if (obj.IsString()) {
				str_t s = json::get_as_string(obj);
				if (s == CP_STRLIT("grid")) {
					return visual_layer::type::grid;
				} else if (s == CP_STRLIT("solid")) {
					return visual_layer::type::solid;
				}
			}
			return visual_layer::type::solid;
		}
	}

	/// Parses visuals from JSON objects.
	class ui_config_parser {
	public:
		/// Parses an \ref animated_property from a given JSON object. If the JSON object
		/// is not an object but an array, number, or any other invalid format, the
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
						const std::function<double(double)>
							*fptr = manager::get().try_get_transition_func(json::get_as_string(mem->value));
						if (fptr == nullptr) {
							ani.transition_func = transition_functions::linear;
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
				ani.to = json_object_parsers::parse<T>(obj);
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
		/// Parses a \ref visual_layer from the given JSON object, and registers all required textures to the given
		/// \ref texture_table. If the layer contains only a single string, then it is treated as the file name of
		/// the texture, and interpreted as an \ref animated_property<os::texture>.
		inline static void parse_layer(visual_layer &layer, const json::value_t &val, texture_table &table) {
			if (val.IsObject()) {
				_find_and_parse_ani(val, CP_STRLIT("texture"), layer.texture_animation, table);
				_find_and_parse_ani(val, CP_STRLIT("color"), layer.color_animation);
				_find_and_parse_ani(val, CP_STRLIT("size"), layer.size_animation);
				_find_and_parse_ani(val, CP_STRLIT("margin"), layer.margin_animation);
				_try_find_and_parse(val, CP_STRLIT("type"), layer.layer_type);
				_try_find_and_parse(val, CP_STRLIT("anchor"), layer.rect_anchor);
				_try_find_and_parse(val, CP_STRLIT("width_alloc"), layer.width_alloc);
				_try_find_and_parse(val, CP_STRLIT("height_alloc"), layer.height_alloc);
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
		inline static void parse_visual_state(visual_state &vs, const json::value_t &val, texture_table &table) {
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
		/// Parses a \ref class_visual from the given JSON object, and registers all required textures to the given
		/// \ref texture_table. Inheritance is implemented by duplicating the \ref visual_state that it inherits
		/// from and then parse the new state without clearing the old one. Therefore, the state that's inherited
		/// from must occur before the state that inherits from it.
		inline static void parse_visual(class_visual &provider, const json::value_t &val, texture_table &table) {
			if (val.IsArray()) {
				for (auto i = val.Begin(); i != val.End(); ++i) {
					visual_state vps;
					state_pattern pattern;
					if (i->IsObject()) {
						pattern = _parse_state_pattern(*i);
						json::value_t::ConstMemberIterator fmem;
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
							parse_visual_state(vps, fmem->value, table);
						}
					} else {
						parse_visual_state(vps, *i, table);
					}
					provider.register_state(pattern, std::move(vps));
				}
			} else {
				logger::get().log_warning(CP_HERE, "element visuals must be declared as an array");
			}
		}
		/// Parses the whole set of visuals from the given JSON object. The original list is not emptied so
		/// configs can be appended to one another.
		inline static void parse_visual_config(
			std::map<str_t, class_visual> &providers, const json::value_t &val, texture_table &table
		) {
			if (val.IsObject()) {
				for (auto i = val.MemberBegin(); i != val.MemberEnd(); ++i) {
					class_visual vp;
					parse_visual(vp, i->value, table);
					if (
						!providers.insert(std::make_pair(json::get_as_string(i->name), std::move(vp))).second
						) {
						logger::get().log_warning(CP_HERE, "duplicate class visuals");
					}
				}
			} else {
				logger::get().log_warning(CP_HERE, "invalid visual config format");
			}
		}

		/// Parses a \ref metrics_state from the given JSON object, and adds it to \p value. If one for the specified
		/// state already exists in \p value, it is kept if the inheritance is not overriden with \p inherit_from.
		inline static void parse_metrics_state(const json::value_t &val, element_metrics &value) {
			if (val.IsObject()) {
				metrics_state mst, *dest = &mst;
				state_pattern pattern = _parse_state_pattern(val);
				json::value_t::ConstMemberIterator fmem;
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
					_find_and_parse_ani(fmem->value, CP_STRLIT("size"), dest->size_animation);
					_find_and_parse_ani(fmem->value, CP_STRLIT("margin"), dest->margin_animation);
					_find_and_parse_ani(fmem->value, CP_STRLIT("padding"), dest->padding_animation);
					_try_find_and_parse(fmem->value, CP_STRLIT("anchor"), dest->elem_anchor);
					_try_find_and_parse(fmem->value, CP_STRLIT("width_alloc"), dest->width_alloc);
					_try_find_and_parse(fmem->value, CP_STRLIT("height_alloc"), dest->height_alloc);
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
		inline static void parse_metrics(element_metrics &metrics, const json::value_t &val) {
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
		inline static void parse_additional_arrangement_attributes(
			class_arrangements::child &child, const json::value_t &val
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
		///
		/// \param obj The object.
		/// \param val The JSON value.
		/// \param registry The parsed mapping of \ref class_arrangements so far, used for inheritance.
		template <typename T> inline static void parse_class_arrangements(
			T &obj, const json::value_t &val, const std::map<str_t, class_arrangements> &registry
		) {
			if (val.IsObject()) {
				bool has_metrics = false;
				str_t metrics_inheritance;
				if (json::try_get(val, CP_STRLIT("inherit_metrics_from"), metrics_inheritance)) {
					auto src = registry.find(metrics_inheritance);
					if (src != registry.end()) {
						has_metrics = true;
						obj.metrics = src->second.metrics;
					} else {
						logger::get().log_warning(CP_HERE, "invalid metrics inheritance");
					}
				}
				json::value_t::ConstMemberIterator fmem;
				fmem = val.FindMember(CP_STRLIT("metrics"));
				if (fmem != val.MemberEnd()) {
					has_metrics = true;
					parse_metrics(obj.metrics, fmem->value);
				}
				if (!has_metrics) {
					logger::get().log_warning(CP_HERE, "missing metrics for child");
				}
				fmem = val.FindMember(CP_STRLIT("children"));
				if (fmem != val.MemberEnd() && fmem->value.IsArray()) {
					for (auto elem = fmem->value.Begin(); elem != fmem->value.End(); ++elem) {
						class_arrangements::child ch;
						parse_additional_arrangement_attributes(ch, *elem);
						parse_class_arrangements(ch, *elem, registry);
						obj.children.emplace_back(std::move(ch));
					}
				}
			} else {
				logger::get().log_warning(CP_HERE, "invalid child arrangement format");
			}
		}
		/// Parses the whole set of arrangements from the given JSON object. The original list is not emptied so
		/// configs can be appended to one another.
		inline static void parse_arrangements_config(
			std::map<str_t, class_arrangements> &providers, const json::value_t &val
		) {
			if (val.IsObject()) {
				for (auto i = val.MemberBegin(); i != val.MemberEnd(); ++i) {
					class_arrangements arr;
					parse_class_arrangements(arr, i->value, providers);
					if (
						!providers.insert(std::make_pair(json::get_as_string(i->name), std::move(arr))).second
						) {
						logger::get().log_warning(CP_HERE, "duplicate class arrangements");
					}
				}
			} else {
				logger::get().log_warning(CP_HERE, "invalid arrangements config format");
			}
		}
	protected:
		/// Finds the animation with the given name within the given JSON object, and parses it if there is one.
		template <typename T, typename ...Args> inline static void _find_and_parse_ani(
			const json::value_t &val, const str_t &s, animated_property<T> &p, Args &&...args
		) {
			auto found = val.FindMember(s.c_str());
			if (found != val.MemberEnd()) {
				parse_animation(p, found->value, std::forward<Args>(args)...);
			}
		}
		/// Finds the attribute with the given name within the given JSON object, and parses it with a corresponding
		/// parser.
		///
		/// \return Whether the attribute has been found.
		template <typename T> inline static bool _try_find_and_parse(const json::value_t &val, const str_t &s, T &v) {
			auto found = val.FindMember(s.c_str());
			if (found != val.MemberEnd()) {
				v = json_object_parsers::parse<T>(found->value);
				return true;
			}
			return false;
		}
		/// Finds the attribute with the given name within the given JSON object, and parses it with a corresponding
		/// parser. If no such attribute is found, the given default value is returned.
		template <typename T> inline static T _find_and_parse(const json::value_t &val, const str_t &s, T dflt) {
			T v = dflt;
			if (!_try_find_and_parse(v)) {
				return dflt;
			}
			return v;
		}

		/// Finds the corresponding fields in \p val and parses them as a \ref state_pattern. If the given JSON
		/// object is an object, then this function searches for the fields `states' and `states_mask'. Otherwise,
		/// it directly passes the object to \ref _parse_state_id(), and no mask is used. All calls to
		/// \ref _parse_state_id() will have the \p configonly parameter set to \p false.
		inline static state_pattern _parse_state_pattern(const json::value_t &val) {
			state_pattern pat;
			if (val.IsObject()) {
				json::value_t::ConstMemberIterator fmem = val.FindMember(CP_STRLIT("states"));
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
		static element_state_id _parse_state_id(const json::value_t &val, bool configonly);
	};

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
					set_bits(gest.mod_keys, parse_modifier(str_t(last, cur - last)));
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
		/// Parses an \ref class_hotkey_group from an JSON array.
		inline static bool parse_class_hotkey(class_hotkey_group &gp, const json::value_t &obj) {
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
			std::map<str_t, class_hotkey_group> &mapping, const json::value_t &obj
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


	/// Registry of the visual of each element class.
	class class_visuals_registry {
	public:
		std::map<str_t, class_visual> visuals; ///< The mapping between class names and visuals.

		/// Returns the visual corresponding to the given class name. If no such visual exists, the default visual
		/// (the one corresponding to an empty class name) is returned.
		class_visual &get_visual_or_default(const str_t &cls) {
			auto found = visuals.find(cls);
			return found == visuals.end() ? visuals[str_t()] : found->second;
		}

		/// Loads the visual config from the given JSON object.
		texture_table load_json(const json::value_t &val) {
			visuals.clear();
			texture_table texs;
			ui_config_parser::parse_visual_config(visuals, val, texs);
			return texs;
		}
	};

	/// Registry of the arrangements of each element class.
	class class_arrangements_registry {
	public:
		std::map<str_t, class_arrangements> arrangements; ///< The mapping between cass names and arrangements.

		/// Returns the arrangements corresponding to the given class name. If no such arrangements exists, the
		/// default arrangements (the one corresponding to an empty class name) is returned.
		class_arrangements &get_arrangements_or_default(const str_t &cls) {
			auto found = arrangements.find(cls);
			return found == arrangements.end() ? arrangements[str_t()] : found->second;
		}

		/// Loads the arrangements config from the given JSON object.
		void load_json(const json::value_t &val) {
			arrangements.clear();
			ui_config_parser::parse_arrangements_config(arrangements, val);
		}
	};

	/// Registry of \ref class_hotkey_group "element_hotkey_groups".
	class class_hotkeys_registry {
	public:
		std::map<str_t, class_hotkey_group> hotkeys; ///< The mapping between class names and hotkey groups.

		/// Finds the hotkey group corresponding to the class name given.
		///
		/// \return Pointer to the hotkey group. \p nullptr if none is found.
		const class_hotkey_group *try_get(const str_t &cls) const {
			auto it = hotkeys.find(cls);
			if (it != hotkeys.end()) {
				return &it->second;
			}
			return nullptr;
		}

		/// Loads the whole set of hotkeys from the given JSON object.
		void load_json(const json::value_t &val) {
			hotkeys.clear();
			hotkey_json_parser::parse_config(hotkeys, val);
		}
	};


	/// Configuration of an element's state, layout, and visuals.
	struct element_configuration {
		visual_configuration visual_config; ///< Visual configuration.
		metrics_configuration metrics_config; ///< Layout configuration.
		const class_hotkey_group *hotkey_config = nullptr; ///< Hotkey configuration.

		/// Changes the state of the both configurations.
		void on_state_changed(element_state_id st) {
			visual_config.on_state_changed(st);
			metrics_config.on_state_changed(st);
		}
		/// Checks if all animations have finished.
		bool all_stationary() const {
			return visual_config.get_state().all_stationary && metrics_config.get_state().all_stationary;
		}
		/// Updates \ref visual_config and \ref metrics_config.
		///
		/// \return Whether \ref visual_config and \ref metrics_config are both stationary.
		bool update(double dt) {
			bool visstat = visual_config.update(dt), metstat = metrics_config.update(dt);
			return visstat && metstat;
		}
	};
}
