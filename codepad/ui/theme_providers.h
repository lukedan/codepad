#pragma once

#include <unordered_map>
#include <memory>

#include "../os/current.h"
#include "draw.h"

namespace codepad {
	namespace ui {
		class element;

		struct thickness {
			constexpr explicit thickness(double uni = 0.0) : left(uni), top(uni), right(uni), bottom(uni) {
			}
			constexpr thickness(double l, double t, double r, double b) : left(l), top(t), right(r), bottom(b) {
			}
			double left, top, right, bottom;

			constexpr rectd extend(rectd r) const {
				return rectd(r.xmin - left, r.xmax + right, r.ymin - top, r.ymax + bottom);
			}
			constexpr rectd shrink(rectd r) const {
				return rectd(r.xmin + left, r.xmax - right, r.ymin + top, r.ymax - bottom);
			}

			constexpr double width() const {
				return left + right;
			}
			constexpr double height() const {
				return top + bottom;
			}
			constexpr vec2d size() const {
				return vec2d(width(), height());
			}
		};
		enum class cursor {
			normal,
			busy,
			crosshair,
			hand,
			help,
			text_beam,
			denied,
			arrow_all,
			arrow_northeast_southwest,
			arrow_north_south,
			arrow_northwest_southeast,
			arrow_east_west,

			invisible,
			not_specified
		};
		enum class anchor : unsigned char {
			none = 0,

			left = 1,
			top = 2,
			right = 4,
			bottom = 8,

			top_left = top | left,
			top_right = top | right,
			bottom_left = bottom | left,
			bottom_right = bottom | right,

			stretch_horizontally = left | right,
			stretch_vertically = top | bottom,

			dock_left = stretch_vertically | left,
			dock_top = stretch_horizontally | top,
			dock_right = stretch_vertically | right,
			dock_bottom = stretch_horizontally | bottom,

			all = left | top | right | bottom
		};
		enum class visibility : unsigned char {
			ignored = 0,
			render_only = 1,
			interaction_only = 2,
			visible = render_only | interaction_only
		};

		// TODO control templates

		inline double linear_transition_func(double v) {
			return v;
		}
		inline double concave_quadratic_transition_func(double v) {
			return v * v;
		}
		inline double convex_quadratic_transition_func(double v) {
			v = 1.0 - v;
			return 1.0 - v * v;
		}
		inline double smoothstep_transition_func(double v) {
			return v * v * (3.0 - 2.0 * v);
		}
		class visual_json_parser;

		template <typename T> struct animation_params {
			struct state {
				T from, current_value;
				double current_time_warped = 0.0;
				bool stationary = false;
			};

			T from, to;
			bool has_from = false, auto_reverse = false, repeat = false;
			double duration = 0.0, reverse_duration_scale = 1.0;
			std::function<double(double)> transition_func = linear_transition_func;

			state init_state(T curv) const {
				state s;
				s.from = has_from ? from : curv;
				s.current_value = s.from;
				return s;
			}
			void update(state &s, double dt) const {
				if (!s.stationary) {
					s.current_time_warped += dt;
					double period = duration;
					if (auto_reverse) {
						period += duration * reverse_duration_scale;
					}
					if (s.current_time_warped >= period) {
						if (repeat) {
							s.current_time_warped -= period * std::floor(s.current_time_warped / period);
						} else {
							s.current_value = to;
							s.stationary = true;
							return;
						}
					}
					double progress = 0.0;
					if (s.current_time_warped < duration) {
						progress = s.current_time_warped / duration;
					} else {
						progress = 1.0 - (s.current_time_warped - duration) / (duration * reverse_duration_scale);
					}
					s.current_value = lerp(s.from, to, transition_func(progress));
				}
			}
		};
		template <> struct animation_params<os::texture> {
			constexpr static double default_frametime = 1.0 / 30.0;

			animation_params() {
				frames.push_back(texture_keyframe(std::make_shared<os::texture>(), 0.0)); // HACK otherwise objects without texture won't be rendered
			}

			using texture_keyframe = std::pair<std::shared_ptr<os::texture>, double>;
			struct state {
				std::vector<texture_keyframe>::const_iterator current_frame;
				double current_frame_time = 0.0;
				bool reversing = false, stationary = false;
			};

			std::vector<texture_keyframe> frames;
			bool auto_reverse = false, repeat = false;
			double reverse_duration_scale = 1.0;

			double duration() const {
				double res = 0.0;
				for (auto i = frames.begin(); i != frames.end(); ++i) {
					res += i->second;
				}
				return res;
			}
			state init_state() const {
				state s;
				s.current_frame = frames.begin();
				return s;
			}
			void update(state &s, double dt) const {
				if (s.stationary) {
					return;
				}
				if (s.current_frame != frames.end()) {
					s.current_frame_time += dt;
					while (s.current_frame_time >= s.current_frame->second) {
						if (s.reversing) {
							s.current_frame_time -= s.current_frame->second * reverse_duration_scale;
							if (s.current_frame == frames.begin()) {
								if (!repeat) {
									s.stationary = true;
									break;
								}
								s.reversing = false;
							} else {
								--s.current_frame;
							}
						} else {
							s.current_frame_time -= s.current_frame->second;
							++s.current_frame;
							if (s.current_frame == frames.end()) {
								if (repeat) {
									if (auto_reverse) {
										s.reversing = true;
										--s.current_frame;
									} else {
										s.current_frame = frames.begin();
									}
								} else {
									--s.current_frame;
									s.stationary = true;
									break;
								}
							}
						}
					}
				}
			}
		};
		struct visual_layer {
			struct state {
			public:
				animation_params<os::texture>::state current_texture;
				animation_params<colord>::state current_color;
				animation_params<vec2d>::state current_size;
				animation_params<thickness>::state current_margin;
				bool all_stationary = false;
			};
			enum class type {
				solid,
				grid,
			};

