// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Fundamental types and variables of the user interface.

#include <cstdint>
#include <charconv>
#include <chrono>

#include "../core/misc.h"
#include "../core/json.h"
#include "renderer.h"

namespace codepad::ui {
	/// Contains a collection of specializations of \ref parse() that parse objects from JSON objects.
	namespace json_object_parsers {
		/// The standard parser interface.
		template <typename T> T parse(const json::node_t&);

		/// Parses \ref vec2d. The object must be of the following format: <tt>[x, y]</tt>
		template <> inline vec2d parse<vec2d>(const json::node_t &obj) {
			if (obj.IsArray() && obj.Size() > 1) {
				return vec2d(obj[0].GetDouble(), obj[1].GetDouble());
			}
			logger::get().log_warning(CP_HERE, "invalid vec2 representation");
			return vec2d();
		}
		/// Parses \ref colord. The object can take the following formats: <tt>["hsl", h, s, l(, a)]</tt> for
		/// HSL format colors, and <tt>[r, g, b(, a)]</tt> for RGB format colors.
		template <> inline colord parse<colord>(const json::node_t &obj) {
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
				}
				if (obj.Size() == 3) {
					return colord(
						obj[0].GetDouble(), obj[1].GetDouble(), obj[2].GetDouble(), 1.0
					);
				}
			}
			logger::get().log_warning(CP_HERE, "invalid color representation");
			return colord();
		}
		/// Parses a duration from a number. The unit is seconds. Note that this function has a different name, so
		/// each duration type has to implement their own \p parse() function.
		///
		/// \todo Also accept string representations.
		template <typename Rep, typename Period> inline std::chrono::duration<Rep, Period> parse_duration(
			const json::node_t &val
		) {
			using _res_t = std::chrono::duration<Rep, Period>;

			if (val.IsNumber()) {
				return std::chrono::duration_cast<_res_t>(std::chrono::duration<double>(val.GetDouble()));
			}
			logger::get().log_warning(CP_HERE, "invalid duration representation");
			return _res_t();
		}
	}

	/// The style of the cursor.
	enum class cursor {
		normal, ///< The normal cursor.
		busy, ///< The `busy' cursor.
		crosshair, ///< The `crosshair' cursor.
		hand, ///< The `hand' cursor, usually used to indicate a link.
		help, ///< The `help' cursor.
		text_beam, ///< The `I-beam' cursor, usually used to indicate an input field.
		denied, ///< The `denied' cursor.
		arrow_all, ///< A cursor with arrows to all four directions.
		arrow_northeast_southwest, ///< A cursor with arrows to the top-right and bottom-left directions.
		arrow_north_south, ///< A cursor with arrows to the top and bottom directions.
		arrow_northwest_southeast, ///< A cursor with arrows to the top-left and bottom-right directions.
		arrow_east_west, ///< A cursor with arrows to the left and right directions.
		invisible, ///< An invisible cursor.

		not_specified ///< An unspecified cursor.
	};

	/// Represents a button of the mouse.
	enum class mouse_button {
		primary, ///< The primary button. For the right-handed layout, this is the left button.
		tertiary, ///< The middle button.
		secondary ///< The secondary button. For the right-handed layout, this is the right button.
	};
	/// Represents a key on the keyboard.
	///
	/// \todo Document all keys, add support for more keys (generic super key, symbols, etc.).
	enum class key {
		cancel,
		xbutton_1, xbutton_2,
		backspace, ///< The `backspace' key.
		tab, ///< The `tab' key.
		clear,
		enter, ///< The `enter' key.
		shift, ///< The `shift' key.
		control, ///< The `control' key.
		alt, ///< The `alt' key.
		pause,
		capital_lock, ///< The `caps lock' key.
		escape, ///< The `escape' key.
		convert,
		nonconvert,
		space, ///< The `space' key.
		page_up, ///< The `page up' key.
		page_down, ///< The `page down' key.
		end, ///< The `end' key.
		home, ///< The `home' key.
		left, ///< The left arrow key.
		up, ///< The up arrow key.
		right, ///< The right arrow key.
		down, ///< The down arrow key.
		select,
		print,
		execute,
		snapshot,
		insert, ///< The `insert' key.
		del, ///< The `delete' key.
		help,
		a, b, c, d, e, f, g, h, i, j, k, l, m,
		n, o, p, q, r, s, t, u, v, w, x, y, z,
		left_super, ///< The left `super' (win) key.
		right_super, ///< The right `super' (win) key.
		apps,
		sleep,
		multiply, ///< The `multiply' key.
		add, ///< The `add' key.
		separator, ///< The `separator' key.
		subtract, ///< The `subtract' key.
		decimal, ///< The `decimal' key.
		divide, ///< The `divide' key.
		f1, f2, f3, f4,
		f5, f6, f7, f8,
		f9, f10, f11, f12,
		num_lock, ///< The `num lock' key.
		scroll_lock, ///< The `scroll lock' key.
		left_shift, ///< The left `shift' key.
		right_shift, ///< The right `shift' key.
		left_control, ///< The left `control' key.
		right_control, ///< The right `control' key.
		left_alt, ///< The left `alt' key.
		right_alt, ///< The right `alt' key.

		max_value ///< The total number of supported keys.
	};
	/// The total number of supported keys.
	constexpr size_t total_num_keys = static_cast<size_t>(key::max_value);

	using element_state_id = std::uint_fast32_t; ///< Bitsets that represents states of an \ref element.
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
	namespace json_object_parsers {
		/// Parses \ref thickness. The object can take the following formats:
		/// <tt>[left, top, right, bottom]</tt> or a single number specifying the value for all four
		/// directions.
		template <> inline thickness parse<thickness>(const json::node_t &obj) {
			if (obj.IsArray() && obj.Size() > 3) {
				return thickness(
					obj[0].GetDouble(), obj[1].GetDouble(),
					obj[2].GetDouble(), obj[3].GetDouble()
				);
			}
			if (obj.IsNumber()) {
				return thickness(obj.GetDouble());
			}
			logger::get().log_warning(CP_HERE, "invalid thickness representation");
			return thickness();
		}
	}
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

