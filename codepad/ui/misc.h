// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Miscellaneous types and functions for the user interface.

#include <cstdint>
#include <charconv>
#include <chrono>

#include "../core/misc.h"
#include "../core/json/misc.h"
#include "../core/json/parsing.h"
#include "renderer.h"

namespace codepad {
	/// Contains a collection of functions that parse objects from JSON objects.
	namespace json {
		/// Parser for \ref vec2d.
		template <> struct default_parser<vec2d> {
			/// Parses \ref vec2d. The object must be of the following format: <tt>[x, y]</tt>
			template <typename Value> std::optional<vec2d> operator()(const Value &val) const {
				std::optional<double> x, y;
				if (auto arr = val.template try_cast<typename Value::array_type>()) {
					if (arr->size() >= 2) {
						if (arr->size() > 2) {
							val.template log<log_level::warning>(CP_HERE) << u8"too many elements in vec2";
						}
						x = arr->at(0).template parse<double>();
						y = arr->at(1).template parse<double>();
					} else {
						val.template log<log_level::error>(CP_HERE) << u8"too few elements in vec2";
					}
				} else if (auto obj = val.template try_cast<typename Value::object_type>()) {
					if (obj->size() > 2) {
						val.template log<log_level::warning>(CP_HERE) << u8"redundant fields in vec2 definition";
					}
					x = obj->template parse_member<double>(u8"x");
					y = obj->template parse_member<double>(u8"y");
				} else {
					val.template log<log_level::error>(CP_HERE) << u8"invalid vec2 format";
				}
				if (x && y) {
					return vec2d(x.value(), y.value());
				}
				return std::nullopt;
			}
		};

		/// Parser for \ref colord.
		template <> struct default_parser<colord> {
			/// Parses \ref colord. The object can take the following formats: <tt>["hsl", h, s, l(, a)]</tt> for
			/// HSL format colors, and <tt>[r, g, b(, a)]</tt> for RGB format colors.
			template <typename Value> std::optional<colord> operator()(const Value &val) const {
				if (auto arr = val.template cast<typename Value::array_type>()) { // must be an array
					if (arr->size() >= 3) {
						colord result;
						if (arr->size() > 3) {
							if (auto format = arr->at(0).template try_cast<str_view_t>()) {
								if (format.value() == u8"hsl") {
									auto
										h = arr->at(1).template cast<double>(),
										s = arr->at(2).template cast<double>(),
										l = arr->at(3).template cast<double>();
									result = colord::from_hsl(h.value(), s.value(), l.value());
									if (arr->size() > 4) {
										result.a = arr->at(3).template cast<double>().value_or(1.0);
										if (arr->size() > 5) {
											val.template log<log_level::error>(CP_HERE) <<
												"redundant fields in color definition";
										}
									}
									return result;
								}
							} else {
								result.a = arr->at(3).template cast<double>().value_or(1.0);
							}
							if (arr->size() > 4) {
								val.template log<log_level::error>(CP_HERE) <<
									"redundant fields in color definition";
							}
						}
						result.r = arr->at(0).template cast<double>().value_or(0.0);
						result.g = arr->at(1).template cast<double>().value_or(0.0);
						result.b = arr->at(2).template cast<double>().value_or(0.0);
						return result;
					} else {
						val.template log<log_level::error>(CP_HERE) << "too few elements in color definition";
					}
				}
				return std::nullopt;
			}
		};

