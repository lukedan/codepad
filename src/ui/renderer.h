// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes used to render the user interface.

#include <functional>
#include <algorithm>
#include <cstring>
#include <variant>
#include <any>

#include "../core/math.h"
#include "../core/json/misc.h"

namespace codepad::ui {
	class window_base;

	/// Determines the style of rendered text.
	enum class font_style : unsigned char {
		normal, ///< Normal text.
		italic, ///< Slant text.
		oblique ///< Artificially slant text.
	};
	//              fc   dwrite
	// THIN         0    100
	// EXTRALIGHT   40   200
	// LIGHT        50   300
	// SEMILIGHT    55   350
	// BOOK         75
	// REGULAR	    80   400
	// MEDIUM	    100  500
	// SEMIBOLD	    180  600
	// BOLD		    200  700
	// EXTRABOLD	205  800
	// BLACK		210  900
	// EXTRABLACK	215  950
	/// The weight of text.
	///
	/// \todo WTF is the difference between FontConfig and DWrite?
	enum class font_weight : unsigned char {
		normal = 40
	};
	//                  fc   dwrite
	// ULTRACONDENSED	50   1
	// EXTRACONDENSED	63   2
	// CONDENSED	    75   3
	// SEMICONDENSED	87   4
	// NORMAL		    100  5
	// SEMIEXPANDED     113  6
	// EXPANDED	        125  7
	// EXTRAEXPANDED	150  8
	// ULTRAEXPANDED	200  9
	/// The horizontal stretch of text.
	///
	/// \todo Documentation and values.
	enum class font_stretch : unsigned char {
		ultra_condensed,
		extra_condensed,
		condensed,
		semi_condensed,
		normal,
		semi_expanded,
		expanded,
		extra_expanded,
		ultra_expanded
	};

	/// Determines how text is wrapped when it overflows the given boundary.
	enum class wrapping_mode : unsigned char {
		none, ///< Don't wrap.
		wrap ///< Wrap, but in an unspecified manner.
	};
	/// Controls the horizontal alignment of text.
	///
	/// \todo Justified alignment?
	enum class horizontal_text_alignment : unsigned char {
		front, ///< The front of the text is aligned with the front end of the layout box.
		center, ///< Center alignment.
		rear ///< The rear of the text is aligned with the rear end of the layout box.
	};
	/// Controls the vertical alignment of text.
	enum class vertical_text_alignment : unsigned char {
		top, ///< Top.
		center, ///< Center.
		bottom ///< Bottom.
	};

	/// Clockwise or counter-clockwise direction.
	enum class sweep_direction : unsigned char {
		clockwise, ///< Clockwise.
		counter_clockwise ///< Counter-clockwise.
	};
	/// Major or minor arcs.
	enum class arc_type : unsigned char {
		minor, ///< The arc is less than 180 degrees.
		major ///< The arc is greater than 180 degrees.
	};


	/// Basic interface of an bitmap.
	class bitmap {
	public:
		/// Default virtual destructor.
		virtual ~bitmap() = default;

		/// Returns the logical size of this bitmap.
		[[nodiscard]] virtual vec2d get_size() const = 0;
	};
	/// Basic interface of an off-screen render target.
	class render_target {
	public:
		/// Default virtual destructor.
		virtual ~render_target() = default;
	};

	/// Stores a \ref bitmap and potentially the associated \ref render_target.
	struct render_target_data {
		/// Default constructor.
		render_target_data() = default;
		/// Initializes all fields of this struct.
		render_target_data(std::unique_ptr<render_target> rt, std::unique_ptr<bitmap> bmp) :
			target(std::move(rt)), target_bitmap(std::move(bmp)) {
		}

		std::unique_ptr<render_target> target; ///< The \ref render_target.
		std::unique_ptr<bitmap> target_bitmap; ///< The \ref bitmap.
	};

	/// The parameters used to identify a font.
	struct font_parameters {
		/// Default constructor.
		font_parameters() = default;
		/// Initializes all fields of this struct.
		font_parameters(
			std::u8string f, double sz, font_style st = font_style::normal,
			font_weight w = font_weight::normal, font_stretch width = font_stretch::normal
		) : family(std::move(f)), size(sz), style(st), weight(w), stretch(width) {
		}