			rectd get_center_rect(const state&, rectd) const;
			state init_state() const {
				state s;
				s.current_texture = texture_animation.init_state();
				s.current_color = color_animation.init_state(colord());
				s.current_size = size_animation.init_state(vec2d());
				s.current_margin = margin_animation.init_state(thickness());
				return s;
			}
			state init_state(const state &old) const {
				state s;
				s.current_texture = texture_animation.init_state();
				s.current_color = color_animation.init_state(old.current_color.current_value);
				s.current_size = size_animation.init_state(old.current_size.current_value);
				s.current_margin = margin_animation.init_state(old.current_margin.current_value);
				return s;
			}
			void update(state &s, double dt) const {
				if (!s.all_stationary) {
					texture_animation.update(s.current_texture, dt);
					color_animation.update(s.current_color, dt);
					size_animation.update(s.current_size, dt);
					margin_animation.update(s.current_margin, dt);
					s.all_stationary =
						s.current_texture.stationary && s.current_color.stationary &&
						s.current_size.stationary && s.current_margin.stationary;
				}
			}
			void render(rectd layout, const state &s) const {
				if (s.current_texture.current_frame == texture_animation.frames.end()) {
					return;
				}
				switch (layer_type) {
				case type::solid:
					{
						rectd cln = get_center_rect(s, layout);
						os::renderer_base::get().draw_quad(
							*s.current_texture.current_frame->first, cln,
							rectd(0.0, 1.0, 0.0, 1.0), s.current_color.current_value
						);
					}
					break;
				case type::grid:
					{
						size_t
							w = s.current_texture.current_frame->first->get_width(),
							h = s.current_texture.current_frame->first->get_height();
						rectd
							outer = layout, inner = get_center_rect(s, outer),
							texr(
								s.current_margin.current_value.left / static_cast<double>(w),
								1.0 - s.current_margin.current_value.right / static_cast<double>(w),
								s.current_margin.current_value.top / static_cast<double>(h),
								1.0 - s.current_margin.current_value.bottom / static_cast<double>(h)
							);
						colord curc = s.current_color.current_value;
						render_batch rb;
						rb.reserve(18);
						rb.add_quad(
							rectd(outer.xmin, inner.xmin, outer.ymin, inner.ymin),
							rectd(0.0, texr.xmin, 0.0, texr.ymin), curc
						);
						rb.add_quad(
							rectd(inner.xmin, inner.xmax, outer.ymin, inner.ymin),
							rectd(texr.xmin, texr.xmax, 0.0, texr.ymin), curc
						);
						rb.add_quad(
							rectd(inner.xmax, outer.xmax, outer.ymin, inner.ymin),
							rectd(texr.xmax, 1.0, 0.0, texr.ymin), curc
						);
						rb.add_quad(
							rectd(outer.xmin, inner.xmin, inner.ymin, inner.ymax),
							rectd(0.0, texr.xmin, texr.ymin, texr.ymax), curc
						);
						rb.add_quad(
							rectd(inner.xmin, inner.xmax, inner.ymin, inner.ymax),
							rectd(texr.xmin, texr.xmax, texr.ymin, texr.ymax), curc
						);
						rb.add_quad(
							rectd(inner.xmax, outer.xmax, inner.ymin, inner.ymax),
							rectd(texr.xmax, 1.0, texr.ymin, texr.ymax), curc
						);
						rb.add_quad(
							rectd(outer.xmin, inner.xmin, inner.ymax, outer.ymax),
							rectd(0.0, texr.xmin, texr.ymax, 1.0), curc
						);
						rb.add_quad(
							rectd(inner.xmin, inner.xmax, inner.ymax, outer.ymax),
							rectd(texr.xmin, texr.xmax, texr.ymax, 1.0), curc
						);
						rb.add_quad(
							rectd(inner.xmax, outer.xmax, inner.ymax, outer.ymax),
							rectd(texr.xmax, 1.0, texr.ymax, 1.0), curc
						);
						rb.draw(*s.current_texture.current_frame->first);
					}
					break;
				}
			}

