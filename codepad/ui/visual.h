#pragma once

/// \file
/// Classes for defining and rendering the visuals of elements.

#include <map>
#include <unordered_map>
#include <memory>

#include "../os/filesystem.h"
#include "../os/renderer.h"
#include "../core/misc.h"

namespace codepad::ui {
	using element_state_id = std::uint_least32_t; ///< Bitsets that represents states of an \ref element.
	constexpr static element_state_id normal_element_state_id = 0; ///< Represents the default (normal) state.
	/// Represents a margin, a padding, etc.
	struct thickness {
		/// Constructs the struct with the same value for all four sides.
		constexpr explicit thickness(double uni = 0.0) : left(uni), top(uni), right(uni), bottom(uni) {
		}
		/// Constructs the struct with the given values for the four sides.
		constexpr thickness(double l, double t, double r, double b) : left(l), top(t), right(r), bottom(b) {
		}

		double
			left, ///< The length on the left side.
			top, ///< The length on the top side.
			right, ///< The length on the right side.
			bottom; ///< The length on the bottom side.

		/// Enlarges the given rectangle with the lengths of the four sides.
		constexpr rectd extend(rectd r) const {
			return rectd(r.xmin - left, r.xmax + right, r.ymin - top, r.ymax + bottom);
		}
		/// Shrinks the given rectangle with the lengths of the four sides.
		constexpr rectd shrink(rectd r) const {
			return rectd(r.xmin + left, r.xmax - right, r.ymin + top, r.ymax - bottom);
		}

		/// Returns the total horizontal length.
		constexpr double width() const {
			return left + right;
		}
		/// Returns the total vertical length.
		constexpr double height() const {
			return top + bottom;
		}
		/// Returns the vector composed of the total horizontal length and the total vertical length.
		constexpr vec2d size() const {
			return vec2d(width(), height());
		}
	};
	/// Used to specify to which sides an object is anchored.
	/// If an object is anchored to a side, then the distance between the borders
	/// of the object and its container is kept to be the specified value.
	enum class anchor : unsigned char {
		none = 0, ///< The object is not anchored to any side.

		left = 1, ///< The object is anchored to the left side.
		top = 2, ///< The object is anchored to the top side.
		right = 4, ///< The object is anchored to the right side.
		bottom = 8, ///< The object is anchored to the bottom side.

		top_left = top | left, ///< The object is anchored to the top side and the left side.
		top_right = top | right, ///< The object is anchored to the top side and the right side.
		bottom_left = bottom | left, ///< The object is anchored to the bottom side and the left side.
		bottom_right = bottom | right, ///< The object is anchored to the bottom side and the right side.

		stretch_horizontally = left | right, ///< The object is anchored to the left side and the right side.
		stretch_vertically = top | bottom, ///< The object is anchored to the top side and the bottom side.

		dock_left = stretch_vertically | left, ///< The object is anchored to all but the right side.
		dock_top = stretch_horizontally | top, ///< The object is anchored to all but the bottom side.
		dock_right = stretch_vertically | right, ///< The object is anchored to all but the left side.
		dock_bottom = stretch_horizontally | bottom, ///< The object is anchored to all but the top side.

		all = left | top | right | bottom ///< The object is anchored to all four sides.
	};
	/// Determines how size is allocated to each \ref element.
	enum class size_allocation_type : unsigned char {
		automatic, ///< The size is determined by \ref element::get_desired_size.
		fixed, ///< The user specifies the size in pixels.
		proportion ///< The user specifies the size as a proportion.
	};
	/// Represents an orientation.
	enum class orientation {
		horizontal, ///< Horizontal.
		vertical ///< Vertical.
	};

	/// Transition functions used in animations.
	namespace transition_functions {
		/// The linear transition function.
		inline double linear(double v) {
			return v;
		}

		/// The smoothstep transition function.
		inline double smoothstep(double v) {
			return v * v * (3.0 - 2.0 * v);
		}

		/// The concave quadratic transition function.
		inline double concave_quadratic(double v) {
			return v * v;
		}
		/// The convex quadratic transition function.
		inline double convex_quadratic(double v) {
			v = 1.0 - v;
			return 1.0 - v * v;
		}

		/// The concave cubic transition function.
		inline double concave_cubic(double v) {
			return v * v * v;
		}
		/// The convex cubic transition function.
		inline double convex_cubic(double v) {
			v = 1.0 - v;
			return 1.0 - v * v * v;
		}
	}

	class visual_json_parser;