		std::u8string family; ///< The font family.
		double size = 10.0; ///< The font size.
		font_style style = font_style::normal; ///< The font style.
		font_weight weight = font_weight::normal; ///< The font weight.
		font_stretch stretch = font_stretch::normal; ///< The stretch of the font.
	};

	/// Stores the result of hit test operations.
	struct caret_hit_test_result {
		/// Default constructor.
		caret_hit_test_result() = default;
		/// Initializes all fields of this struct.
		caret_hit_test_result(std::size_t c, rectd layout, bool r) : character(c), character_layout(layout), rear(r) {
		}

		std::size_t character = 0; ///< The character index that the given point is on.
		rectd character_layout; ///< The layout of \ref character.
		bool rear = false; ///< Indicates if the position is after \ref character.
	};
	/// Stores the metrics of a single line.
	struct line_metrics {
		/// Default constructor.
		line_metrics() = default;
		/// Initializes all fields of this struct.
		line_metrics(double h, double b) : height(h), baseline(b) {
		}

		double
			height = 0.0, ///< The height of this line.
			baseline = 0.0; ///< The distance from the top of the line to the baseline.
	};
	/// A piece of text, possibly containing text with various different formats.
	class formatted_text {
	public:
		/// Default virtual destructor.
		virtual ~formatted_text() = default;

		/// Returns the region occupied by the text in the layout region.
		[[nodiscard]] virtual rectd get_layout() const = 0;
		/// Returns the metrics of all lines.
		[[nodiscard]] virtual std::vector<line_metrics> get_line_metrics() const = 0;

		/// Retrieves information about the character that is below the given point.
		[[nodiscard]] virtual caret_hit_test_result hit_test(vec2d) const = 0;
		/// Returns the space occupied by the character at the given position.
		[[nodiscard]] virtual rectd get_character_placement(std::size_t) const = 0;

		/// Sets the color of the specified range of text.
		virtual void set_text_color(colord, std::size_t, std::size_t) = 0;
		/// Sets the font family of the specified range of text.
		virtual void set_font_family(const std::u8string&, std::size_t, std::size_t) = 0;
		/// Sets the font size of the specified range of text.
		virtual void set_font_size(double, std::size_t, std::size_t) = 0;
		/// Sets the font style of the specified range of text.
		virtual void set_font_style(font_style, std::size_t, std::size_t) = 0;
		/// Sets the font weight of the specified range of text.
		virtual void set_font_weight(font_weight, std::size_t, std::size_t) = 0;
		/// Sets the font stretch of the specified range of text.
		virtual void set_font_stretch(font_stretch, std::size_t, std::size_t) = 0;
	};

	/// A font in a font family. The "EM unit" used here is slightly different from the Wikipedia definition:
	/// multiplying the EM unit by font size gives the length in device-independent pixels.
	class font {
	public:
		/// Default virtual destructor.
		virtual ~font() = default;

		/// Returns the distance between the top of the line and the baseline in em units.
		[[nodiscard]] virtual double get_ascent_em() const = 0;
		/// Returns the recommended height of a line in em units.
		[[nodiscard]] virtual double get_line_height_em() const = 0;

		/// Returns whether this font contains a glyph for the given codepoint.
		[[nodiscard]] virtual bool has_character(codepoint) const = 0;

		/// Returns the width of the given character.
		[[nodiscard]] virtual double get_character_width_em(codepoint) const = 0;
		/// Returns the maximum width of all given characters.
		[[nodiscard]] virtual double get_maximum_character_width_em(std::basic_string_view<codepoint> str) const {
			double res = std::numeric_limits<double>::min();
			for (codepoint cp : str) {
				res = std::max(res, get_character_width_em(cp));
			}
			return res;
		}
	};

	/// Represents a family of similar fonts.
	class font_family {
	public:
		/// Default virtual destructor.
		virtual ~font_family() = default;

		/// Returns a font in this family matching the given description.
		[[nodiscard]] virtual std::unique_ptr<font> get_matching_font(
			font_style, font_weight, font_stretch
		) const = 0;
	};