			void clear() {

			}

			animation_params<os::texture> texture_animation;
			animation_params<colord> color_animation;
			animation_params<vec2d> size_animation;
			animation_params<thickness> margin_animation;
			anchor rect_anchor = anchor::all;
			type layer_type = type::solid;
		};
		class visual_provider_state {
			friend class visual_json_parser;
		public:
			struct state {
				std::vector<visual_layer::state> layer_states;
				unsigned timestamp = 0;
				bool all_stationary = false;
			};

			const std::vector<visual_layer> &get_layers() const {
				return _layers;
			}
			state init_state() const;
			state init_state(const state&) const;
			void update(state&, double) const;
			void render(rectd rgn, const state &s) const {
				auto j = s.layer_states.begin();
				for (auto i = _layers.begin(); i != _layers.end(); ++i, ++j) {
					assert_true_usage(j != s.layer_states.end(), "invalid layer state data");
					i->render(rgn, *j);
				}
			}
		protected:
			std::vector<visual_layer> _layers;
		};
		using visual_state_id = unsigned;
		class visual_provider;
		class visual_manager {
			friend class visual_provider;
			friend class visual_json_parser;
			friend struct codepad::globals;
		public:
			constexpr static visual_state_id normal_state = 0;

			struct predefined_states {
				visual_state_id mouse_over, mouse_down, focused;
			};

			inline static const predefined_states &default_states() {
				return _get_table().predefined;
			}
			inline static visual_state_id get_state_id(const str_t &s) {
				return _get_table().state_id_mapping.at(s);
			}
			inline static visual_provider &get_provider_or_default(const str_t &s) {
				return _get_table().get_provider_or_default(s);
			}
			inline static unsigned current_timestamp() {
				return _get_table().timestamp;
			}
			template <typename T> inline static bool register_transition_function(str_t &&s, T &&f) {
				return _get_table().register_transition_func(std::forward<str_t>(s), std::forward<T>(f));
			}
			inline static const std::function<double(double)> &get_transition_function(const str_t &s) {
				return _get_table().transition_func_mapping.at(s);
			}
			inline static void load_config(const json::value_t&);
		protected:
			struct _registration {
				_registration() {
					predefined.mouse_over = register_or_get_state(U"mouse_over");
					predefined.mouse_down = register_or_get_state(U"mouse_down");
					predefined.focused = register_or_get_state(U"focused");

					register_transition_func(U"linear", linear_transition_func);
					register_transition_func(U"concave_quadratic", concave_quadratic_transition_func);
					register_transition_func(U"convex_quadratic", convex_quadratic_transition_func);
					register_transition_func(U"smoothstep", smoothstep_transition_func);
				}

				std::map<str_t, visual_provider> providers;
				std::map<str_t, visual_state_id> state_id_mapping;
				std::map<visual_state_id, str_t> state_name_mapping; // TODO is this necessary?
				std::map<str_t, std::function<double(double)>> transition_func_mapping;
				predefined_states predefined;
				unsigned timestamp = 0;
				int mapping_alloc = 0;

