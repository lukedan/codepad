// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Miscellaneous types and functions for the user interface.

#include <cstdint>
#include <charconv>
#include <chrono>

#include "../core/misc.h"
#include "../core/json.h"
#include "renderer.h"

namespace codepad {
	/// Contains a collection of functions that parse objects from JSON objects.
	namespace json::object_parsers {
		/// Parser for \ref vec2d.
		template <> struct parser<vec2d> {
			/// Parses \ref vec2d. The object must be of the following format: <tt>[x, y]</tt>
			template <typename Value> bool operator()(const Value &obj, vec2d &v) const {
				if (typename Value::array_t arr; json::try_cast(obj, arr) && arr.size() >= 2) {
					v.x = json::cast_or_default<double>(arr[0], 0.0);
					v.y = json::cast_or_default<double>(arr[1], 0.0);
					return true;
				}
				return false;
			}
		};

		/// Parser for \ref colord.
		template <> struct parser<colord> {
			/// Parses \ref colord. The object can take the following formats: <tt>["hsl", h, s, l(, a)]</tt> for
			/// HSL format colors, and <tt>[r, g, b(, a)]</tt> for RGB format colors.
			template <typename Value> bool operator()(const Value &obj, colord &c) const {
				if (typename Value::array_t arr; json::try_cast(obj, arr) && arr.size() >= 3) { // must be an array
					if (arr.size() > 3) {
						if (str_view_t type; json::try_cast(arr[0], type) && type == u8"hsl") {
							double h, s, l;
							json::try_cast(arr[1], h);
							json::try_cast(arr[2], s);
							json::try_cast(arr[3], l);
							c = colord::from_hsl(h, s, l);
							if (arr.size() > 4) {
								json::try_cast(arr[4], c.a);
							}
							return true;
						} else {
							json::try_cast(arr[3], c.a);
						}
					}
					json::try_cast(arr[0], c.r);
					json::try_cast(arr[1], c.g);
					json::try_cast(arr[2], c.b);
					return true;
				}
				return false;
			}
		};

		/// Parser for \p std::chrono::duration.
		template <typename Rep, typename Period> struct parser<std::chrono::duration<Rep, Period>> {
			/// Parses a duration from a number. If the object is a number, it is treated as the number of seconds.
			///
			/// \todo Also accept string representations.
			template <typename Value> bool operator()(const Value &val, std::chrono::duration<Rep, Period> &dur) const {
				if (double secs; json::try_cast(val, secs)) {
					dur = std::chrono::duration_cast<std::chrono::duration<Rep, Period>>(
						std::chrono::duration<double>(secs)
						);
					return true;
				}
				return false;
			}
		};
	}

	namespace ui {
		/// Orientation.
		enum class orientation : unsigned char {
			horizontal, ///< Horizontal.
			vertical ///< Vertical.
		};
	}
	namespace json::object_parsers {
		/// Parser for \ref ui::orientation.
		template <> struct parser<ui::orientation> {
			/// Parses \ref orientation.
			template <typename Value> bool operator()(const Value &obj, ui::orientation &o) const {
				if (str_view_t val; json::try_cast(obj, val)) {
					if (val == u8"h" || val == u8"hori" || val == u8"horizontal") {
						o = ui::orientation::horizontal;
						return true;
					} else if (val == u8"v" || val == u8"vert" || val == u8"vertical") {
						o = ui::orientation::vertical;
						return true;
					}
				}
				return false;
			}
		};
	}

	namespace ui {
		/// The visibility of an \ref element.
		enum class visibility : unsigned char {
			none = 0, ///< Invisible in all aspects.
			visual = 1, ///< The element is rendered.
			interact = 2, ///< The element is taken into account during hit testing.
			layout = 4, ///< The element is taken into account when calculating layout.
			focus = 8, ///< The element can be the focused element.