	/// Represents a single line of text with the same font parameters. This is mainly used for code editors.
	class plain_text {
	public:
		/// Default virtual destructor.
		virtual ~plain_text() = default;

		/// Returns the total width of this text clip.
		[[nodiscard]] virtual double get_width() const = 0;

		/// Retrieves information about the character that is below the given horizontal position.
		[[nodiscard]] virtual caret_hit_test_result hit_test(double) const = 0;
		/// Returns the space occupied by the character at the given position.
		[[nodiscard]] virtual rectd get_character_placement(std::size_t) const = 0;
	};

	/// Basic interface used to construct path geometries. There should be only one instance of this object for a
	/// renderer at any time.
	class path_geometry_builder {
	public:
		/// Default constructor.
		path_geometry_builder() = default;
		/// No move construction.
		path_geometry_builder(path_geometry_builder&&) = delete;
		/// No copy construction.
		path_geometry_builder(const path_geometry_builder&) = delete;
		/// No move assignment.
		path_geometry_builder &operator=(path_geometry_builder&&) = delete;
		/// No copy assignment.
		path_geometry_builder &operator=(const path_geometry_builder&) = delete;
		/// Default virtual destructor.
		virtual ~path_geometry_builder() = default;

		/// Closes and ends the current sub-path.
		virtual void close() = 0;
		/// Moves to the given position and starts a new sub-path.
		virtual void move_to(vec2d) = 0;

		/// Adds a segment from the current position to the given position.
		virtual void add_segment(vec2d) = 0;
		/// Adds a cubic bezier segment.
		virtual void add_cubic_bezier(vec2d to, vec2d control1, vec2d control2) = 0;
		/// Adds an arc, a part of a ellipse.
		///
		/// \param to The ending point of this arc.
		/// \param radius The radius before rotation.
		/// \param rotation The clockwise rotation of the ellipse.
		/// \param dir The sweep direction of this arc.
		/// \param type Indicates whether this is a major arc or a minor one.
		virtual void add_arc(vec2d to, vec2d radius, double rotation, sweep_direction dir, arc_type type) = 0;
	};

	// brush parameters
	/// Stores information about a gradient stop.
	struct gradient_stop {
		/// Default constructor.
		gradient_stop() = default;
		/// Initializes all fields of this struct.
		gradient_stop(colord c, double pos) : color(c), position(pos) {
		}

		colord color; ///< The color of this gradient stop.
		double position = 0.0; ///< The position of this gradient.
	};
}
namespace codepad::json {
	/// Parser for \ref ui::gradient_stop.
	template <> struct default_parser<ui::gradient_stop> {
		/// The main parser interface.
		template <typename ValueType> std::optional<ui::gradient_stop> operator()(const ValueType &val) const {
			std::optional<double> pos;
			std::optional<colord> color;
			if (auto object = val.template try_cast<typename ValueType::object_type>()) {
				if (object->size() > 2) {
					val.template log<log_level::warning>(CP_HERE) << "redundant fields in gradient stop definition";
				}
				pos = object->template parse_member<double>(u8"position");
				color = object->template parse_member<colord>(u8"color");
			} else if (auto arr = val.template try_cast<typename ValueType::array_type>()) {
				if (arr->size() >= 2) {
					if (arr->size() > 2) {
						val.template log<log_level::warning>(CP_HERE) <<
							"redundant data in gradient stop definition";
					}
					pos = arr->at(0).template parse<double>();
					color = arr->at(1).template parse<colord>();
				} else {
					val.template log<log_level::error>(CP_HERE) <<
						"not enough information in gradient stop definition";
				}
			} else {
				val.template log<log_level::error>(CP_HERE) << "invalid gradient stop format";
			}
			if (pos && color) {
				return ui::gradient_stop(color.value(), pos.value());
			}
			return std::nullopt;
		}
	};
}

