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
		/// Returns whether this bitmap does not actually reference any object.
		virtual bool empty() const = 0;
	};
	/// Basic interface of an off-screen render target.
	class render_target {
	public:
		/// Default virtual destructor.
		virtual ~render_target() = default;

		/// Returns the \ref bitmap corresponding to this \ref render_target.
		virtual std::unique_ptr<bitmap> get_bitmap() const = 0;
		/// Returns whether this render target does not actually reference any object.
		virtual bool empty() const = 0;
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
		virtual std::unique_ptr<render_target> create_render_target(vec2d) = 0;

		/// Loads a \ref bitmap from disk.
		virtual std::unique_ptr<bitmap> load_bitmap(const std::filesystem::path&) = 0;

		/// Starts drawing to the given window.
		virtual void begin_drawing(window_base&) = 0;
		/// Starts drawing to the given \ref render_target.
		virtual void begin_drawing(render_target&) = 0;
		/// Finishes drawing to the last render target on which \ref begin_drawing() has been called.
		virtual void end_drawing() = 0;

		/// Pushes a new matrix onto the stack for subsequent drawing operations.
		virtual void push_matrix(matd3x3) = 0;
		/// Multiplies the current matrix with the given matrix and pushes it onto the stack for subsequent drawing
		/// operations.
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
	protected:
		/// Called to register the creation of a window.
		virtual void _new_window(window_base&) = 0;
		/// Called to register the deletion of a window.
		virtual void _delete_window(window_base&) = 0;

		/// Returns a reference to the renderer-specific data of the given window.
		static std::any &_get_window_data(window_base&);
	};
}