				visual_provider &get_provider_or_default(const str_t&);

				visual_state_id register_or_get_state(str_t name) {
					auto found = state_id_mapping.find(name);
					if (found == state_id_mapping.end()) {
						logger::get().log_info(CP_HERE, "registering state: ", convert_to_utf8(name));
						visual_state_id res = 1 << mapping_alloc;
						++mapping_alloc;
						state_id_mapping[name] = res;
						state_name_mapping[res] = std::move(name);
						return res;
					}
					return found->second;
				}
				template <typename T> bool register_transition_func(str_t &&s, T &&f) {
					return transition_func_mapping.insert(
						std::make_pair(std::forward<str_t>(s), std::forward<T>(f))
					).second;
				}
			};
			static _registration &_get_table();
		};
		class visual_provider {
			friend class visual_json_parser;
		public:
			struct render_state {
			public:
				void set_class(str_t cls) {
					_cls = std::move(cls);
					_animst = visual_manager::get_provider_or_default(_cls).get_state_or_default(_state).init_state();
				}
				const str_t &get_class() const {
					return _cls;
				}

				void set_state(visual_state_id s) {
					_state = s;
					_animst = visual_manager::get_provider_or_default(_cls).get_state_or_default(_state).init_state(_animst);
				}
				void set_state_bit(visual_state_id bit, bool set) {
					visual_state_id ns = _state;
					if (set) {
						ns |= bit;
					} else {
						ns &= ~bit;
					}
					if (ns != _state) {
						set_state(ns);
					}
				}
				visual_state_id get_state() const {
					return _state;
				}
				bool test_state_bit(visual_state_id s) const {
					return test_bit_all(_state, s);
				}

				bool stationary() const {
					return _animst.timestamp == visual_manager::current_timestamp() && _animst.all_stationary;
				}
				void update(double dt) {
					if (!stationary()) {
						visual_provider_state &vps = visual_manager::get_provider_or_default(_cls).get_state_or_default(_state);
						if (_animst.timestamp != visual_manager::current_timestamp()) {
							_animst = vps.init_state();
						}
						vps.update(_animst, dt);
					}
				}
				void render(rectd rgn) {
					visual_provider_state &ps = visual_manager::get_provider_or_default(_cls).get_state_or_default(_state);
					if (_animst.timestamp != visual_manager::current_timestamp()) {
						_animst = ps.init_state();
					}
					ps.render(rgn, _animst);
				}
				bool update_and_render(double dt, rectd rgn) {
					update(dt);
					render(rgn);
					return !stationary();
				}
				bool update_and_render_multiple(double dt, const std::vector<rectd> &rgn) {
					update(dt);
					for (auto i = rgn.begin(); i != rgn.end(); ++i) {
						render(*i);
					}
					return !stationary();
				}
			protected:
				str_t _cls;
				visual_state_id _state = visual_manager::normal_state;
				visual_provider_state::state _animst;
			};

			visual_provider_state &get_state_or_default(visual_state_id s) {
				auto found = _states.find(s);
				if (found != _states.end()) {
					return found->second;
				}
				s = visual_manager::normal_state;
				return _states[s];
			}
			visual_provider_state &get_state_or_create(visual_state_id s) {
				return _states[s];
			}
		protected:
			std::map<visual_state_id, visual_provider_state> _states;
		};