		/// Parser for \p std::chrono::duration.
		template <typename Rep, typename Period> struct default_parser<std::chrono::duration<Rep, Period>> {
			/// Parses a duration from a number. If the object is a number, it is treated as the number of seconds.
			///
			/// \todo Also accept string representations.
			template <typename Value> std::optional<std::chrono::duration<Rep, Period>> operator()(
				const Value &val
				) const {
				if (auto secs = val.template cast<double>()) {
					return std::chrono::duration_cast<std::chrono::duration<Rep, Period>>(
						std::chrono::duration<double>(secs.value())
						);
				}
				return std::nullopt;
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
	namespace json {
		/// Parser for \ref ui::orientation.
		template <> struct default_parser<ui::orientation> {
			/// Parses \ref orientation.
			template <typename Value> std::optional<ui::orientation> operator()(const Value &obj) const {
				if (auto opt_str = obj.template cast<str_view_t>()) {
					str_view_t str = opt_str.value();
					if (str == u8"h" || str == u8"hori" || str == u8"horizontal") {
						return ui::orientation::horizontal;
					} else if (str == u8"v" || str == u8"vert" || str == u8"vertical") {
						return ui::orientation::vertical;
					} else {
						obj.template log<log_level::error>(CP_HERE) << "invalid orientation string";
					}
				}
				return std::nullopt;
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
	namespace json {
		/// Parser for \ref ui::visibility.
		template <> struct default_parser<ui::visibility> {
			/// Parses \ref ui::visibility. Each character in the string corresponds to a bit of the value.
			template <typename Value> std::optional<ui::visibility> operator()(const Value &val) const {
				if (val.template is<json::null_t>()) {
					return ui::visibility::none;
				} else if (auto str = val.template try_cast<str_view_t>()) {
					return get_bitset_from_string<ui::visibility>({
						{u8'v', ui::visibility::visual},
						{u8'i', ui::visibility::interact},
						{u8'l', ui::visibility::layout},
						{u8'f', ui::visibility::focus}
						}, str.value());
				} else {
					val.template log<log_level::error>(CP_HERE) << "invalid visibility format";
				}
				return std::nullopt;
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
	namespace json {
		/// Parser for \ref ui::cursor.
		template <> struct default_parser<ui::cursor> {
			/// Parses a \ref ui::cursor.
			template <typename Value> std::optional<ui::cursor> operator()(const Value &val) const {
				if (auto str = val.template cast<str_view_t>()) {
					if (str.value() == u8"normal") {
						return ui::cursor::normal;
					} else if (str.value() == u8"busy") {
						return ui::cursor::busy;
					} else if (str.value() == u8"crosshair") {
						return ui::cursor::crosshair;
					} else if (str.value() == u8"hand") {
						return ui::cursor::hand;
					} else if (str.value() == u8"help") {
						return ui::cursor::help;
					} else if (str.value() == u8"text_beam") {
						return ui::cursor::text_beam;
					} else if (str.value() == u8"denied") {
						return ui::cursor::denied;
					} else if (str.value() == u8"arrow_all") {
						return ui::cursor::arrow_all;
					} else if (str.value() == u8"arrow_northeast_southwest") {
						return ui::cursor::arrow_northeast_southwest;
					} else if (str.value() == u8"arrow_north_south") {
						return ui::cursor::arrow_north_south;
					} else if (str.value() == u8"arrow_northwest_southeast") {
						return ui::cursor::arrow_northwest_southeast;
					} else if (str.value() == u8"arrow_east_west") {
						return ui::cursor::arrow_east_west;
					} else if (str.value() == u8"invisible") {
						return ui::cursor::invisible;
					}
				}
				return std::nullopt;
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
	}
	/// Parser for \ref ui::mouse_button.
	template <> struct enum_parser<ui::mouse_button> {
		/// The parser interface.
		inline static std::optional<ui::mouse_button> parse(str_view_t text) {
			if (text == u8"primary" || text == u8"m1") {
				return ui::mouse_button::primary;
			} else if (text == u8"secondary" || text == u8"m2") {
				return ui::mouse_button::secondary;
			} else if (text == u8"tertiary" || text == u8"middle") {
				return ui::mouse_button::tertiary;
			}
			return std::nullopt;
		}
	};
	namespace ui {
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
		constexpr std::size_t total_num_keys = static_cast<std::size_t>(key::max_value);
	}
	/// Parser for \ref ui::key.
	template <> struct enum_parser<ui::key> {
		/// The parser interface.
		inline static std::optional<ui::key> parse(str_view_t str) {
			// TODO caseless comparison
			if (str.length() == 1) {
				if (str[0] >= 'a' && str[0] <= 'z') {
					return static_cast<ui::key>(
						static_cast<std::size_t>(ui::key::a) + (str[0] - 'a')
						);
				}
				switch (str[0]) {
				case ' ':
					return ui::key::space;
				case '+':
					return ui::key::add;
				case '-':
					return ui::key::subtract;
				case '*':
					return ui::key::multiply;
				case '/':
					return ui::key::divide;
				}
			}
			if (str == u8"left") {
				return ui::key::left;
			} else if (str == u8"right") {
				return ui::key::right;
			} else if (str == u8"up") {
				return ui::key::up;
			} else if (str == u8"down") {
				return ui::key::down;
			} else if (str == u8"space") {
				return ui::key::space;
			} else if (str == u8"insert") {
				return ui::key::insert;
			} else if (str == u8"delete") {
				return ui::key::del;
			} else if (str == u8"backspace") {
				return ui::key::backspace;
			} else if (str == u8"home") {
				return ui::key::home;
			} else if (str == u8"end") {
				return ui::key::end;
			} else if (str == u8"enter") {
				return ui::key::enter;
			}
			return std::nullopt;
		}
	};
	namespace json {
		/// Parser for \ref ui::key.
		template <> struct default_parser<ui::key> {
			/// The parser interface.
			template <typename Value> std::optional<ui::key> operator()(const Value &val) const {
				if (auto str = val.template cast<str_view_t>()) {
					return enum_parser<ui::key>::parse(str.value());
				}
				return std::nullopt;
			}
		};
	}

	namespace ui {
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
	namespace json {
		/// Parser for \ref ui::thickness.
		template <> struct default_parser<ui::thickness> {
			/// Parses \ref ui::thickness. The object can take the following formats:
			/// <tt>[left, top, right, bottom]</tt> or a single number specifying the value for all four
			/// directions.
			template <typename Value> std::optional<ui::thickness> operator()(const Value &val) const {
				if (auto arr = val.template try_cast<typename Value::array_type>()) {
					if (arr->size() >= 4) {
						if (arr->size() > 4) {
							val.template log<log_level::error>(CP_HERE) <<
								"redundant elements in thickness definition";
						}
						auto
							l = arr->at(0).template cast<double>(),
							t = arr->at(1).template cast<double>(),
							r = arr->at(2).template cast<double>(),
							b = arr->at(3).template cast<double>();
						if (l && t && r && b) {
							return ui::thickness(l.value(), t.value(), r.value(), b.value());
						}
					} else {
						val.template log<log_level::error>(CP_HERE) << "too few elements in thickness";
					}
				} else if (auto v = val.template try_cast<double>()) {
					return ui::thickness(v.value());
				} else {
					val.template log<log_level::error>(CP_HERE) << "invalid thickness format";
				}
				return std::nullopt;
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

			/// Returns a \ref size_allocation corresponding to the given number of pixels.
			inline static size_allocation pixels(double px) {
				return size_allocation(px, true);
			}
			/// Returns a \ref size_allocation corresponding to the given proportion.
			inline static size_allocation proportion(double val) {
				return size_allocation(val, false);
			}

			double value = 0.0; ///< The value.
			bool is_pixels = false; ///< Indicates whether \ref value is in pixels instead of a proportion.
		};
	}
	namespace json {
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
		template <> struct default_parser<ui::size_allocation> {
			/// Parses a \ref ui::size_allocation. The object can either be a full representation of the struct with two
			/// fields, a single number in pixels, or a string that optionally ends with `*', `%', or `px', the former
			/// two of which indicates that the value is a proportion. If the string ends with `%', the value is
			/// additionally divided by 100, thus using `%' and `*' mixedly is not recommended.
			template <typename Value> std::optional<ui::size_allocation> operator()(const Value &val) const {
				if (auto pixels = val.template try_cast<double>()) { // number in pixels
					return ui::size_allocation::pixels(pixels.value());
				} else if (auto str = val.template try_cast<str_view_t>()) {
					ui::size_allocation res(0.0, true); // in pixels if no suffix is present
					// the value is additionally divided by 100 if it's a percentage
					bool percentage = _details::ends_with(str.value(), "%");
					if (percentage || _details::ends_with(str.value(), "*")) { // proportion
						res.is_pixels = false;
					}
					// it's not safe to use std::strtod here since it requires that the input string be null-terminated
#ifdef __GNUC__
					// FIXME hack because libstdc++ maintainer doesn't know how to implement from_chars
					res.value = std::stod(str_t(str.value()));
#else
					std::from_chars(str->data(), str->data() + str->size(), res.value); // result ignored
#endif
					if (percentage) {
						res.value *= 0.01;
					}
					return res;
				} else if (auto full = val.template try_cast<typename Value::object_type>()) {
					// full object representation
					auto value = full->template parse_member<double>(u8"value");
					auto is_pixels = full->template parse_member<bool>(u8"is_pixels");
					if (value && is_pixels) {
						if (full->size() > 2) {
							full->template log<log_level::error>(CP_HERE) << "redundant fields in size allocation";
						}
						return ui::size_allocation(value.value(), is_pixels.value());
					}
				} else {
					val.template log<log_level::error>(CP_HERE) << "invalid size allocation format";
				}
				return std::nullopt;
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
	namespace json {
		/// Parser for \ref ui::anchor.
		template <> struct default_parser<ui::anchor> {
			/// Parses \ref ui::anchor. The object can be a string that contains any combination of characters `l',
			/// `t', `r', and `b', standing for \ref ui::anchor::left, \ref ui::anchor::top, \ref ui::anchor::right,
			/// and \ref ui::anchor::bottom, respectively.
			template <typename Value> std::optional<ui::anchor> operator()(const Value &obj) const {
				if (auto str = obj.template cast<str_view_t>()) {
					return get_bitset_from_string<ui::anchor>({
						{CP_STRLIT('l'), ui::anchor::left},
						{CP_STRLIT('t'), ui::anchor::top},
						{CP_STRLIT('r'), ui::anchor::right},
						{CP_STRLIT('b'), ui::anchor::bottom}
						}, str.value());
				}
				return std::nullopt;
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
	/// Parser for \ref ui::size_allocation_type.
	template <> struct enum_parser<ui::size_allocation_type> {
		/// The parser interface.
		inline static std::optional<ui::size_allocation_type> parse(str_view_t str) {
			// TODO caseless comparison
			if (str == u8"fixed" || str == u8"pixels" || str == u8"px") {
				return ui::size_allocation_type::fixed;
			} else if (str == u8"proportion" || str == u8"prop" || str == u8"*") {
				return ui::size_allocation_type::proportion;
			} else if (str == u8"automatic" || str == u8"auto") {
				return ui::size_allocation_type::automatic;
			}
			return std::nullopt;
		}
	};
	namespace json {
		/// Parser for \ref ui::size_allocation_type.
		template <> struct default_parser<ui::size_allocation_type> {
			/// Parses \ref ui::size_allocation_type. Checks if the given object (which must be a string) is one of
			/// the constants and returns the corresponding value.
			template <typename Value> std::optional<ui::size_allocation_type> operator()(const Value &obj) const {
				if (auto str = obj.template cast<str_view_t>()) {
					return enum_parser<ui::size_allocation_type>::parse(str.value());
				}
				return std::nullopt;
			}
		};
	}

	namespace ui {
		/// A JSON parser that loads resources from the specified \ref manager. The default implementation does not rely
		/// upon a \ref manager and simply defaults to \ref json::default_parser.
		template <typename T> struct managed_json_parser {
			/// The parser interface.
			template <typename Value> std::optional<T> operator()(const Value &v) const {
				return json::default_parser<T>{}(v);
			}
		};
	}
}