namespace codepad::ui {
	/// Contains information about how size should be allocated on a certain orientation for an \ref element.
	struct size_allocation {
		/// Default constructor.
		size_allocation() = default;
		/// Initializes all fields of this struct.
		size_allocation(double v, bool px) : value(v), is_pixels(px) {
		}

		double value = 0.0; ///< The value.
		bool is_pixels = false; ///< Indicates whether \ref value is in pixels instead of a proportion.
	};
	namespace json_object_parsers {
		/// ends_with.
		///
		/// \todo Use STL implementation after C++20.
		namespace _details {
			/// Determines if \p full ends with \p patt.
			inline bool ends_with(str_view_t full, str_view_t patt) {
				if (patt.size() > full.size()) {
					return false;
				}
				for (auto i = patt.rbegin(), j = full.rbegin(); i != patt.rend(); ++i, ++j) {
					if (*i != *j) {
						return false;
					}
				}
				return true;
			}
		}
		/// Parses a \ref size_allocation. The object can either be a full representation of the struct with two
		/// fields, a single number in pixels, or a string that optionally ends with `*', `%', or `px', the former
		/// two of which indicates that the value is a proportion. If the string ends with `%', the value is
		/// additionally divided by 100, thus using `%' and `*' mixedly is not recommended.
		template <> inline size_allocation parse<size_allocation>(const json::node_t &obj) {
			if (obj.IsObject()) { // full object representation
				size_allocation alloc;
				json::try_get(obj, "value", alloc.value);
				json::try_get(obj, "is_pixels", alloc.is_pixels);
				return alloc;
			}
			if (obj.IsNumber()) { // number in pixels
				return size_allocation(obj.GetDouble(), true);
			}
			if (obj.IsString()) {
				auto view = json::get_as_string_view(obj);
				size_allocation alloc(0.0, true); // in pixels if no suffix is present
				bool percentage = _details::ends_with(view, "%"); // the value is additionally divided by 100
				if (percentage || _details::ends_with(view, "*")) { // proportion
					alloc.is_pixels = false;
				}
				std::from_chars(view.data(), view.data() + view.size(), alloc.value); // result ignored, 0 by default
				if (percentage) {
					alloc.value *= 0.01;
				}
				return alloc;
			}
			return size_allocation();
		}
	}

	/// Used to specify to which sides an object is anchored. If an object is anchored to a side, then the distance
	/// between the borders of the object and its container is kept to be the value specified in the element's
	/// margin. Otherwise, the margin value is treated as a proportion.
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
	namespace json_object_parsers {
		/// Parses \ref anchor. The object can be a string that contains any combination of characters `l', `t',
		/// `r', and `b', standing for \ref anchor::left, \ref anchor::top, \ref anchor::right, and
		/// \ref anchor::bottom, respectively
		template <> inline anchor parse<anchor>(const json::node_t &obj) {
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
	}
}
namespace codepad {
	/// Enables bitwise operators for \ref ui::anchor.
	template<> struct enable_enum_bitwise_operators<ui::anchor> : std::true_type {
	};
}

namespace codepad::ui {
	/// Determines how size is allocated to an \ref codepad::ui::element.
	enum class size_allocation_type : unsigned char {
		/// The size is determined by \ref element::get_desired_width() and \ref element::get_desired_height().
		automatic,
		fixed, ///< The user specifies the size in pixels.
		proportion ///< The user specifies the size as a proportion.
	};
	namespace json_object_parsers {
		/// Parses \ref size_allocation_type. Checks if the given object (which must be a string) is one of the
		/// constants and returns the corresponding value. If none matches, \ref size_allocation_type::automatic is
		/// returned.
		template <> inline size_allocation_type parse<size_allocation_type>(const json::node_t &obj) {
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
	}

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

