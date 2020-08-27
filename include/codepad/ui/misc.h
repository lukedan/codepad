// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Miscellaneous types and functions for the user interface.

#include <cstdint>
#include <charconv>
#include <chrono>
#include <compare>

#include "../core/misc.h"
#include "../core/json/misc.h"
#include "../core/json/parsing.h"
#include "renderer.h"

namespace codepad {
	// forward declarations
	namespace ui {
		struct mouse_position;
		class element;
		class manager;
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
			/// Parses \ref ui::orientation.
			template <typename Value> std::optional<ui::orientation> operator()(const Value&) const;
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
	/// Enables bitwise operators for \ref ui::visibility.
	template <> struct enable_enum_bitwise_operators<ui::visibility> : public std::true_type {
	};
	namespace json {
		/// Parser for \ref ui::visibility.
		template <> struct default_parser<ui::visibility> {
			/// Parses \ref ui::visibility. Each character in the string corresponds to a bit of the value.
			template <typename Value> std::optional<ui::visibility> operator()(const Value&) const;
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
			template <typename Value> std::optional<ui::cursor> operator()(const Value&) const;
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
		static std::optional<ui::mouse_button> parse(std::u8string_view);
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
		static std::optional<ui::key> parse(std::u8string_view);
	};

	namespace ui {
		/// The type of a line ending.
		enum class line_ending : unsigned char {
			none, ///< Unspecified or invalid. Sometimes also used to indicate EOF or soft linebreaks.
			r, ///< \p \\r.
			n, ///< \p \\n, usually used in Linux.
			rn ///< \p \\r\\n, usually used in Windows.
		};
		/// Returns the length, in codepoints, of the string representation of a \ref line_ending.
		inline std::size_t get_line_ending_length(line_ending le) {
			switch (le) {
			case line_ending::none:
				return 0;
			case line_ending::n:
				[[fallthrough]];
			case line_ending::r:
				return 1;
			case line_ending::rn:
				return 2;
			}
			return 0;
		}
		/// Returns the string representation of the given \ref line_ending.
		inline std::u32string_view line_ending_to_string(line_ending le) {
			switch (le) {
			case line_ending::r:
				return U"\r";
			case line_ending::n:
				return U"\n";
			case line_ending::rn:
				return U"\r\n";
			default:
				return U"";
			}
		}

		/// Used to analyze a sequence of codepoints and find linebreaks.
		struct linebreak_analyzer {
		public:
			/// Initializes \ref new_line_callback.
			explicit linebreak_analyzer(std::function<void(std::size_t, line_ending)> callback) :
				new_line_callback(std::move(callback)) {
			}

			/// Adds a new codepoint to the back of this \ref linebreak_analyzer.
			void put(codepoint c) {
				if (_last == U'\r') { // linebreak starting at the last codepoint
					if (c == U'\n') { // \r\n
						new_line_callback(_ncps - 1, line_ending::rn);
						_ncps = 0;
					} else { // \r
						new_line_callback(_ncps - 1, line_ending::r);
						_ncps = 1;
					}
				} else if (c == U'\n') { // \n
					new_line_callback(_ncps, line_ending::n);
					_ncps = 0;
				} else { // normal character, or \r (handled later)
					++_ncps;
				}
				_last = c;
			}
			/// Finish analysis. Must be called before using the return value of \ref result().
			void finish() {
				if (_last == U'\r') {
					new_line_callback(_ncps - 1, line_ending::r);
					_ncps = 0;
				}
				new_line_callback(_ncps, line_ending::none);
			}

			/// The callback that will be invoked when a new line is encountered.
			std::function<void(std::size_t, line_ending)> new_line_callback;
		protected:
			std::size_t _ncps = 0; ///< The number of codepoints in the current line.
			codepoint _last = 0; ///< The last codepoint.
		};


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
			template <typename Value> std::optional<ui::thickness> operator()(const Value&) const;
		};
	}

	/// Specialization for ui::thickness since it doesn't support arithmetic operators.
	template <> inline ui::thickness lerp<ui::thickness>(ui::thickness from, ui::thickness to, double perc) {
		return ui::thickness(
			std::lerp(from.left, to.left, perc),
			std::lerp(from.top, to.top, perc),
			std::lerp(from.right, to.right, perc),
			std::lerp(from.bottom, to.bottom, perc)
		);
	}

	namespace ui {
		/// Contains information about how size should be allocated on a certain orientation for an \ref element.
		struct size_allocation {
			/// Default constructor.
			size_allocation() = default;
			/// Initializes all fields of this struct. Use \ref pixels() or \ref proportion() instead of this
			/// whenever possible.
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
		/// Parser for \ref ui::size_allocation.
		template <> struct default_parser<ui::size_allocation> {
			/// Parses a \ref ui::size_allocation. The object can either be a full representation of the struct with two
			/// fields, a single number in pixels, or a string that optionally ends with `*', `%', or `px', the former
			/// two of which indicates that the value is a proportion. If the string ends with `%', the value is
			/// additionally divided by 100, thus using `%' and `*' mixedly is not recommended.
			template <typename Value> std::optional<ui::size_allocation> operator()(const Value&) const;
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
			template <typename Value> std::optional<ui::anchor> operator()(const Value&) const;
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
		static std::optional<ui::size_allocation_type> parse(std::u8string_view);
	};