namespace codepad::ui {
	using gradient_stop_collection = std::vector<gradient_stop>; ///< A list of gradient stops.
	/// Structures used to stores the parameters of a brush.
	namespace brush_parameters {
		/// Defines a brush that paints the region with the same color.
		struct solid_color {
			/// Default constructor.
			solid_color() = default;
			/// Initializes \ref color.
			explicit solid_color(colord c) : color(c) {
			}

			colord color; ///< The color of this brush.
		};
		/// Defines a brush with linear color gradients.
		struct linear_gradient {
			/// Default constructor.
			linear_gradient() = default;
			/// Initializes all fields of this struct.
			linear_gradient(vec2d f, vec2d t, const gradient_stop_collection &col) : from(f), to(t), gradients(&col) {
			}

			vec2d
				from, ///< The point where the gradient starts.
				to; ///< The point where the gradient stops.
			const gradient_stop_collection *gradients = nullptr; ///< The list of gradient stops.
		};
		/// Defines a brush with radial color gradients.
		struct radial_gradient {
			/// Default constructor.
			radial_gradient() = default;
			/// Initializes all fields of this struct.
			radial_gradient(vec2d c, double r, const gradient_stop_collection &col) :
				center(c), radius(r), gradients(&col) {
			}

			vec2d center; ///< The center of the circles.
			double radius = 0.0; ///< The radius of the circle.
			const gradient_stop_collection *gradients = nullptr; ///< The list of gradient stops.
		};
		/// Defines a bitmap brush.
		struct bitmap_pattern {
			/// Default constructor.
			bitmap_pattern() = default;
			/// Initializes \ref image.
			explicit bitmap_pattern(bitmap *im) : image(im) {
			}

			bitmap *image = nullptr; ///< The source image.
		};
		/// No brush.
		struct none {
		};
	}
	/// Generic brush parameters, together with the transform of the brush.
	struct generic_brush_parameters {
		/// The value type.
		using value_type = std::variant<
			brush_parameters::none,
			brush_parameters::solid_color,
			brush_parameters::linear_gradient,
			brush_parameters::radial_gradient,
			brush_parameters::bitmap_pattern
		>;

		/// Default constructor. Initializes \ref transform to be the identity transform.
		generic_brush_parameters() {
			transform.set_identity();
		}
		/// Initializes \ref value with a specific type of brush.
		template <typename Brush> generic_brush_parameters(Brush b) :
			value(std::in_place_type<Brush>, std::move(b)) {
			transform.set_identity();
		}
		/// Initializes \ref value with a specific type of brush, and \ref transform.
		template <typename Brush> generic_brush_parameters(Brush b, matd3x3 trans) :
			value(std::in_place_type<Brush>, std::move(b)), transform(trans) {
		}

		value_type value; ///< The value of this brush.
		matd3x3 transform; ///< The transform of this brush.
	};
	/// A pen, defined using a brush.
	struct generic_pen_parameters {
		/// Default constructor.
		generic_pen_parameters() = default;
		/// Initializes all fields of this struct.
		explicit generic_pen_parameters(generic_brush_parameters b, double t = 1.0) :
			brush(std::move(b)), thickness(t) {
		}

		generic_brush_parameters brush; ///< The brush.
		double thickness = 1.0; ///< The thickness of this pen.
	};


	/// Basic interface of a renderer.
	class renderer_base {
		friend window_base;
	public:
		/// Default virtual destructor.
		virtual ~renderer_base() = default;

		/// Creates a new render target of the given size and scaling factor.
		virtual render_target_data create_render_target(vec2d size, vec2d scaling_factor) = 0;

		/// Loads a \ref bitmap from disk. The second parameter specifies the scaling factor of this bitmap.
		virtual std::unique_ptr<bitmap> load_bitmap(const std::filesystem::path&, vec2d) = 0;

		/// Returns a font family identified by its name.
		virtual std::unique_ptr<font_family> find_font_family(const std::u8string&) = 0;

		/// Starts drawing to the given window.
		virtual void begin_drawing(window_base&) = 0;
		/// Starts drawing to the given \ref render_target.
		virtual void begin_drawing(render_target&) = 0;
		/// Finishes drawing to the last render target on which \ref begin_drawing() has been called.
		virtual void end_drawing() = 0;