	/// A property of a \ref visual_layer that can be animated. This only stores the
	/// parameters of the animation; the actual animating process is done on \ref state.
	/// After the animation is over, the value stays at \ref to.
	///
	/// \tparam T The type of the underlying value.
	template <typename T> struct animated_property {
		/// Represents a state of the animated property.
		struct state {
			/// Default constructor.
			state() = default;
			/// Constructs the state of a given animated property, with a given initial value
			/// that's taken if animated_property::has_from is \p false.
			state(const animated_property &prop, const T &curv) :
				from(prop.has_from ? prop.from : curv), current_value(from) {
			}
			/// Constructs the state of a given animated property with no initial value.
			explicit state(const animated_property &prop) : state(prop, prop.from) {
			}

			T
				/// The start value. It's equal to animated_property::from if animated_property::has_from
				/// is \p true or the state is created with init_state() const instead of init_state(T) const.
				/// Otherwise it's equal to the value passed to init_state(T) const.
				from,
				current_value; ///< The current value.
			/// The elapsed time since the animation started or last repeated.
			double current_time_warped = 0.0;
			bool stationary = false; ///< Marks whether the animation has finished.
		};

		T
			from{}, ///< The initial value of the animation.
			to{}; ///< The final value of the animation.
		bool
			has_from = false, ///< Whether the animation should always start from \ref from.
			/// Whether the animatiou should automatically play backwards to the beginning after
			/// reaching \ref to. Note that if \ref has_from is \p false it may not end at \ref from.
			auto_reverse = false,
			repeat = false; ///< Whether the animation should repeat itself after it's finished.
		double
			/// The time it should take for the animated value to change to \ref to for the first time.
			duration = 0.0,
			/// The proportion that the duration is scaled by when the animation is played backwards.
			reverse_duration_scale = 1.0;
		/// The transition function used to control the process of the animation.
		/// This function should accept a double in the range of [0, 1] and return a double in the same range.
		/// The input indicates the current process of the animation, and the output is used to linearly
		/// interpolate the current value between \ref from and \ref to.
		std::function<double(double)> transition_func = transition_functions::linear;

		/// Updates the given state.
		///
		/// \param s The state to be updated.
		/// \param dt The time since the state was last updated.
		void update(state &s, double dt) const {
			if (!s.stationary) {
				s.current_time_warped += dt;
				double period = duration; // the period
				if (auto_reverse) { // add reverse portion to the period
					period += duration * reverse_duration_scale;
				}
				if (s.current_time_warped >= period) {
					if (repeat) { // start new period
						s.current_time_warped = std::fmod(s.current_time_warped, period);
					} else { // the animation is over
						s.current_value = to;
						s.stationary = true; // no need to update anymore
						return;
					}
				}
				double progress = 0.0; // value passed to transition_func
				if (s.current_time_warped < duration) { // forward
					progress = s.current_time_warped / duration;
				} else { // backwards
					progress = 1.0 - (s.current_time_warped - duration) / (duration * reverse_duration_scale);
				}
				s.current_value = lerp(s.from, to, transition_func(progress));
			}
		}
	};
	/// Specialization of \ref animated_property for \ref os::texture "textures".
	/// The multiple supplied textures are displayed in order, each for its specified duration.
	/// After the animation is over, the current image is kept to be the last image displayed.
	template <> struct animated_property<os::texture> {
		/// The default time an image is displayed if no duration is specified.
		constexpr static double default_frametime = 1.0 / 30.0;

		/// A frame. Contains an image and the duration it's displayed.
		using texture_keyframe = std::pair<std::shared_ptr<os::texture>, double>;
		/// Represents a state of the animated property.
		struct state {
			/// Default constructor.
			state() = default;
			/// Constructs the state from a given property, setting the current frame as the first one of its
			/// frames. If there are no frames in the property, a solid rectangle filling the area is displayed.
			explicit state(const animated_property &prop) : current_frame(prop.frames.begin()) {
			}

			/// Iterator to the current frame. \p end() of the container if it's empty.
			std::vector<texture_keyframe>::const_iterator current_frame;
			double current_frame_time = 0.0; ///< The time since the current frame has been displayed.
			bool
				reversing = false, ///< Marks whether the animation is currently playing in reverse.
				stationary = false; ///< Marks whether the animation has finished.
		};

		std::vector<texture_keyframe> frames; ///< The list of frames and frametimes.
		bool
			/// Whether the animatiou should automatically play backwards to the beginning after
			/// playing forward to the end.
			auto_reverse = false,
			repeat = false; ///< Whether the animation should repeat after ending.
		/// The proportion that the frame times are scaled by when the animation is played backwards.
		double reverse_duration_scale = 1.0;

		/// Returns the sum of all frame times.
		double duration() const {
			double res = 0.0;
			for (auto i = frames.begin(); i != frames.end(); ++i) {
				res += i->second;
			}
			return res;
		}
		/// Updates the given state.
		///
		/// \param s The state to be updated.
		/// \param dt The time since the state was last updated.
		void update(state &s, double dt) const {
			if (s.current_frame == frames.end()) {
				s.stationary = true;
			}
			if (s.stationary) {
				return;
			}
			s.current_frame_time += dt;
			while (true) {
				if (s.reversing) { // backwards
					double frametime = s.current_frame->second * reverse_duration_scale;
					if (s.current_frame_time < frametime) { // current frame not finished
						break;
					}
					s.current_frame_time -= frametime;
					if (s.current_frame == frames.begin()) { // at the end
						if (!repeat) { // no more animation
							s.stationary = true;
							break;
						}
						s.reversing = false;
					} else {
						--s.current_frame;
					}
				} else { // forward
					if (s.current_frame_time < s.current_frame->second) { // current frame not finished
						break;
					}
					s.current_frame_time -= s.current_frame->second;
					++s.current_frame;
					if (s.current_frame == frames.end()) { // at the end
						if (auto_reverse) {
							s.reversing = true;
							--s.current_frame;
						} else if (repeat) {
							s.current_frame = frames.begin();
						} else { // no more animation
							s.stationary = true;
							--s.current_frame; // roll back to the last frame
							break;
						}
					}
				}
			}
		}
	};
	/// A layer in the rendering of objects.
	struct visual_layer {
		/// The state of a layer. Contains the states of all its \ref animated_property "animated properties".
		struct state {
		public:
			/// Default constructor.
			state() = default;
			/// Initializes all property states with the properties of the given layer.
			explicit state(const visual_layer &layer) :
				current_texture(layer.texture_animation), current_color(layer.color_animation),
				current_size(layer.size_animation), current_margin(layer.margin_animation) {
			}
			/// Initializes all property states with the properties of the layer and the previous state.
			state(const visual_layer &layer, const state &old) :
				current_texture(layer.texture_animation),
				current_color(layer.color_animation, old.current_color.current_value),
				current_size(layer.size_animation, old.current_size.current_value),
				current_margin(layer.margin_animation, old.current_margin.current_value) {
			}

