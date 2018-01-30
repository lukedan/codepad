#pragma once

#include <unordered_map>
#include <memory>

#include "../os/filesystem.h"
#include "draw.h"

namespace codepad {
	namespace ui {
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

			T from{}, to{};
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
		class visual_state {
			friend class visual_json_parser;
		public:
			using id_t = std::uint32_t;

			constexpr static id_t normal_id = 0;

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

		class visual {
			friend struct globals;
			friend class visual_json_parser;
		public:
			struct predefined_states {
				visual_state::id_t mouse_over, mouse_down, focused, corpse;
			};
			struct render_state {
			public:
				void set_class(str_t);
				const str_t &get_class() const {
					return _cls;
				}

				void set_state(visual_state::id_t);
				void set_state_bit(visual_state::id_t bit, bool set) {
					visual_state::id_t ns = _state;
					if (set) {
						ns |= bit;
					} else {
						ns &= ~bit;
					}
					if (ns != _state) {
						set_state(ns);
					}
				}
				visual_state::id_t get_state() const {
					return _state;
				}
				bool test_state_bit(visual_state::id_t s) const {
					return test_bit_all(_state, s);
				}

				bool stationary() const;
				void update();
				void render(rectd);
				bool update_and_render(rectd rgn) {
					update();
					render(rgn);
					return !stationary();
				}
				bool update_and_render_multiple(const std::vector<rectd> &rgn) {
					update();
					for (auto i = rgn.begin(); i != rgn.end(); ++i) {
						render(*i);
					}
					return !stationary();
				}
			protected:
				str_t _cls;
				visual_state::state _animst;
				std::chrono::high_resolution_clock::time_point _last;
				visual_state::id_t _state = visual_state::normal_id;

				void _set_animst_and_last(visual_state::state s) {
					_animst = std::move(s);
					_last = std::chrono::high_resolution_clock::now();
				}
			};

			const visual_state &get_state_or_default(visual_state::id_t s) {
				auto found = _states.find(s);
				if (found != _states.end()) {
					return found->second;
				}
				return _states[visual_state::normal_id];
			}
			visual_state &get_state_or_create(visual_state::id_t s) {
				return _states[s];
			}

			inline static visual_state::id_t get_or_register_state_id(const str_t &name) {
				return _registry::get().get_or_register_state_id(name);
			}
			inline static const predefined_states &get_predefined_states() {
				return _registry::get().predef_states;
			}
			inline static const std::function<double(double)> *try_get_transition_func(const str_t &name) {
				auto &mapping = _registry::get().transition_func_mapping;
				auto it = mapping.find(name);
				if (it != mapping.end()) {
					return &it->second;
				}
				return nullptr;
			}
		protected:
			struct _registry {
				_registry() {
					predef_states.mouse_over = get_or_register_state_id(CP_STRLIT("mouse_over"));
					predef_states.mouse_down = get_or_register_state_id(CP_STRLIT("mouse_down"));
					predef_states.focused = get_or_register_state_id(CP_STRLIT("focused"));
					predef_states.corpse = get_or_register_state_id(CP_STRLIT("corpse"));

					transition_func_mapping.insert({CP_STRLIT("linear"), linear_transition_func});
					transition_func_mapping.insert({CP_STRLIT("concave_quadratic"), concave_quadratic_transition_func});
					transition_func_mapping.insert({CP_STRLIT("convex_quadratic"), convex_quadratic_transition_func});
					transition_func_mapping.insert({CP_STRLIT("smoothstep"), smoothstep_transition_func});
				}

				std::map<str_t, std::function<double(double)>> transition_func_mapping;
				std::map<str_t, visual_state::id_t> state_id_mapping;
				std::map<visual_state::id_t, str_t> state_name_mapping; // TODO is this necessary?
				predefined_states predef_states;
				int id_allocation = 0;

				visual_state::id_t get_or_register_state_id(const str_t &name) {
					auto found = state_id_mapping.find(name);
					if (found == state_id_mapping.end()) {
						logger::get().log_info(CP_HERE, "registering state: ", convert_to_utf8(name));
						visual_state::id_t res = 1 << id_allocation;
						++id_allocation;
						state_id_mapping[name] = res;
						state_name_mapping[res] = name;
						return res;
					}
					return found->second;
				}

				static _registry &get();
			};

			std::map<visual_state::id_t, visual_state> _states;
		};
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