		template <typename T> struct json_object_parser {
			static T parse(const json::value_t&);
		};
		template <> struct json_object_parser<colord> {
			inline static colord parse(const json::value_t &obj) {
				if (obj.IsArray()) {
					if (obj.Size() > 3 && obj[0].IsString() && str_t(obj[0].GetString()) == U"hsl") {
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
				anchor_top_char = U't', anchor_bottom_char = U'b',
				anchor_left_char = U'l', anchor_right_char = U'r';

			template <typename T> inline static void parse_animation(animation_params<T> &ani, const json::value_t &obj) {
				if (obj.IsObject()) {
					json::value_t::ConstMemberIterator mem;
					mem = obj.FindMember(U"to");
					if (mem != obj.MemberEnd()) {
						ani.to = json_object_parser<T>::parse(mem->value);
					} else {
						logger::get().log_warning(CP_HERE, "no \"to\" property found in animation");
					}
					mem = obj.FindMember(U"from");
					if (mem != obj.MemberEnd()) {
						ani.has_from = true;
						ani.from = json_object_parser<T>::parse(mem->value);
					}
					json::try_get(obj, U"auto_reverse", ani.auto_reverse);
					json::try_get(obj, U"repeat", ani.repeat);
					json::try_get(obj, U"duration", ani.duration);
					json::try_get(obj, U"reverse_duration_scale", ani.reverse_duration_scale);
					mem = obj.FindMember(U"transition");
					if (mem != obj.MemberEnd()) {
						if (mem->value.IsString()) {
							ani.transition_func = visual_manager::get_transition_function(json::get_as_string(mem->value));
						} else {
							logger::get().log_warning(CP_HERE, "invalid transition function");
						}
					}
				} else {
					ani.to = json_object_parser<T>::parse(obj);
				}
			}
			inline static void parse_animation(animation_params<os::texture> &ani, const json::value_t &obj) {
				using ani_t = animation_params<os::texture>;
				// TODO deferred texture loading
				bool good = true;
				if (obj.IsString()) {
					ani.frames.push_back(ani_t::texture_keyframe(
						std::make_shared<os::texture>(os::load_image(json::get_as_string(obj))), 0.0
					));
				} else if (obj.IsObject()) {
					auto fs = obj.FindMember(U"frames");
					if (fs != obj.MemberEnd()) {
						if (fs->value.IsArray()) {
							ani.frames.clear();
							double lastframetime = ani_t::default_frametime;
							auto beg = fs->value.Begin(), end = fs->value.End();
							for (auto i = beg; i != end; ++i) {
								if (i->IsArray()) {
									if ((*i).Size() >= 2 && (*i)[0].IsString() && (*i)[1].IsNumber()) {
										double frametime = (*i)[1].GetDouble();
										ani.frames.push_back(ani_t::texture_keyframe(std::make_shared<os::texture>(
											os::load_image(json::get_as_string((*i)[0]))
											), frametime));
										lastframetime = frametime;
									} else {
										good = false;
									}
								} else if (i->IsString()) {
									ani.frames.push_back(ani_t::texture_keyframe(std::make_shared<os::texture>(
										os::load_image(json::get_as_string(*i))
										), lastframetime));
								} else {
									good = false;
								}
							}
						} else {
							good = false;
						}
					}
					json::try_get(obj, U"auto_reverse", ani.auto_reverse);
					json::try_get(obj, U"repeat", ani.repeat);
					json::try_get(obj, U"reverse_duration_scale", ani.reverse_duration_scale);
				} else {
					good = false;
				}
				if (!good) {
					logger::get().log_warning(CP_HERE, "invalid texture animation format");
				}
			}
			inline static void parse_layer(visual_layer &layer, const json::value_t &val) {
				if (val.IsObject()) {
					str_t typestr;
					if (json::try_get(val, U"type", typestr)) {
						if (typestr == U"solid") {
							layer.layer_type = visual_layer::type::solid;
						} else {
							layer.layer_type = visual_layer::type::grid;
						}
					}
					_find_and_parse(val, U"texture", layer.texture_animation);
					_find_and_parse(val, U"color", layer.color_animation);
					_find_and_parse(val, U"size", layer.size_animation);
					_find_and_parse(val, U"margins", layer.margin_animation);
					str_t anc;
					if (json::try_get(val, U"anchor", anc)) {
						layer.rect_anchor = static_cast<anchor>(get_bitset_from_string<unsigned, anchor>({
							{U'l', anchor::left},
							{U't', anchor::top},
							{U'r', anchor::right},
							{U'b', anchor::bottom}
						}, anc));
					}
				} else if (val.IsString()) {
					layer = visual_layer();
					parse_animation(layer.texture_animation, val);
					return;
				} else {
					logger::get().log_warning(CP_HERE, "invalid layer info");
				}
			}
			inline static void parse_state(visual_provider_state &vs, const json::value_t &val) {
				if (val.IsArray()) {
					if (vs._layers.size() < val.Size()) {
						vs._layers.resize(val.Size());
					}
					for (size_t i = 0; i < val.Size(); ++i) {
						parse_layer(vs._layers[i], val[static_cast<rapidjson::SizeType>(i)]);
					}
				} else {
					logger::get().log_warning(CP_HERE, "state format incorrect");
				}
			}
			inline static void parse_provider(visual_provider &provider, const json::value_t &val) {
				if (val.IsArray()) {
					for (auto i = val.Begin(); i != val.End(); ++i) {
						visual_provider_state vps;
						visual_state_id id = visual_manager::normal_state;
						if (i->IsObject()) {
							json::value_t::ConstMemberIterator fmem;
							fmem = i->FindMember(U"states");
							if (fmem != i->MemberEnd()) {
								id = _parse_vid(fmem->value);
							}
							fmem = i->FindMember(U"inherit_from");
							if (fmem != i->MemberEnd()) {
								visual_state_id pid = _parse_vid(fmem->value);
								auto found = provider._states.find(pid);
								if (found != provider._states.end()) {
									vps = found->second;
								} else {
									logger::get().log_warning(CP_HERE, "invalid inheritance");
								}
							}
							fmem = i->FindMember(U"layers");
							if (fmem != i->MemberEnd()) {
								parse_state(vps, fmem->value);
							}
						} else {
							parse_state(vps, *i);
						}
						if (!provider._states.insert(std::make_pair(id, std::move(vps))).second) {
							logger::get().log_warning(CP_HERE, "state registration failed");
						}
					}
				} else {
					logger::get().log_warning(CP_HERE, "unrecognized skin format");
				}
			}
		protected:
			template <typename T> inline static void _find_and_parse(
				const json::value_t &val, const str_t &s, animation_params<T> &p
			) {
				auto found = val.FindMember(s.c_str());
				if (found != val.MemberEnd()) {
					parse_animation(p, found->value);
				}
			}
			inline static visual_state_id _parse_vid(const json::value_t &val) {
				if (!val.IsArray()) {
					return visual_manager::normal_state;
				}
				visual_state_id id = visual_manager::normal_state;
				for (auto j = val.Begin(); j != val.End(); ++j) {
					id |= visual_manager::get_state_id(json::get_as_string(*j));
				}
				return id;
			}
		};

		inline visual_provider_state::state visual_provider_state::init_state() const {
			state res;
			res.timestamp = visual_manager::current_timestamp();
			for (auto i = _layers.begin(); i != _layers.end(); ++i) {
				res.layer_states.push_back(i->init_state());
			}
			return res;
		}
		inline visual_provider_state::state visual_provider_state::init_state(const state &old) const {
			state res;
			res.timestamp = visual_manager::current_timestamp();
			auto i = _layers.begin();
			for (
				auto j = old.layer_states.begin();
				i != _layers.end() && j != old.layer_states.end();
				++i, ++j
				) {
				res.layer_states.push_back(i->init_state(*j));
			}
			for (; i != _layers.end(); ++i) {
				res.layer_states.push_back(i->init_state());
			}
			return res;
		}
		inline void visual_provider_state::update(state &s, double dt) const {
			if (s.timestamp != visual_manager::current_timestamp()) {
				s = init_state();
			}
			s.all_stationary = true;
			auto j = s.layer_states.begin();
			for (auto i = _layers.begin(); i != _layers.end(); ++i, ++j) {
				assert_true_usage(j != s.layer_states.end(), "invalid layer state data");
				i->update(*j, dt);
				if (!j->all_stationary) {
					s.all_stationary = false;
				}
			}
		}

		inline void visual_manager::load_config(const json::value_t &val) {
			_registration &reg = _get_table();
			++reg.timestamp;
			reg.providers.clear();
			for (auto i = val.MemberBegin(); i != val.MemberEnd(); ++i) {
				visual_provider vp;
				visual_json_parser::parse_provider(vp, i->value);
				assert_true_usage(reg.providers.insert(
					std::make_pair(json::get_as_string(i->name), std::move(vp))
				).second, "visual provider registration failed");
			}
		}

		inline visual_provider &visual_manager::_registration::get_provider_or_default(const str_t &cls) {
			auto found = providers.find(cls);
			if (found != providers.end()) {
				return found->second;
			}
			return providers[str_t()];
		}
	}

	template <> inline ui::thickness lerp<ui::thickness>(ui::thickness from, ui::thickness to, double perc) {
		return ui::thickness(
			lerp(from.left, to.left, perc),
			lerp(from.top, to.top, perc),
			lerp(from.right, to.right, perc),
			lerp(from.bottom, to.bottom, perc)
		);
	}
}