			animated_property<os::texture>::state current_texture; ///< The state of the texture.
			animated_property<colord>::state current_color; ///< The state of the color.
			animated_property<vec2d>::state current_size; ///< The state of the size.
			animated_property<thickness>::state current_margin; ///< The stzte of the margin.
			bool all_stationary = false; ///< Marks if all states are stationary.
		};
		/// The type of a layer.
		enum class type {
			solid, ///< The contents are rendered as a single solid block.
			/// The contents are divided by a grid specified by \ref margin_animation and \ref rect_anchor.
			/// \ref margin_animation is treated as if it were in pixels, which corresponds to pixels in the
			/// textures specified by \ref texture_animation.
			grid
		};

		/// Returns the center region of the layout, calculated from the given rectangle.
		/// If \ref layer_type is type::solid, then the rectangle returned contains the coordinates
		/// where the texture is drawn. Otherwise, the rectangle is the part where part of the texture
		/// is stretched to fill.
		///
		/// \param s The state of the layer.
		/// \param rgn The region of the layout.
		rectd get_center_rect(const state &s, rectd rgn) const;
		/// Updates the given state.
		///
		/// \param s The state to be updated.
		/// \param dt The time since the state was last updated.
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
		/// Renders an object with the given layout and state.
		///
		/// \param rgn The layout of the object.
		/// \param s The current state of the layer.
		void render(rectd rgn, const state &s) const;

		animated_property<os::texture> texture_animation; ///< Textures(s) used to render the layer.
		animated_property<colord> color_animation; ///< The color of the layer.
		animated_property<vec2d> size_animation; ///< The size used to calculate the center region.
		animated_property<thickness> margin_animation; ///< The margin used to calculate the center region.
		anchor rect_anchor = anchor::all; ///< The anchor of the center region.
		size_allocation_type
			/// The allocation type of the width of \ref size_animation.
			width_alloc = size_allocation_type::automatic,
			/// The allocation type of the height of \ref size_animation.
			height_alloc = size_allocation_type::automatic;
		type layer_type = type::solid; ///< Determines how the center region is handled.
	};
	/// Stores all layers of an object's visual in a certain state.
	class visual_state {
	public:
		/// The state of the visual of an object. Stores the states of all layers, and
		/// additional information to determine if any layer needs to be updated.
		struct state {
			/// Default constructor.
			state() = default;
			/// Initializes the state with the given visual state. All layer states will be initialized
			/// with corresponding layers of the visual state.
			state(const visual_state&);
			/// Initializes the state with the visual state and the previous state. All layer states will
			/// be initialized with corresponding layers of the visual state and layer states in the
			/// previous state. The number of resulting layer states will always be equal to the number of
			/// layers in the visual state. If the previous state has less layers then remaining layer
			/// states will be initialized without a layer state, and if the previous state has more layers
			/// the redundant layers in the end will be ignored.
			state(const visual_state&, const state&);