	/// Type of the transition function used to control the process of a \ref animated_property. This function
	/// accepts a double in the range of [0, 1] and return a double in the same range. The input indicates the
	/// current process of the animation, and the output is used to linearly interpolate the current value between
	/// \ref animated_property::from and \ref animated_property::to.
	using transition_function = std::function<double(double)>;

	/// Used to interpolate between values using \ref lerp().
	template <typename T> struct default_lerp {
		/// Calls \ref lerp().
		T operator()(T from, T to, double perc) const {
			return lerp(from, to, perc);
		}
	};
	/// Returns the target value without interpolating.
	struct no_lerp {
		/// Returns \p to.
		template <typename T> T operator()(T, T to, double) const {
			return to;
		}
	};
	using animation_clock_t = std::chrono::high_resolution_clock; ///< Type of the clock used for animation updating.
	using animation_time_point_t = animation_clock_t::time_point; ///< Represents a time point in an animation.
	using animation_duration_t = animation_clock_t::duration; ///< Represents a duration in an animation.
	namespace json_object_parsers {
		/// Parses a \ref animation_duration_t by calling \ref parse_duration().
		template <> inline animation_duration_t parse<animation_duration_t>(const json::node_t &obj) {
			return parse_duration<animation_duration_t::rep, animation_duration_t::period>(obj);
		}
	}
	/// A property that can be animated. Each property consists of multiple key frames with corresponding
	/// target values, durations, and transition functions.
	template <typename T, typename Lerp = default_lerp<T>> struct animated_property {
	public:
		/// The maximum number of key frames to advance per update. This is to prevent repeating key frames with zero
		/// duration from locking up the program.
		constexpr static size_t maximum_frames_per_update = 1000;

		/// A key frame.
		struct key_frame {
			/// Default constructor.
			key_frame() = default;
			/// Initializes all fields of this struct.
			explicit key_frame(
				T tar, animation_duration_t dur = animation_duration_t::zero(), transition_function func = nullptr
			) : target(tar), duration(dur), transition_func(std::move(func)) {
			}

			T target{}; ///< The target value.
			/// The duration of this key frame, i.e., the time after the last key frame and before this key frame.
			animation_duration_t duration{0};
			/// The transition function. If this is \p nullptr, then the animation will immediately reach \ref target
			/// value at this \ref key_frame.
			transition_function transition_func;
		};
		/// Tracks an ongoing animation.
		struct state {
			/// Default constructor.
			state() = default;
			/// Initializes this state using a custom starting value.
			state(const T &start, animation_time_point_t now) :
				from(start), current_value(start), key_frame_start(now) {
			}
			/// Initializes this state using \ref default_from_value as the starting value.
			state(const animated_property &prop, animation_time_point_t now) : state(prop.default_from_value, now) {
			}

			T
				from, ///< The value of the last key frame, or the original value.
				current_value; ///< The current value of the animation.
			animation_time_point_t key_frame_start; ///< Time when the last \ref key_frame was reached.
			size_t
				current_frame = 0, ///< The index of the current \ref key_frame.
				repeated = 0; ///< The number of times that this animation has been repeated.
			bool stationary = false; ///< Indicates whether this animation has finished.
		};

		std::vector<key_frame> key_frames; ///< The list of key frames.
		T default_from_value; ///< The default starting value of this animation if no value is previously present.
		/// The number of times to repeat the whole animation. If this is 0, then the animation will be repeated
		/// indefinitely.
		size_t repeat_times = 1;

		/// Updates a given \ref state.
		///
		/// \param s The \ref state.
		/// \param now The time of now.
		/// \return The time, in seconds, before this \ref state needs to be updated again.
		animation_duration_t update(state &s, animation_time_point_t now) const {
			if (!s.stationary) {
				for (size_t i = 0; i < maximum_frames_per_update; ++i) { // go through the frames
					if (s.current_frame >= key_frames.size()) { // animation has finished
						s.stationary = true;
						s.current_value = key_frames.empty() ? default_from_value : key_frames.back().target;
						return animation_duration_t::max();
					}
					const key_frame &f = key_frames[s.current_frame];
					animation_time_point_t key_frame_end = s.key_frame_start + f.duration;
					if (key_frame_end > now) { // at the correct frame
						if (f.transition_func) {
							s.current_value = Lerp()(s.from, f.target, f.transition_func(
								std::chrono::duration<double>(now - s.key_frame_start) / f.duration
							));
							return animation_duration_t(0); // update immediately
						} else { // wait until this key frame is over
							s.current_value = f.target;
							return key_frame_end - now;
						}
					}
					// advance frame
					s.key_frame_start += f.duration;
					s.from = f.target; // next frame starts at the target value of this frame
					if (++s.current_frame == key_frames.size()) { // at the end, check if should repeat
						++s.repeated;
						if (repeat_times == 0 || s.repeated < repeat_times) { // yes
							s.current_frame = 0;
						} else { // this animation has finished
							s.stationary = true;
							s.current_value = f.target;
							return animation_duration_t::max();
						}
					}
				}
				logger::get().log_warning(CP_HERE, "potential infinite loop in animation");
			}
			return animation_duration_t::max(); // don't update
		}
	};

