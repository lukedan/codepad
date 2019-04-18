// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes and structs used to determine the layout and visual parameters of an \ref codepad::ui::element.

#include <map>
#include <vector>
#include <functional>

#include "misc.h"
#include "animation_path.h"

namespace codepad::ui {
	/// Defines a position or offset relative to a region.
	struct relative_vec2d {
		vec2d
			/// The relative offset in a specific region, (0, 0) for the top-left corner, and (1, 1) for the
			/// bottom-right corner.
			relative,
			absolute; ///< The absolute offset in addition to \ref relative.

		/// Returns the absolute position given the containing rectangle.
		vec2d get_absolute_position(rectd layout) const {
			return layout.xmin_ymin() + get_absolute_offset(layout.size());
		}
		/// Returns the absolute offset given the size of the containing region.
		vec2d get_absolute_offset(vec2d size) const {
			return absolute + vec2d(size.x * relative.x, size.y * relative.y);
		}
	};
	/// Defines a length relative to that of a region.
	struct relative_double {
		double
			relative = 0.0, ///< The relative part of the length.
			absolute = 0.0; ///< The absolute part of the length.

		/// Returns the absolute length given that of the region.
		double get_absolute(double total) const {
			return relative * total + absolute;
		}
	};
	namespace json_object_parsers {
		/// Parses a \ref relative_vec2d. The node can either be its full representation
		/// (<tt>{"absolute": [x, y], "relative": [x, y]}</tt>), or a list of two vectors with the relative value in
		/// the front (<tt>[[absx, absy], [relx, rely]]</tt>), or a single vector indicating the relative value.
		template <typename Value> inline bool try_parse(const Value &obj, relative_vec2d &v) {
			if (typename Value::object_t full; json::try_cast(obj, full)) {
				// {"absolute": [x, y], "relative": [x, y]}
				if (auto fmem = full.find_member(u8"absolute"); fmem != full.member_end()) {
					try_parse(fmem.value(), v.absolute);
				}
				if (auto fmem = full.find_member(u8"relative"); fmem != full.member_end()) {
					try_parse(fmem.value(), v.relative);
				}
				return true;
			}
			if (typename Value::array_t arr; json::try_cast(obj, arr) && arr.size() >= 2) {
				// [[relx, rely], [absx, absy]]
				if (arr[0].is<typename Value::array_t>() && arr[1].is<typename Value::array_t>()) {
					try_parse(arr[0], v.relative);
					try_parse(arr[1], v.absolute);
					return true;
				}
			}
			// absolute only
			v.relative = vec2d();
			return try_parse(obj, v.absolute);
		}
		/// Parses a \ref relative_double. The format is similar to that of \ref relative_vec2d.
		template <typename Value> inline bool try_parse(const Value &obj, relative_double &d) {
			if (typename Value::object_t full; json::try_cast(obj, full)) { // full representation
				json::try_cast_member(full, u8"absolute", d.absolute);
				json::try_cast_member(full, u8"relative", d.relative);
				return true;
			}
			if (typename Value::array_t arr; json::try_cast(obj, arr) && arr.size() >= 2) { // a list of two doubles
				json::try_cast(arr[0], d.relative);
				json::try_cast(arr[1], d.absolute);
				return true;
			}
			// absolute only
			d.relative = 0.0;
			return json::try_cast(obj, d.absolute);
		}
	}

	/// Various types of transforms.
	namespace transforms {
		/// Transformation defined by a matrix.
		struct raw {
			/// Sets \ref matrix to be an identity matrix.
			raw() {
				matrix.set_identity();
			}
			/// Initializes \ref matrix.
			explicit raw(matd3x3 m) : matrix(m) {
			}

			/// Returns \ref matrix.
			matd3x3 get_matrix(rectd) const {
				return matrix;
			}
			/// Transforms the given point.
			vec2d transform_point(vec2d pt, rectd) const {
				return matrix.transform(pt);
			}

			matd3x3 matrix; ///< The matrix.
		};
		/// Transformation that translates an object.
		struct translation {
			/// Default constructor.
			translation() = default;
			/// Initializes \ref translation.
			explicit translation(relative_vec2d trans) : offset(trans) {
			}

			/// Returns a \ref matd3x3 that represents this transform.
			matd3x3 get_matrix(rectd layout) const {
				return matd3x3::translate(offset.get_absolute_offset(layout.size()));
			}
			/// Translates the given point.
			vec2d transform_point(vec2d pt, rectd layout) const {
				return pt + offset.get_absolute_offset(layout.size());
			}