			std::vector<visual_layer::state> layer_states; ///< The states of all layers.
			bool all_stationary = false; ///< Indicates whether all animations have finished.
		};

		/// Updates the given state.
		///
		/// \param s The state to be updated.
		/// \param dt The time since the state was last updated.
		void update(state &s, double dt) const;
		void render(rectd rgn, const state &s) const {
			auto j = s.layer_states.begin();
			for (auto i = layers.begin(); i != layers.end(); ++i, ++j) {
				assert_true_usage(j != s.layer_states.end(), "invalid layer state data");
				i->render(rgn, *j);
			}
		}

		std::vector<visual_layer> layers; ///< All layers of the visual state.
	};

	/// Represents the visual of a certain object in all possible states.
	class visual {
		friend class visual_json_parser;
	public:
		/// Contains information about the visual state of an object.
		struct render_state {
		public:
			/// Sets the class of the object. The class is used to determine which
			/// \ref visual determines the look of the element.
			void set_class(str_t);
			/// Returns the class of the object.
			const str_t &get_class() const {
				return _cls;
			}

			/// Sets the state of the object. The object can have different looks in
			/// different states, and it can have animations to transition between them.
			/// The state of the object is represented by a set of bits, each of which
			/// represents one particular state.
			void set_state(element_state_id);
			/// Sets the status of a certain bit of the object's state. Calls \ref set_state
			/// if the current state is different from the state with the desired change(s).
			///
			/// \param bit Contains the bit(s) of states that are to be turned on or off.
			/// \param set Indicates whether the bits should be turned on or off.
			/// \return Whether \ref set_state has been called because the new state with
			///         the bits turned on or off is different from the previous one.
			bool set_state_bit(element_state_id bit, bool set) {
				element_state_id ns = _state;
				if (set) {
					ns |= bit;
				} else {
					ns &= ~bit;
				}
				if (ns != _state) {
					set_state(ns);
					return true;
				}
				return false;
			}
			/// Tests if the current state has all specified state bits.
			bool test_state_bits(element_state_id bits) const {
				return test_bit_all(_state, bits);
			}
			/// Returns the state of the object.
			element_state_id get_state() const {
				return _state;
			}

			/// Returns whether all animations have finished.
			bool stationary() const;
			/// Updates the animations.
			void update();
			/// Renders an object with the current state, in the given region.
			void render(rectd);
			/// Updates the animations and then renders an object in the given region.
			///
			/// \return Whether all animations have finished.
			bool update_and_render(rectd rgn) {
				update();
				render(rgn);
				return !stationary();
			}
			/// Updates the animations and then renders multiple objects in the given regions.
			///
			/// \return Whether all animations have finished.
			bool update_and_render_multiple(const std::vector<rectd> &rgn) {
				update();
				for (auto i = rgn.begin(); i != rgn.end(); ++i) {
					render(*i);
				}
				return !stationary();
			}
		protected:
			str_t _cls; ///< The class of the visual.
			visual_state::state _animst; ///< The state of the objects' looks.
			/// The time when this render state was last updated.
			std::chrono::high_resolution_clock::time_point _last;
			element_state_id _state = normal_element_state_id; ///< The state ID of the visual.
			/// Indicates the `version' of the visual config that this state is created with.
			unsigned _timestamp = 0;

			/// Sets \ref _animst to the given state, and \ref _timestamp and \ref _last to the current value.
			void _reset_state(visual_state::state);
		};

		/// Returns the \ref visual_state corresponding to the given \ref element_state_id. If none is registered for
		/// the id, one corresponding to visual_state::normal_id is created (if one doesn't exist) and returned.
		const visual_state &get_state_or_default(element_state_id s) {
			auto found = _states.find(s);
			if (found != _states.end()) {
				return found->second;
			}
			return _states[normal_element_state_id];
		}
		/// Returns the \ref visual_state corresponding to the given visual state id.
		/// If none is registered for the id, an empty one is created.
		visual_state &get_state_or_create(element_state_id s) {
			return _states[s];
		}
	protected:
		/// The mapping from all state IDs to their corresponding \ref visual_state "visual_states".
		std::map<element_state_id, visual_state> _states;
	};
}

namespace codepad {
	/// Specialization for ui::thickness since it doesn't support arithmetic operators.
	template <> inline ui::thickness lerp<ui::thickness>(ui::thickness from, ui::thickness to, double perc) {
		return ui::thickness(
			lerp(from.left, to.left, perc),
			lerp(from.top, to.top, perc),
			lerp(from.right, to.right, perc),
			lerp(from.bottom, to.bottom, perc)
		);
	}
}