			full = visual | interact | layout | focus ///< Visible in all aspects.
		};
	}
	/// Enables bitwise operators for \ref modifier_keys.
	template <> struct enable_enum_bitwise_operators<ui::visibility> : public std::true_type {
	};
	namespace json::object_parsers {
		/// Parser for \ref ui::visibility.
		template <> struct parser<ui::visibility> {
			/// Parses \ref ui::visibility. Each character in the string corresponds to a bit of the value.
			template <typename Value> bool operator()(const Value &obj, ui::visibility &v) const {
				if (obj.is<json::null_t>()) {
					v = ui::visibility::none;
					return true;
				}
				if (str_view_t val; json::try_cast(obj, val)) {
					v = get_bitset_from_string<ui::visibility>({
						{u8'v', ui::visibility::visual},
						{u8'i', ui::visibility::interact},
						{u8'l', ui::visibility::layout},
						{u8'f', ui::visibility::focus}
						}, val);
					return true;
				}
				return false;
			}
		};
	}

	namespace ui {
		/// The style of the cursor.
		enum class cursor : unsigned char {
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
	}
	namespace json::object_parsers {
		/// Parser for \ref ui::cursor.
		template <> struct parser<ui::cursor> {
			/// Parses a \ref ui::cursor.
			template <typename Value> bool operator()(const Value &obj, ui::cursor &c) const {
				if (str_view_t s; json::try_cast(obj, s)) {
					if (s == u8"normal") {
						c = ui::cursor::normal;
						return true;
					}
					if (s == u8"busy") {
						c = ui::cursor::busy;
						return true;
					}
					if (s == u8"crosshair") {
						c = ui::cursor::crosshair;
						return true;
					}
					if (s == u8"hand") {
						c = ui::cursor::hand;
						return true;
					}
					if (s == u8"help") {
						c = ui::cursor::help;
						return true;
					}
					if (s == u8"text_beam") {
						c = ui::cursor::text_beam;
						return true;
					}
					if (s == u8"denied") {
						c = ui::cursor::denied;
						return true;
					}
					if (s == u8"arrow_all") {
						c = ui::cursor::arrow_all;
						return true;
					}
					if (s == u8"arrow_northeast_southwest") {
						c = ui::cursor::arrow_northeast_southwest;
						return true;
					}
					if (s == u8"arrow_north_south") {
						c = ui::cursor::arrow_north_south;
						return true;
					}
					if (s == u8"arrow_northwest_southeast") {
						c = ui::cursor::arrow_northwest_southeast;
						return true;
					}
					if (s == u8"arrow_east_west") {
						c = ui::cursor::arrow_east_west;
						return true;
					}
					if (s == u8"invisible") {
						c = ui::cursor::invisible;
						return true;
					}
				}
				return false;
			}
		};
	}

	namespace ui {
		/// Represents a button of the mouse.
		enum class mouse_button : unsigned char {
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
	}
	namespace json::object_parsers {
		/// Parser for \ref ui::thickness.
		template <> struct parser<ui::thickness> {
			/// Parses \ref ui::thickness. The object can take the following formats:
			/// <tt>[left, top, right, bottom]</tt> or a single number specifying the value for all four
			/// directions.
			template <typename Value> bool operator()(const Value &obj, ui::thickness &t) const {
				if (typename Value::array_t arr; json::try_cast(obj, arr) && arr.size() >= 4) {
					json::try_cast(arr[0], t.left);
					json::try_cast(arr[1], t.top);
					json::try_cast(arr[2], t.right);
					json::try_cast(arr[3], t.bottom);
					return true;
				}
				if (double v; json::try_cast(obj, v)) {
					t.left = t.top = t.right = t.bottom = v;
					return true;
				}
				return false;
			}
		};
	}

	/// Specialization for ui::thickness since it doesn't support arithmetic operators.
	template <> inline ui::thickness lerp<ui::thickness>(ui::thickness from, ui::thickness to, double perc) {
		return ui::thickness(
			lerp(from.left, to.left, perc),
			lerp(from.top, to.top, perc),
			lerp(from.right, to.right, perc),
			lerp(from.bottom, to.bottom, perc)
		);
	}