			relative_vec2d offset; ///< The translation.
		};
		/// Transformation that scales an object.
		struct scale {
			/// Initializes this transform to be the identity transform.
			scale() : scale_factor(1.0, 1.0) {
			}
			/// Initializes all fields of this struct.
			scale(relative_vec2d c, vec2d s) : center(c), scale_factor(s) {
			}

			/// Returns a \ref matd3x3 that represents this transform.
			matd3x3 get_matrix(rectd layout) const {
				return matd3x3::scale(center.get_absolute_position(layout), scale_factor);
			}
			/// Scales the given point.
			vec2d transform_point(vec2d pt, rectd layout) const {
				vec2d c = center.get_absolute_position(layout);
				pt -= c;
				pt.x *= scale_factor.x;
				pt.y *= scale_factor.y;
				return pt + c;
			}

			relative_vec2d center; ///< The center, which is not affected by this transform.
			vec2d scale_factor; ///< The scale factor of both orientations.
		};
		/// Transformation that rotates an object clockwise.
		struct rotation {
			/// Default constructor.
			rotation() = default;
			/// Initializes all fields of this struct.
			rotation(relative_vec2d c, double a) : center(c), angle(a) {
			}

			/// Returns a \ref matd3x3 that represents this transform.
			matd3x3 get_matrix(rectd layout) const {
				return matd3x3::rotate_clockwise(center.get_absolute_position(layout), angle);
			}
			/// Rotates the given point.
			vec2d transform_point(vec2d pt, rectd layout) const {
				vec2d vec(std::cos(angle), std::sin(angle)), c = center.get_absolute_position(layout);
				pt -= c;
				pt = vec2d(pt.x * vec.x - pt.y * vec.y, pt.x * vec.y + pt.y * vec.x);
				return pt + c;
			}

			relative_vec2d center; ///< The center of rotation.
			double angle = 0.0; ///< The angle to roatate, in radians.
		};

		struct generic;
		/// A collection of \ref transforms::generic that are applied in order.
		struct collection {
			/// Returns the combined transformation matrix.
			matd3x3 get_matrix(rectd) const;
			/// Transforms the given point. This is performed by calling \p transform_point() for each component.
			vec2d transform_point(vec2d, rectd) const;

			std::vector<generic> components; ///< The list of transforms.
		};

		/// A generic, polymorphic transform.
		struct generic {
			/// Holds a generic transformation.
			using value_type = std::variant<raw, translation, scale, rotation, collection>;

			/// Returns the transformation matrix.
			matd3x3 get_matrix(rectd layout) const {
				return std::visit([&layout](auto && trans) {
					return trans.get_matrix(layout);
					}, value);
			}
			/// Transforms the given point.
			vec2d transform_point(vec2d pt, rectd layout) const {
				return std::visit([&pt, &layout](auto && trans) {
					return trans.transform_point(pt, layout);
					}, value);
			}

			value_type value; ///< The value of this transformation.
		};


		inline matd3x3 collection::get_matrix(rectd layout) const {
			matd3x3 res;
			res.set_identity();
			for (const generic &g : components) {
				res = g.get_matrix(layout) * res;
			}
			return res;
		}

		inline vec2d collection::transform_point(vec2d pt, rectd layout) const {
			for (const generic &g : components) {
				pt = g.transform_point(pt, layout);
			}
			return pt;
		}
	}


	/// Various types of brushes.
	namespace brushes {
		/// Corresponds to \ref brush_parameters::solid_color.
		struct solid_color {
			/// Returns the corresponding \ref brush_parameters::solid_color given the target region.
			brush_parameters::solid_color get_parameters(rectd) const {
				return brush_parameters::solid_color(color);
			}

			colord color; ///< \sa brush_parameters::solid_color::color
		};
		/// Corresponds to \ref brush_parameters::linear_gradient.
		struct linear_gradient {
			/// Returns the corresponding \ref brush_parameters::linear_gradient given the target region.
			brush_parameters::linear_gradient get_parameters(rectd layout) const {
				return brush_parameters::linear_gradient(
					from.get_absolute_position(layout), to.get_absolute_position(layout), gradient_stops
				);
			}

			gradient_stop_collection gradient_stops; ///< \sa brush_parameters::linear_gradient::gradients
			relative_vec2d
				from, ///< \sa brush_parameters::linear_gradient::from
				to; ///< \sa brush_parameters::linear_gradient::to
		};
		/// Corresponds to \ref brush_parameters::radial_gradient.
		struct radial_gradient {
			/// Returns the corresponding \ref brush_parameters::radial_gradient given the target region.
			brush_parameters::radial_gradient get_parameters(rectd layout) const {
				return brush_parameters::radial_gradient(
					center.get_absolute_position(layout), radius, gradient_stops
				);
			}

