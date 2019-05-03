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

#include "../core/misc.h"

namespace codepad::ui {
	class window_base;

	/// Basic interface of an bitmap.
	class bitmap {
	public:
		/// Default virtual destructor.
		virtual ~bitmap() = default;

		/// Returns the size of this bitmap.
		virtual vec2d get_size() const = 0;
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
			render_target(std::move(rt)), bitmap(std::move(bmp)) {
		}

		std::unique_ptr<render_target> render_target; ///< The \ref render_target.
		std::unique_ptr<bitmap> bitmap; ///< The \ref bitmap.
	};

	/// Basic interface of the formatting of text, with determined font, size, style, and weight.
	class text_format {
	public:
		/// Default virtual destructor.
		virtual ~text_format() = default;
	};
	/// A piece of text whose format has been calculated and cached to speed up rendering and measuring operations.
	class formatted_text {
	public:
		/// Stores the result of hit test operations.
		struct hit_test_result {
			size_t character = 0; ///< The character index that the given point is on.
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

		/// Default virtual destructor.
		virtual ~formatted_text() = default;

		/// Returns the region occupied by the text in the layout region.
		virtual rectd get_layout() const = 0;
		/// Returns the metrics of all lines.
		virtual std::vector<line_metrics> get_line_metrics() const = 0;
		/*virtual std::pair<size_t, bool> hit_test(vec2d) const = 0;*/
	};

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
		wrap ///< Wrap, but in a unspecified manner.
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
		/// Adds an arc (part of a circle).
		virtual void add_arc(vec2d to, double radius, sweep_direction dir, arc_type type) = 0;
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
			double radius; ///< The radius of the circle.
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
		template <typename Brush> explicit generic_brush_parameters(Brush b) :
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

		/// Creates a new render target of the given size.
		virtual render_target_data create_render_target(vec2d) = 0;

		/// Loads a \ref bitmap from disk.
		virtual std::unique_ptr<bitmap> load_bitmap(const std::filesystem::path&) = 0;

		/// Returns a pointer to the font identified by its name. The font may either be cached and returned
		/// directly, or loaded on demand.
		virtual std::unique_ptr<text_format> create_text_format(
			str_view_t, double, font_style, font_weight, font_stretch
		) = 0;

		/// Starts drawing to the given window.
		virtual void begin_drawing(window_base&) = 0;
		/// Starts drawing to the given \ref render_target.
		virtual void begin_drawing(render_target&) = 0;
		/// Finishes drawing to the last render target on which \ref begin_drawing() has been called.
		virtual void end_drawing() = 0;

		/// Pushes a new matrix onto the stack for subsequent drawing operations.
		virtual void push_matrix(matd3x3) = 0;
		/// Multiplies the current matrix with the given matrix and pushes it onto the stack for subsequent drawing
		/// operations. Note that this matrix is multiplied as the right hand side, i.e., M * M' * v, where M is the
		/// current matrix, M' is the given matrix, and v is the vector being transformed. Thus, this transform is
		/// applied \emph before previous transforms.
		virtual void push_matrix_mult(matd3x3) = 0;
		/// Pops a matrix from the stack.
		virtual void pop_matrix() = 0;

		/// Clears the current surface using the given color.
		virtual void clear(colord) = 0;

		/// Draws a \ref ellipse_geometry.
		virtual void draw_ellipse(
			vec2d center, double radiusx, double radiusy,
			const generic_brush_parameters &brush, const generic_pen_parameters &pen
		) = 0;
		/// Draws a \ref rectangle_geometry.
		virtual void draw_rectangle(
			rectd rect, const generic_brush_parameters &brush, const generic_pen_parameters &pen
		) = 0;
		/// Draws a \ref rounded_rectangle_geometry.
		virtual void draw_rounded_rectangle(
			rectd region, double radiusx, double radiusy,
			const generic_brush_parameters &brush, const generic_pen_parameters &pen
		) = 0;
		/// Starts to build a path.
		virtual path_geometry_builder &start_path() = 0;
		/// Finishes building the given path and draws it. The path will then be discarded.
		virtual void end_and_draw_path(const generic_brush_parameters&, const generic_pen_parameters&) = 0;

		/// Calculates the format of the given text using the given parameters, to speed up operations such as size
		/// querying and hit testing.
		virtual std::unique_ptr<formatted_text> format_text(
			str_view_t, text_format&, vec2d, wrapping_mode, horizontal_text_alignment, vertical_text_alignment
		) = 0;
		/// \ref format_text() that accepts a UTF-32 string.
		virtual std::unique_ptr<formatted_text> format_text(
			std::basic_string_view<codepoint>, text_format&, vec2d, wrapping_mode,
			horizontal_text_alignment, vertical_text_alignment
		) = 0;
		/// Draws the given \ref formatted_text at the given position using the given brush. The position indicates
		/// the top left corner of the layout box.
		virtual void draw_formatted_text(formatted_text&, vec2d, const generic_brush_parameters&) = 0;
		/// Shorthand for a combination of \ref format_text() and \ref draw_formatted_text(). Implementations may
		/// override this to reduce intermediate steps.
		virtual void draw_text(
			str_view_t text, rectd layout,
			text_format &format, wrapping_mode wrap,
			horizontal_text_alignment halign, vertical_text_alignment valign,
			const generic_brush_parameters &brush
		) {
			auto fmt = format_text(text, format, layout.size(), wrap, halign, valign);
			draw_formatted_text(*fmt, layout.xmin_ymin(), brush);
		}
		/// \ref draw_text() that accepts a UTF-32 string.
		virtual void draw_text(
			std::basic_string_view<codepoint> text, rectd layout,
			text_format &format, wrapping_mode wrap,
			horizontal_text_alignment halign, vertical_text_alignment valign,
			const generic_brush_parameters &brush
		) {
			auto fmt = format_text(text, format, layout.size(), wrap, halign, valign);
			draw_formatted_text(*fmt, layout.xmin_ymin(), brush);
		}
	protected:
		/// Called to register the creation of a window.
		virtual void _new_window(window_base&) = 0;
		/// Called to register the deletion of a window.
		virtual void _delete_window(window_base&) = 0;

		/// Returns a reference to the renderer-specific data of the given window.
		static std::any &_get_window_data(window_base&);
	};
}