	namespace ui {
		/// A JSON parser that loads resources from the specified \ref manager. The default implementation does not rely
		/// upon a \ref manager and simply defaults to \ref json::default_parser.
		template <typename T> struct managed_json_parser {
			/// Initializes this struct with a dummy parameter.
			explicit managed_json_parser(manager&) {
			}

			/// The parser interface.
			template <typename Value> std::optional<T> operator()(const Value &v) const {
				return json::default_parser<T>{}(v);
			}
		};


		/// A temporary \ref render_target, with pixel snapping. The transform of the current render target must not
		/// change after this is created until the text has been drawn onto the original render target when this is
		/// disposed or when using \ref finish(). This is mainly used to work around the fact that pushing clips
		/// (layers) in Direct2D disables subpixel antialiasing.
		class pixel_snapped_render_target {
		public:
			/// Constructs this struct with the renderer, the target area where text is to be drawn, and the scaling
			/// factor of the current render target. This function computes the offset required for pixel snapping,
			/// and starts rendering to the temporary render target.
			pixel_snapped_render_target(renderer_base &r, rectd target_area, vec2d scaling) : _renderer(r) {
				_target = r.create_render_target(target_area.size(), scaling, colord(1.0, 1.0, 1.0, 0.0));
				vec2d corner = target_area.xmin_ymin();
				matd3x3 transform = r.get_matrix(), snap_transform = matd3x3::identity();
				if (!transform.has_rotation_or_nonrigid()) {
					vec2d trans_corner = corner + vec2d(transform[0][2], transform[1][2]);
					trans_corner.x *= scaling.x;
					trans_corner.y *= scaling.y;
					vec2d offset = vec2d(std::round(trans_corner.x), std::round(trans_corner.y)) - trans_corner;
					offset.x /= scaling.x;
					offset.y /= scaling.y;

					_snapped_position = corner + offset;
					snap_transform = matd3x3::translate(-offset);
				}

				_renderer.begin_drawing(*_target.target);
				_renderer.push_matrix(snap_transform);
			}
			/// Calls \ref finish().
			~pixel_snapped_render_target() {
				finish();
			}

			/// If this buffer is still valid, finishes rendering to the buffer and draws it onto the last render
			/// target.
			void finish() {
				if (_target.target_bitmap) {
					_renderer.pop_matrix();
					_renderer.end_drawing();

					_renderer.push_matrix_mult(matd3x3::translate(_snapped_position));
					_renderer.draw_rectangle(
						rectd::from_corners(vec2d(), _target.target_bitmap->get_size()),
						ui::generic_brush(
							ui::brushes::bitmap_pattern(_target.target_bitmap.get())
						),
						ui::generic_pen()
					);
					_renderer.pop_matrix();

					_target.target.reset();
					_target.target_bitmap.reset();
				}
			}
		protected:
			/// The position where the temporary buffer should be drawn onto in the original render target, with
			/// pixel snapping.
			vec2d _snapped_position;
			render_target_data _target; ///< The temporary render target.
			renderer_base &_renderer; ///< The renderer.
		};

		/// Used when starting dragging to add a small deadzone where dragging will not yet trigger. The mouse is
		/// captured when the mouse is in the deadzone.
		class drag_deadzone {
		public:
			/// Default constructor.
			drag_deadzone();
			/// Initializes \ref radius.
			explicit drag_deadzone(double r) : radius(r) {
			}

			/// Initializes the starting position and starts dragging by capturing the mouse.
			void start(const mouse_position &mouse, element &parent);
			/// Updates the mouse position.
			///
			/// \return \p true if the mouse has moved out of the deadzone and dragging should start, or \p false if
			///         the mouse is still in the deadzone.
			bool update(const mouse_position &mouse, element &parent);
			/// Cancels the drag operation.
			void on_cancel(element &parent);
			/// Cancels the drag operation without releasing the capture (as it has already been lost).
			void on_capture_lost() {
				_deadzone = false;
			}

			/// Returns \p true if the user is trying to drag the associated object but is in the deadzone.
			bool is_active() const {
				return _deadzone;
			}

			double radius = 0.0; ///< The radius of the deadzone.
		protected:
			/// The starting position relative to the window. This is to ensure that the size of the deadzone stays
			/// consistent when the element itself is transformed.
			vec2d _start;
			bool _deadzone = false; ///< \p true if the user is dragging and is in the deadzone.
		};


		/// A caret and the associated selected region.
		struct caret_selection {
		public:
			/// Default constructor.
			caret_selection() = default;
			/// Sets the caret position, and sets the selection to empty.
			explicit caret_selection(std::size_t pos) : caret(pos), selection(pos) {
			}
			/// Initializes all fields of this struct.
			caret_selection(std::size_t c, std::size_t s) : caret(c), selection(s) {
			}

			/// Returns the range covered by this selection. Basically calls \p std::minmax().
			std::pair<std::size_t, std::size_t> get_range() const {
				return std::minmax({ caret, selection });
			}

			/// Returns whether there is a selection, i.e., \ref caret != \ref selection.
			bool has_selection() const {
				return caret != selection;
			}

			/// Default comparisons. \ref caret will always be compared first.
			friend std::strong_ordering operator<=>(const caret_selection&, const caret_selection&) = default;

			std::size_t
				caret = 0, ///< The caret.
				selection = 0; ///< The other end of the selection region.
		};
	}
}