		/// Clears the current surface using the given color.
		virtual void clear(colord) = 0;

		// transform
		/// Pushes a new matrix onto the stack for subsequent drawing operations.
		virtual void push_matrix(matd3x3) = 0;
		/// Multiplies the current matrix with the given matrix and pushes it onto the stack for subsequent drawing
		/// operations. Note that this matrix is multiplied as the right hand side, i.e., M * M' * v, where M is the
		/// current matrix, M' is the given matrix, and v is the vector being transformed. Thus, this transform is
		/// applied *before* previous transforms.
		virtual void push_matrix_mult(matd3x3) = 0;
		/// Pops a matrix from the stack.
		virtual void pop_matrix() = 0;
		/// Returns the current transformation matrix.
		[[nodiscard]] virtual matd3x3 get_matrix() const = 0;

		// geometry drawing & building
		/// Starts to build a path. Other drawing functions should *not* be used until the path has been finished.
		virtual path_geometry_builder &start_path() = 0;

		/// Draws a ellipse.
		virtual void draw_ellipse(
			vec2d center, double radiusx, double radiusy,
			const generic_brush_parameters &brush, const generic_pen_parameters &pen
		) = 0;
		/// Draws a rectangle.
		virtual void draw_rectangle(
			rectd rect, const generic_brush_parameters &brush, const generic_pen_parameters &pen
		) = 0;
		/// Draws a rounded rectangle.
		virtual void draw_rounded_rectangle(
			rectd region, double radiusx, double radiusy,
			const generic_brush_parameters &brush, const generic_pen_parameters &pen
		) = 0;
		/// Finishes building the current path and draws it. The path will then be discarded.
		virtual void end_and_draw_path(const generic_brush_parameters&, const generic_pen_parameters&) = 0;

		// clipping
		/// Pushes an ellipse clip.
		virtual void push_ellipse_clip(vec2d center, double radiusx, double radiusy) = 0;
		/// Pushes a rectangle clip.
		virtual void push_rectangle_clip(rectd rect) = 0;
		/// Pushes a rounded rectangle clip.
		virtual void push_rounded_rectangle_clip(rectd rect, double radiusx, double radiusy) = 0;
		/// Finishes building the current path and pushes it as a clip. The path will then be discarded.
		virtual void end_and_push_path_clip() = 0;
		/// Pops a previously pushed clip.
		virtual void pop_clip() = 0;

		// formatted text related
		/// Creates a new \ref formatted_text from the given parameters, using the given font parameters and color
		/// for the entire clip of text.
		virtual std::unique_ptr<formatted_text> create_formatted_text(
			std::u8string_view, const font_parameters&, colord, vec2d, wrapping_mode,
			horizontal_text_alignment, vertical_text_alignment
		) = 0;
		/// \ref create_formatted_text() that accepts a UTF-32 string.
		virtual std::unique_ptr<formatted_text> create_formatted_text(
			std::basic_string_view<codepoint>, const font_parameters&, colord, vec2d, wrapping_mode,
			horizontal_text_alignment, vertical_text_alignment
		) = 0;
		/// Draws the given \ref formatted_text at the given position using the given brush. The position indicates
		/// the top left corner of the layout box.
		virtual void draw_formatted_text(const formatted_text&, vec2d) = 0;

		// plain text related
		/// Creates a new \ref plain_text from the given parameters.
		virtual std::unique_ptr<plain_text> create_plain_text(std::u8string_view, font&, double font_size) = 0;
		/// \ref create_plain_text() that accepts a UTF-32 string.
		virtual std::unique_ptr<plain_text> create_plain_text(
			std::basic_string_view<codepoint>, font&, double font_size
		) = 0;
		/// Draws the given \ref plain_text at the given position, using the given color.
		virtual void draw_plain_text(const plain_text&, vec2d, colord) = 0;
	protected:
		/// Called to register the creation of a window.
		virtual void _new_window(window_base&) = 0;
		/// Called to register the deletion of a window.
		virtual void _delete_window(window_base&) = 0;

		/// Returns a reference to the renderer-specific data of the given window.
		static std::any &_get_window_data(window_base&);
	};
}