			gradient_stop_collection gradient_stops; ///< \sa brush_parameters::radial_gradient::gradients
			relative_vec2d center; ///< \sa brush_parameters::radial_gradient::center
			double radius; ///< \sa brush_parameters::radial_gradient::radius
		};
		/// Corresponds to \ref brush_parameters::bitmap_pattern.
		struct bitmap_pattern {
			/// Returns the corresponding \ref brush_parameters::bitmap_pattern given the target region.
			brush_parameters::bitmap_pattern get_parameters(rectd) const {
				return brush_parameters::bitmap_pattern(&*image);
			}

			std::shared_ptr<bitmap> image; ///< \sa brush_parameters::bitmap_pattern::image
		};
		/// Corresponds to \ref brush_parameters::none.
		struct none {
			/// Returns a new \ref brush_parameters::none object.
			brush_parameters::none get_parameters(rectd) const {
				return brush_parameters::none();
			}
		};
	}

	/// A generic brush.
	struct generic_brush {
		/// Stores the actual value of the brush.
		using value_type = std::variant<
			brushes::none,
			brushes::solid_color,
			brushes::linear_gradient,
			brushes::radial_gradient,
			brushes::bitmap_pattern
		>;

		/// Returns the corresponding brush parameters given the target region.
		generic_brush_parameters get_parameters(rectd layout) const {
			if (std::holds_alternative<brushes::none>(value)) {
				return generic_brush_parameters();
			}
			return std::visit([this, &layout](auto && b) {
				return generic_brush_parameters(b.get_parameters(layout), transform.get_matrix(layout));
				}, value);
		}

		value_type value; ///< The value of this generic brush.
		transforms::generic transform; ///< The transform of this brush.
	};
	/// A generic pen defined by a brush.
	struct generic_pen {
		/// Returns the corresponding pen parameters given the target region.
		generic_pen_parameters get_parameters(rectd layout) const {
			return generic_pen_parameters(brush.get_parameters(layout), thickness);
		}

		generic_brush brush; ///< The brush.
		double thickness; ///< The thickness of this pen.
	};

	/// Various types of geometries that can be used in the definition of an element's visuals.
	namespace geometries {
		/// A rectangular geometry.
		struct rectangle {
			relative_vec2d
				top_left, ///< The top-left corner of this rectangle.
				bottom_right; ///< The bottom-right corner of this rectangle.

			/// Draws this rectangle in the specified region with the specified bursh and pen.
			void draw(
				rectd layout, renderer_base &r,
				const generic_brush_parameters &brush, const generic_pen_parameters &pen
			) const {
				r.draw_rectangle(rectd::from_corners(
					top_left.get_absolute_position(layout), bottom_right.get_absolute_position(layout)
				), brush, pen);
			}
		};
		/// A rectangle geometry with rounded corners.
		struct rounded_rectangle {
			relative_vec2d
				top_left, ///< \sa rectangle::top_left
				bottom_right; ///< \sa rectangle::bottom_right
			relative_double
				radiusx, ///< The horizontal radius.
				radiusy; ///< The vertical radius.

			/// Draws this rounded rectangle in the specified region with the specified bursh and pen.
			void draw(
				rectd layout, renderer_base &r,
				const generic_brush_parameters &brush, const generic_pen_parameters &pen
			) const {
				r.draw_rounded_rectangle(
					rectd::from_corners(top_left.get_absolute_position(layout), bottom_right.get_absolute_position(layout)),
					radiusx.get_absolute(layout.width()), radiusy.get_absolute(layout.height()),
					brush, pen
				);
			}
		};
		/// An ellipse geometry.
		struct ellipse {
			relative_vec2d
				top_left, ///< The top-left corner of the bounding box of this ellipse.
				bottom_right; ///< The bottom-right corner of the bounding box of this ellipse.

			/// Draws this ellipse in the specified region with the specified bursh and pen.
			void draw(
				rectd layout, renderer_base &r,
				const generic_brush_parameters &brush, const generic_pen_parameters &pen
			) const {
				auto rgn = rectd::from_corners(top_left.get_absolute_position(layout), bottom_right.get_absolute_position(layout));
				r.draw_ellipse(rgn.center(), 0.5 * rgn.width(), 0.5 * rgn.height(), brush, pen);
			}
		};
		/// A path geometry that allows for more complicated geometries.
		struct path {
			// different parts
			/// A part of a \ref subpath that's a line segment.
			struct segment {
				relative_vec2d to; ///< The end point of the segment.

				/// Adds this part to a \ref path_geometry_builder.
				void add_to(path_geometry_builder &builder, rectd layout) const {
					builder.add_segment(to.get_absolute_position(layout));
				}
			};
			/// A part of a \ref subpath that's an arc.
			struct arc {
				relative_vec2d to; ///< The end point of this arc.
				double radius; ///< The radius of this arc.
				sweep_direction direction = sweep_direction::clockwise; ///< The sweep direction of this arc.
				arc_type type = arc_type::minor; ///< The type of this arc.