	/// Patterns used to match states.
	struct state_pattern {
		/// Default constructor.
		state_pattern() = default;
		/// Creates a state pattern with the given target and (optionally) mask.
		explicit state_pattern(element_state_id tarval, element_state_id maskval = ~normal_element_state_id) :
			mask(maskval), target(tarval) {
		}

		element_state_id
			/// Used to mask the state in question.
			mask = ~normal_element_state_id,
			/// The target state, after having been masked.
			target = normal_element_state_id;

		/// Equality.
		friend bool operator==(const state_pattern &lhs, const state_pattern &rhs) {
			return lhs.mask == rhs.mask && lhs.target == rhs.target;
		}
		/// Inequality.
		friend bool operator!=(const state_pattern &lhs, const state_pattern &rhs) {
			return !(lhs == rhs);
		}

		/// Tests if the given value matches this pattern.
		bool match(element_state_id v) const {
			return (v & mask) == target;
		}
	};
	/// Records the mapping between different states and corresponding property values.
	template <typename T> class state_mapping {
	public:
		/// Returns the first entry that matches the given \ref element_state_id, or \p nullptr if none exists.
		const T *try_match_state(element_state_id s) const {
			auto found = _match(s);
			return found == _entries.end() ? nullptr : &found->value;
		}
		/// Returns the first entry that matches the given \ref element_state_id. If no such state exists, the first
		/// one of all states is returned.
		const T &match_state_or_first(element_state_id s) const {
			auto found = _match(s);
			return (found == _entries.end() ? _entries.at(0) : *found).value;
		}

		/// Returns the first entry that has the exact same \ref state_pattern as what's given.
		T *try_get_state(state_pattern pat) {
			auto found = _find(_entries.begin(), _entries.end(), pat);
			return found == _entries.end() ? nullptr : &found->value;
		}
		/// Const version of \ref try_get_state().
		const T *try_get_state(state_pattern pat) const {
			auto found = _find(_entries.begin(), _entries.end(), pat);
			return found == _entries.end() ? nullptr : &found->value;
		}

		/// Adds a new entry with the given pattern and state value to the end of the list.
		void register_state(state_pattern pat, T obj) {
			_entries.emplace_back(pat, std::move(obj));
		}
	protected:
		/// Stores a \ref state_pattern and the corresponding state value.
		struct _entry {
			/// Default constructor.
			_entry() = default;
			/// Constructor that initializes all fields of the struct.
			_entry(state_pattern pat, T val) : value(std::move(val)), pattern(pat) {
			}

			T value; ///< The value.
			state_pattern pattern; ///< The pattern.
		};
		std::vector<_entry> _entries; ///< Different property values in different states.

		/// Finds the first entry in \ref _entries whose \ref _entry::pattern matches the given state.
		typename std::vector<_entry>::const_iterator _match(element_state_id state) const {
			auto it = _entries.begin();
			for (; it != _entries.end() && !it->pattern.match(state); ++it) {
			}
			return it;
		}
		/// Finds the first entry in \ref _entries whose \ref _entry::pattern is the same as the given one.
		template <typename It> inline static It _find(It beg, It end, state_pattern pattern) {
			auto it = beg;
			for (; it != end && it->pattern != pattern; ++it) {
			}
			return it;
		}
	};
}