	namespace ui {
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
	}
	namespace json::object_parsers {
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
		/// Parser for \ref ui::size_allocation.
		template <> struct parser<ui::size_allocation> {
			/// Parses a \ref ui::size_allocation. The object can either be a full representation of the struct with two
			/// fields, a single number in pixels, or a string that optionally ends with `*', `%', or `px', the former
			/// two of which indicates that the value is a proportion. If the string ends with `%', the value is
			/// additionally divided by 100, thus using `%' and `*' mixedly is not recommended.
			template <typename Value> bool operator()(const Value &obj, ui::size_allocation &sz) const {
				if (json::try_cast(obj, sz.value)) { // number in pixels
					sz.is_pixels = true;
					return true;
				}
				if (typename Value::object_t full; json::try_cast(obj, full)) { // full object representation
					json::try_cast_member(full, u8"value", sz.value);
					json::try_cast_member(full, u8"is_pixels", sz.is_pixels);
					return true;
				}
				if (str_view_t str; json::try_cast(obj, str)) {
					sz.value = 0;
					sz.is_pixels = true; // in pixels if no suffix is present
					bool percentage = _details::ends_with(str, "%"); // the value is additionally divided by 100
					if (percentage || _details::ends_with(str, "*")) { // proportion
						sz.is_pixels = false;
					}
					std::from_chars(str.data(), str.data() + str.size(), sz.value); // result ignored
					if (percentage) {
						sz.value *= 0.01;
					}
					return true;
				}
				return false;
			}
		};
	}

	namespace ui {
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
	}
	/// Enables bitwise operators for \ref ui::anchor.
	template <> struct enable_enum_bitwise_operators<ui::anchor> : public std::true_type {
	};
	namespace json::object_parsers {
		/// Parser for \ref ui::anchor.
		template <> struct parser<ui::anchor> {
			/// Parses \ref ui::anchor. The object can be a string that contains any combination of characters `l',
			/// `t', `r', and `b', standing for \ref ui::anchor::left, \ref ui::anchor::top, \ref ui::anchor::right,
			/// and \ref ui::anchor::bottom, respectively.
			template <typename Value> bool operator()(const Value &obj, ui::anchor &a) const {
				if (str_view_t str; json::try_cast(obj, str)) {
					a = get_bitset_from_string<ui::anchor>({
						{CP_STRLIT('l'), ui::anchor::left},
						{CP_STRLIT('t'), ui::anchor::top},
						{CP_STRLIT('r'), ui::anchor::right},
						{CP_STRLIT('b'), ui::anchor::bottom}
						}, str);
					return true;
				}
				return false;
			}
		};
	}

	namespace ui {
		/// Determines how size is allocated to an \ref element.
		enum class size_allocation_type : unsigned char {
			/// The size is determined by \ref element::get_desired_width() and \ref element::get_desired_height().
			automatic,
			fixed, ///< The user specifies the size in pixels.
			proportion ///< The user specifies the size as a proportion.
		};
	}
	namespace json::object_parsers {
		/// Parser for \ref ui::size_allocation_type.
		template <> struct parser<ui::size_allocation_type> {
			/// Parses \ref ui::size_allocation_type. Checks if the given object (which must be a string) is one of
			/// the constants and returns the corresponding value.
			template <typename Value> bool operator()(const Value &obj, ui::size_allocation_type &ty) const {
				if (str_view_t str; json::try_cast(obj, str)) {
					if (str == u8"fixed" || str == u8"pixels" || str == u8"px") {
						ty = ui::size_allocation_type::fixed;
						return true;
					}
					if (str == u8"proportion" || str == u8"prop" || str == u8"*") {
						ty = ui::size_allocation_type::proportion;
						return true;
					}
					if (str == u8"automatic" || str == u8"auto") {
						ty = ui::size_allocation_type::automatic;
						return true;
					}
				}
				return false;
			}
		};
	}
}