				/// Adds this part to a \ref path_geometry_builder.
				void add_to(path_geometry_builder &builder, rectd layout) const {
					builder.add_arc(to.get_absolute_position(layout), radius, direction, type);
				}
			};
			/// A part of a \ref subpath that's a cubic bezier.
			struct cubic_bezier {
				relative_vec2d
					to, ///< The end point of the bezier curve.
					control1, ///< The first control point.
					control2; ///< The second control point.

				/// Adds this part to a \ref path_geometry_builder.
				void add_to(path_geometry_builder &builder, rectd layout) const {
					builder.add_cubic_bezier(
						to.get_absolute_position(layout), control1.get_absolute_position(layout), control2.get_absolute_position(layout)
					);
				}
			};

			///< Contains a part of a \ref subpath.
			struct part {
				/// The type that holds a specific type of part.
				using value_type = std::variant<segment, arc, cubic_bezier>;

				/// Adds this part to a \ref path_geometry_builder.
				void add_to(path_geometry_builder &builder, rectd layout) const {
					std::visit([&](auto && obj) {
						obj.add_to(builder, layout);
						}, value);
				}

				value_type value; ///< The value of this part.
			};

			/// A part of a path, independent of all other sub-paths.
			struct subpath {
				std::vector<part> parts; ///< Parts of this subpath.
				relative_vec2d starting_point; ///< The starting point of this subpath.
				bool closed = false; ///< Whether this subpath should be closed when stroked.
			};

			std::vector<subpath> subpaths; ///< The list of sub-paths.

			/// Draws this path in the specified region with the specified brush and pen.
			void draw(
				rectd layout, renderer_base &r,
				const generic_brush_parameters &brush, const generic_pen_parameters &pen
			) const {
				path_geometry_builder &builder = r.start_path();
				for (const auto &sp : subpaths) {
					builder.move_to(sp.starting_point.get_absolute_position(layout));
					for (const auto &p : sp.parts) {
						p.add_to(builder, layout);
					}
					if (sp.closed) {
						builder.close();
					}
				}
				r.end_and_draw_path(brush, pen);
			}
		};
	}

	/// Describes a geometry and how it is rendered.
	struct generic_visual_geometry {
	public:
		using value_type = std::variant<
			geometries::rectangle, geometries::rounded_rectangle, geometries::ellipse, geometries::path
		>; ///< The definition of the value of this geometry.

		/// Draws this geometry in the specified region with the specified brush and pen.
		void draw(rectd layout, renderer_base &r) const {
			r.push_matrix_mult(transform.get_matrix(layout));
			std::visit([&](auto && obj) {
				obj.draw(layout, r, fill.get_parameters(layout), stroke.get_parameters(layout));
				}, value);
			r.pop_matrix();
		}

		value_type value; ///< The value of this geometry.
		transforms::generic transform; ///< The transform of this geometry.
		generic_brush fill; ///< The brush used to fill the geometry.
		generic_pen stroke; ///< The pen used to stroke the geometry.
	};


	/// Parameters that determines the visuals of an element.
	struct element_visual_parameters {
		std::vector<generic_visual_geometry> geometries; ///< The geometries used as the background of the element.
		transforms::generic transform; ///< The transform of the element.
	};
	/// Parameters that determine the layout of an element.
	struct element_layout_parameters {
		thickness
			margin, ///< The element's margin.
			padding; ///< The element's internal padding.
		vec2d size; ///< The element's size.
		anchor elem_anchor = anchor::all; ///< The element's anchor.
		size_allocation_type
			width_alloc = size_allocation_type::automatic, ///< Determines how the element's width is allocated.
			height_alloc = size_allocation_type::automatic; ///< Determines how the element's height is allocated.
	};

	/// Basic parameters that are used by all types of elements.
	struct element_parameters {
		element_visual_parameters visual_parameters; ///< The \ref element_visual_parameters.
		element_layout_parameters layout_parameters; ///< The \ref element_layout_parameters.
		visibility visibility = visibility::full; ///< The visibility of this element.
		cursor custom_cursor = cursor::not_specified; ///< The custom cursor of the element.
	};

	/// Contains configuration of an element's behavior.
	struct element_configuration {
	public:
		element_parameters default_parameters; ///< The default parameters for elements of this class.
		std::map<str_t, storyboard> event_triggers; ///< Animations that are played when events are triggered.
		/// Additional attributes that are dependent of the element's type.
		std::map<str_t, json::value_storage> additional_attributes;
	};
}
