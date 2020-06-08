// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes and structs used to define the layout and visual parameters of a \ref codepad::ui::element.

#include <map>
#include <vector>
#include <functional>

#include "misc.h"
#include "animation_path.h"
#include "renderer.h"

namespace codepad::ui {
	/// Defines a position or offset relative to a region.
	struct relative_vec2d {
		/// Default constructor.
		relative_vec2d() = default;
		/// Initializes all fields of this struct.
		relative_vec2d(vec2d rel, vec2d abs) : relative(rel), absolute(abs) {
		}

		vec2d
			/// The relative offset in a specific region, (0, 0) for the top-left corner, and (1, 1) for the
			/// bottom-right corner.
			relative,
			absolute; ///< The absolute offset in addition to \ref relative.

		/// Returns the absolute offset given the size of the containing region.
		vec2d get_absolute_offset(vec2d size) const {
			return absolute + vec2d(size.x * relative.x, size.y * relative.y);
		}

		/// In-place addition.
		relative_vec2d &operator+=(relative_vec2d rhs) {
			relative += rhs.relative;
			absolute += rhs.absolute;
			return *this;
		}
		/// Addition.
		friend relative_vec2d operator+(relative_vec2d lhs, relative_vec2d rhs) {
			return relative_vec2d(lhs.relative + rhs.relative, lhs.absolute + rhs.absolute);
		}

		/// In-place subtraction.
		relative_vec2d &operator-=(relative_vec2d rhs) {
			relative -= rhs.relative;
			absolute -= rhs.absolute;
			return *this;
		}
		/// Subtraction.
		friend relative_vec2d operator-(relative_vec2d lhs, relative_vec2d rhs) {
			return relative_vec2d(lhs.relative - rhs.relative, lhs.absolute - rhs.absolute);
		}

		/// Multiplication.
		friend relative_vec2d operator*(relative_vec2d lhs, double rhs) {
			return relative_vec2d(lhs.relative * rhs, lhs.absolute * rhs);
		}
		/// Multiplication.
		friend relative_vec2d operator*(double lhs, relative_vec2d rhs) {
			return rhs * lhs;
		}
		/// In-place multiplication.
		relative_vec2d &operator*=(double rhs) {
			relative *= rhs;
			absolute *= rhs;
			return *this;
		}

		/// Division.
		friend relative_vec2d operator/(relative_vec2d lhs, double rhs) {
			return relative_vec2d(lhs.relative / rhs, lhs.absolute / rhs);
		}
		/// In-place multiplication.
		relative_vec2d &operator/=(double rhs) {
			relative /= rhs;
			absolute /= rhs;
			return *this;
		}
	};
	/// Defines a length relative to that of a region.
	struct relative_double {
		/// Default constructor.
		relative_double() = default;
		/// Initializes all fields of this struct.
		relative_double(double rel, double abs) : relative(rel), absolute(abs) {
		}

		double
			relative = 0.0, ///< The relative part of the length.
			absolute = 0.0; ///< The absolute part of the length.

		/// Returns the absolute length given that of the region.
		double get_absolute(double total) const {
			return relative * total + absolute;
		}

		/// In-place addition.
		relative_double &operator+=(relative_double rhs) {
			relative += rhs.relative;
			absolute += rhs.absolute;
			return *this;
		}
		/// Addition.
		friend relative_double operator+(relative_double lhs, relative_double rhs) {
			return relative_double(lhs.relative + rhs.relative, lhs.absolute + rhs.absolute);
		}

		/// In-place subtraction.
		relative_double &operator-=(relative_double rhs) {
			relative -= rhs.relative;
			absolute -= rhs.absolute;
			return *this;
		}
		/// Subtraction.
		friend relative_double operator-(relative_double lhs, relative_double rhs) {
			return relative_double(lhs.relative - rhs.relative, lhs.absolute - rhs.absolute);
		}

		/// Multiplication.
		friend relative_double operator*(relative_double lhs, double rhs) {
			return relative_double(lhs.relative * rhs, lhs.absolute * rhs);
		}
		/// Multiplication.
		friend relative_double operator*(double lhs, relative_double rhs) {
			return rhs * lhs;
		}
		/// In-place multiplication.
		relative_double &operator*=(double rhs) {
			relative *= rhs;
			absolute *= rhs;
			return *this;
		}

		/// Division.
		friend relative_double operator/(relative_double lhs, double rhs) {
			return relative_double(lhs.relative / rhs, lhs.absolute / rhs);
		}
		/// In-place multiplication.
		relative_double &operator/=(double rhs) {
			relative /= rhs;
			absolute /= rhs;
			return *this;
		}
	};
}
namespace codepad::json {
	/// Parser for \ref ui::relative_vec2d.
	template <> struct default_parser<ui::relative_vec2d> {
		/// Parses a \ref ui::relative_vec2d. The node can either be its full representation
		/// (<tt>{"absolute": [x, y], "relative": [x, y]}</tt>), or a list of two vectors with the relative value in
		/// the front (<tt>[[relx, rely], [absx, absy]]</tt>), or a single vector indicating the absolute value.
		template <typename Value> std::optional<ui::relative_vec2d> operator()(const Value&) const;
	};
	/// Parser for \ref ui::relative_double.
	template <> struct default_parser<ui::relative_double> {
		/// Parses a \ref ui::relative_double. The format is similar to that of \ref ui::relative_vec2d.
		template <typename Value> std::optional<ui::relative_double> operator()(const Value&) const;
	};
}

namespace codepad::ui {
	/// Various types of transforms.
	namespace transforms {
		/// The identity transform.
		struct identity {
			/// Returns the identity matrix.
			matd3x3 get_matrix(vec2d) const {
				return matd3x3::identity();
			}
			/// Returns the original point.
			vec2d transform_point(vec2d pt, vec2d) const {
				return pt;
			}
			/// Returns the original point.
			vec2d inverse_transform_point(vec2d pt, vec2d) const {
				return pt;
			}
		};
		/// Transformation that translates an object.
		struct translation {
			/// Default constructor.
			translation() = default;
			/// Initializes \ref translation.
			explicit translation(relative_vec2d trans) : offset(trans) {
			}

			/// Returns a \ref matd3x3 that represents this transform.
			matd3x3 get_matrix(vec2d unit) const {
				return matd3x3::translate(offset.get_absolute_offset(unit));
			}
			/// Translates the given point.
			vec2d transform_point(vec2d pt, vec2d unit) const {
				return pt + offset.get_absolute_offset(unit);
			}
			/// Translates the given point in the opposite direction.
			vec2d inverse_transform_point(vec2d pt, vec2d unit) const {
				return pt - offset.get_absolute_offset(unit);
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
			matd3x3 get_matrix(vec2d unit) const {
				return matd3x3::scale(center.get_absolute_offset(unit), scale_factor);
			}
			/// Scales the given point.
			vec2d transform_point(vec2d pt, vec2d unit) const {
				vec2d c = center.get_absolute_offset(unit);
				pt -= c;
				pt.x *= scale_factor.x;
				pt.y *= scale_factor.y;
				return pt + c;
			}
			/// Scales the given point using the inverse scale factor.
			vec2d inverse_transform_point(vec2d pt, vec2d unit) const {
				vec2d c = center.get_absolute_offset(unit);
				pt -= c;
				pt.x /= scale_factor.x;
				pt.y /= scale_factor.y;
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
			matd3x3 get_matrix(vec2d unit) const {
				return matd3x3::rotate_clockwise(center.get_absolute_offset(unit), angle);
			}
			/// Rotates the given point.
			vec2d transform_point(vec2d pt, vec2d unit) const {
				vec2d vec(std::cos(angle), std::sin(angle)), c = center.get_absolute_offset(unit);
				pt -= c;
				pt = vec2d(pt.x * vec.x - pt.y * vec.y, pt.x * vec.y + pt.y * vec.x);
				return pt + c;
			}
			/// Rotates the given point in the opposite direction.
			vec2d inverse_transform_point(vec2d pt, vec2d unit) const {
				vec2d vec(std::cos(angle), -std::sin(angle)), c = center.get_absolute_offset(unit);
				pt -= c;
				pt = vec2d(pt.x * vec.x - pt.y * vec.y, pt.x * vec.y + pt.y * vec.x);
				return pt + c;
			}

			relative_vec2d center; ///< The center of rotation.
			double angle = 0.0; ///< The angle to roatate, in radians.
		};
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
			matd3x3 get_matrix(vec2d) const {
				return matrix;
			}
			/// Transforms the given point.
			vec2d transform_point(vec2d pt, vec2d) const {
				return matrix.transform_position(pt);
			}
			/// Inverse transforms the given point.
			vec2d inverse_transform_point(vec2d pt, vec2d) const {
				return matrix.inverse().transform_position(pt);
			}

			matd3x3 matrix; ///< The matrix.
		};

		struct generic;
		/// A collection of \ref transforms::generic that are applied in order.
		struct collection {
			/// Returns the combined transformation matrix.
			matd3x3 get_matrix(vec2d) const;
			/// Transforms the given point. This is performed by calling \p transform_point() for each component.
			vec2d transform_point(vec2d, vec2d) const;
			/// Inverse transforms the given point. This is performed by calling \p inverse_transform_point() for
			/// each component in the reverse order.
			vec2d inverse_transform_point(vec2d, vec2d) const;

			std::vector<generic> components; ///< The list of transforms.
		};

		/// A generic, polymorphic transform.
		struct generic {
			/// Holds a generic transformation.
			using value_type = std::variant<identity, translation, scale, rotation, collection, raw>;

			/// Returns the transformation matrix.
			matd3x3 get_matrix(vec2d unit) const {
				return std::visit([&unit](auto &&trans) {
					return trans.get_matrix(unit);
					}, value);
			}
			/// Transforms the given point.
			vec2d transform_point(vec2d pt, vec2d unit) const {
				return std::visit([&pt, &unit](auto &&trans) {
					return trans.transform_point(pt, unit);
					}, value);
			}
			/// Inverse transforms the given point.
			vec2d inverse_transform_point(vec2d pt, vec2d unit) const {
				return std::visit([&pt, &unit](auto &&trans) {
					return trans.inverse_transform_point(pt, unit);
					}, value);
			}

			/// Creates a new generic trasnform that contains the given specific type of transform.
			template <typename Trans, typename ...Args> inline static generic make(Args &&...args) {
				generic res;
				res.value.emplace<Trans>(std::forward<Args>(args)...);
				return res;
			}

			value_type value; ///< The value of this transformation.
		};
	}
}
namespace codepad::json {
	/// Parser for \ref ui::transforms::generic.
	template <> struct default_parser<ui::transforms::generic> {
		/// Parses a generic transform. The value can either be:
		///  - A list, whcih is interpreted as a transform collection.
		///  - An object with a member named either `trasnaltion', `scale', `rotation', or `children'. These members
		///    are checked in order and only the first one is handled.
		template <typename Value> std::optional<ui::transforms::generic> operator()(const Value&) const;
	};
}


/// Various types of brushes.
namespace codepad::ui::brushes {
	/// Corresponds to \ref brush_parameters::solid_color.
	struct solid_color {
		/// Default constructor.
		solid_color() = default;
		/// Initializes all fields of this struct.
		explicit solid_color(colord c) : color(c) {
		}

		/// Returns the corresponding \ref brush_parameters::solid_color given the target region.
		brush_parameters::solid_color get_parameters(vec2d) const {
			return brush_parameters::solid_color(color);
		}

		colord color; ///< \sa brush_parameters::solid_color::color
	};
	/// Corresponds to \ref brush_parameters::linear_gradient.
	struct linear_gradient {
		/// Default constructor.
		linear_gradient() = default;
		/// Initializes all fields of this struct.
		linear_gradient(relative_vec2d f, relative_vec2d t, gradient_stop_collection gs) :
			gradient_stops(std::move(gs)), from(f), to(t) {
		}

		/// Returns the corresponding \ref brush_parameters::linear_gradient given the target region.
		brush_parameters::linear_gradient get_parameters(vec2d unit) const {
			return brush_parameters::linear_gradient(
				from.get_absolute_offset(unit), to.get_absolute_offset(unit), gradient_stops
			);
		}

		gradient_stop_collection gradient_stops; ///< \sa brush_parameters::linear_gradient::gradients
		relative_vec2d
			from, ///< \sa brush_parameters::linear_gradient::from
			to; ///< \sa brush_parameters::linear_gradient::to
	};
	/// Corresponds to \ref brush_parameters::radial_gradient.
	struct radial_gradient {
		/// Default constructor.
		radial_gradient() = default;
		/// Initializes all fields of this struct.
		radial_gradient(relative_vec2d c, double r, gradient_stop_collection gs) :
			gradient_stops(std::move(gs)), center(c), radius(r) {
		}

		/// Returns the corresponding \ref brush_parameters::radial_gradient given the target region.
		brush_parameters::radial_gradient get_parameters(vec2d unit) const {
			return brush_parameters::radial_gradient(
				center.get_absolute_offset(unit), radius, gradient_stops
			);
		}

		gradient_stop_collection gradient_stops; ///< \sa brush_parameters::radial_gradient::gradients
		relative_vec2d center; ///< \sa brush_parameters::radial_gradient::center
		double radius; ///< \sa brush_parameters::radial_gradient::radius
	};
	/// Corresponds to \ref brush_parameters::bitmap_pattern.
	struct bitmap_pattern {
		/// Default constructor.
		bitmap_pattern() = default;
		/// Initializes all fields of this struct.
		explicit bitmap_pattern(std::shared_ptr<bitmap> img) : image(std::move(img)) {
		}

		/// Returns the corresponding \ref brush_parameters::bitmap_pattern given the target region.
		brush_parameters::bitmap_pattern get_parameters(vec2d) const {
			return brush_parameters::bitmap_pattern(&*image);
		}

		std::shared_ptr<bitmap> image; ///< \sa brush_parameters::bitmap_pattern::image
	};
	/// Corresponds to \ref brush_parameters::none.
	struct none {
		/// Returns a new \ref brush_parameters::none object.
		brush_parameters::none get_parameters(vec2d) const {
			return brush_parameters::none();
		}
	};
}
namespace codepad::json {
	/// Parser for \ref ui::brushes::solid_color.
	template <> struct default_parser<ui::brushes::solid_color> {
		/// The parser interface.
		template <typename Value> std::optional<ui::brushes::solid_color> operator()(const Value&) const;
	};
	/// Parser for \ref ui::brushes::linear_gradient.
	template <> struct default_parser<ui::brushes::linear_gradient> {
		/// The parser interface.
		template <typename Value> std::optional<ui::brushes::linear_gradient> operator()(const Value&) const;
	};
	/// Parser for \ref ui::brushes::radial_gradient.
	template <> struct default_parser<ui::brushes::radial_gradient> {
		/// The parser interface.
		template <typename Value> std::optional<ui::brushes::radial_gradient> operator()(const Value&) const;
	};
}
namespace codepad::ui {
	/// Parser for \ref brushes::bitmap_pattern.
	template <> struct managed_json_parser<brushes::bitmap_pattern> {
	public:
		/// Initializes \ref _manager.
		explicit managed_json_parser(manager &m) : _manager(m) {
		}

		/// The parser interface.
		template <typename Value> std::optional<ui::brushes::bitmap_pattern> operator()(const Value&) const;
	protected:
		manager &_manager; ///< The associated \ref manager.
	};

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
		generic_brush_parameters get_parameters(vec2d unit) const {
			if (std::holds_alternative<brushes::none>(value)) {
				return generic_brush_parameters();
			}
			return std::visit([this, &unit](auto &&b) {
				return generic_brush_parameters(b.get_parameters(unit), transform.get_matrix(unit));
				}, value);
		}

		value_type value; ///< The value of this generic brush.
		transforms::generic transform; ///< The transform of this brush.
	};
	/// Used to parse a \ref generic_brush.
	template <> struct managed_json_parser<generic_brush> {
	public:
		/// Initializes \ref _manager.
		managed_json_parser(manager &m) : _manager(m) {
		}

		/// The parser interface.
		template <typename Value> std::optional<generic_brush> operator()(const Value&) const;
	protected:
		manager &_manager; ///< The associated \ref manager.
	};

	/// A generic pen defined by a brush.
	struct generic_pen {
		/// Returns the corresponding pen parameters given the target region.
		generic_pen_parameters get_parameters(vec2d unit) const {
			return generic_pen_parameters(brush.get_parameters(unit), thickness);
		}

		generic_brush brush; ///< The brush.
		double thickness = 1.0; ///< The thickness of this pen.
	};
	/// Used to parse a \ref generic_pen.
	template <> struct managed_json_parser<generic_pen> {
	public:
		/// Initializes \ref _manager.
		managed_json_parser(manager &m) : _manager(m) {
		}

		/// The parser interface.
		template <typename Value> std::optional<generic_pen> operator()(const Value&) const;
	protected:
		manager &_manager; ///< The associated \ref manager.
	};
}

namespace codepad {
	/// Various types of geometries that can be used in the definition of an element's visuals.
	namespace ui::geometries {
		/// A rectangular geometry.
		struct rectangle {
			/// Default constructor.
			rectangle() = default;
			/// Initializes all fields of this struct.
			rectangle(relative_vec2d tl, relative_vec2d br) : top_left(tl), bottom_right(br) {
			}

			relative_vec2d
				top_left, ///< The top-left corner of this rectangle.
				bottom_right; ///< The bottom-right corner of this rectangle.

			/// Draws this rectangle in the specified region with the specified bursh and pen.
			void draw(
				vec2d unit, renderer_base &r,
				const generic_brush_parameters &brush, const generic_pen_parameters &pen
			) const {
				r.draw_rectangle(rectd::from_corners(
					top_left.get_absolute_offset(unit), bottom_right.get_absolute_offset(unit)
				), brush, pen);
			}
		};
	}
	namespace json {
		/// Parser for \ref ui::geometries::rectangle.
		template <> struct default_parser<ui::geometries::rectangle> {
			/// The parser interface.
			template <typename Value> std::optional<ui::geometries::rectangle> operator()(const Value&) const;
		};
	}

	namespace ui::geometries {
		/// A rectangle geometry with rounded corners.
		struct rounded_rectangle {
			/// Default constructor.
			rounded_rectangle() = default;
			/// Initializes all fields of this struct.
			rounded_rectangle(relative_vec2d tl, relative_vec2d br, relative_double rx, relative_double ry) :
				top_left(tl), bottom_right(br), radiusx(rx), radiusy(ry) {
			}

			relative_vec2d
				top_left, ///< \sa rectangle::top_left
				bottom_right; ///< \sa rectangle::bottom_right
			relative_double
				radiusx, ///< The horizontal radius.
				radiusy; ///< The vertical radius.

			/// Draws this rounded rectangle in the specified region with the specified bursh and pen.
			void draw(
				vec2d unit, renderer_base &r,
				const generic_brush_parameters &brush, const generic_pen_parameters &pen
			) const {
				r.draw_rounded_rectangle(
					rectd::from_corners(top_left.get_absolute_offset(unit), bottom_right.get_absolute_offset(unit)),
					radiusx.get_absolute(unit.x), radiusy.get_absolute(unit.y),
					brush, pen
				);
			}
		};
	}
	namespace json {
		/// Parser for \ref ui::geometries::rounded_rectangle.
		template <> struct default_parser<ui::geometries::rounded_rectangle> {
			/// The parser interface.
			template <typename Value> std::optional<ui::geometries::rounded_rectangle> operator()(
				const Value&
			) const;
		};
	}

	namespace ui::geometries {
		/// An ellipse geometry.
		struct ellipse {
			/// Default constructor.
			ellipse() = default;
			/// Initializes all fields of this struct.
			ellipse(relative_vec2d tl, relative_vec2d br) : top_left(tl), bottom_right(br) {
			}

			relative_vec2d
				top_left, ///< The top-left corner of the bounding box of this ellipse.
				bottom_right; ///< The bottom-right corner of the bounding box of this ellipse.

			/// Draws this ellipse in the specified region with the specified bursh and pen.
			void draw(
				vec2d unit, renderer_base &r,
				const generic_brush_parameters &brush, const generic_pen_parameters &pen
			) const {
				auto rgn = rectd::from_corners(
					top_left.get_absolute_offset(unit), bottom_right.get_absolute_offset(unit)
				);
				r.draw_ellipse(rgn.center(), 0.5 * rgn.width(), 0.5 * rgn.height(), brush, pen);
			}
		};
	}
	namespace json {
		/// Parser for \ref ui::geometries::ellipse.
		template <> struct default_parser<ui::geometries::ellipse> {
			/// The parser interface.
			template <typename Value> std::optional<ui::geometries::ellipse> operator()(const Value&) const;
		};
	}

	namespace ui::geometries {
		/// A path geometry that allows for more complicated geometries.
		struct path {
			// different parts
			/// A part of a \ref subpath that's a line segment.
			struct segment {
				relative_vec2d to; ///< The end point of the segment.

				/// Adds this part to a \ref path_geometry_builder.
				void add_to(path_geometry_builder &builder, vec2d unit) const {
					builder.add_segment(to.get_absolute_offset(unit));
				}
			};
			/// A part of a \ref subpath that's an arc.
			struct arc {
				relative_vec2d to; ///< The end point of this arc.
				relative_vec2d radius; ///< The radius of the corresponding ellipse.
				double rotation = 0.0; ///< The rotation of the corresponding ellipse.
				sweep_direction direction = sweep_direction::clockwise; ///< The sweep direction of this arc.
				arc_type type = arc_type::minor; ///< The type of this arc.

				/// Adds this part to a \ref path_geometry_builder.
				void add_to(path_geometry_builder &builder, vec2d unit) const {
					builder.add_arc(
						to.get_absolute_offset(unit), radius.get_absolute_offset(unit), rotation, direction, type
					);
				}
			};
			/// A part of a \ref subpath that's a cubic bezier.
			struct cubic_bezier {
				relative_vec2d
					to, ///< The end point of the bezier curve.
					control1, ///< The first control point.
					control2; ///< The second control point.

				/// Adds this part to a \ref path_geometry_builder.
				void add_to(path_geometry_builder &builder, vec2d unit) const {
					builder.add_cubic_bezier(
						to.get_absolute_offset(unit), control1.get_absolute_offset(unit), control2.get_absolute_offset(unit)
					);
				}
			};

			/// Contains a part of a \ref subpath.
			struct part {
				/// The type that holds a specific type of part.
				using value_type = std::variant<segment, arc, cubic_bezier>;

				/// Adds this part to a \ref path_geometry_builder.
				void add_to(path_geometry_builder &builder, vec2d unit) const {
					std::visit([&](auto &&obj) {
						obj.add_to(builder, unit);
						}, value);
				}

				value_type value; ///< The value of this part.
			};

			/// A part of a path, independent of all other subpaths.
			struct subpath {
				std::vector<part> parts; ///< Parts of this subpath.
				relative_vec2d starting_point; ///< The starting point of this subpath.
				bool closed = false; ///< Whether this subpath should be closed when stroked.
			};

			std::vector<subpath> subpaths; ///< The list of subpaths.

			/// Draws this path in the specified region with the specified brush and pen.
			void draw(
				vec2d unit, renderer_base &r,
				const generic_brush_parameters &brush, const generic_pen_parameters &pen
			) const {
				path_geometry_builder &builder = r.start_path();
				for (const auto &sp : subpaths) {
					builder.move_to(sp.starting_point.get_absolute_offset(unit));
					for (const auto &p : sp.parts) {
						p.add_to(builder, unit);
					}
					if (sp.closed) {
						builder.close();
					}
				}
				r.end_and_draw_path(brush, pen);
			}
		};
	}
	namespace json {
		/// Parser for \ref ui::geometries::path::part.
		template <> struct default_parser<ui::geometries::path::part> {
			/// The parser interface.
			template <typename Value> std::optional<ui::geometries::path::part> operator()(const Value&) const;
		};

		/// Parser for \ref ui::geometries::path::subpath.
		template <> struct default_parser<ui::geometries::path::subpath> {
			/// The parser interface.
			template <typename Value> std::optional<ui::geometries::path::subpath> operator()(const Value&) const;
		};

		/// Parser for \ref ui::geometries::path.
		template <> struct default_parser<ui::geometries::path> {
			/// The parser interface.
			template <typename Value> std::optional<ui::geometries::path> operator()(const Value&) const;
		};
	}
}

namespace codepad::ui {
	/// Describes a geometry and how it is rendered.
	struct generic_visual_geometry {
	public:
		using value_type = std::variant<
			geometries::rectangle, geometries::rounded_rectangle, geometries::ellipse, geometries::path
		>; ///< The definition of the value of this geometry.

		/// Draws this geometry in the specified region with the specified brush and pen.
		void draw(vec2d unit, renderer_base &r) const {
			r.push_matrix_mult(transform.get_matrix(unit));
			std::visit([&](auto &&obj) {
				obj.draw(unit, r, fill.get_parameters(unit), stroke.get_parameters(unit));
				}, value);
			r.pop_matrix();
		}

		value_type value; ///< The value of this geometry.
		transforms::generic transform; ///< The transform of this geometry.
		generic_brush fill; ///< The brush used to fill the geometry.
		generic_pen stroke; ///< The pen used to stroke the geometry.
	};
	/// Parser for \ref generic_visual_geometry.
	template <> struct managed_json_parser<generic_visual_geometry> {
	public:
		/// Initializes \ref _manager.
		explicit managed_json_parser(manager &m) : _manager(m) {
		}

		/// The parser interface.
		template <typename Value> std::optional<generic_visual_geometry> operator()(const Value&) const;
	protected:
		manager &_manager; ///< The associated \ref manager.
	};


	/// Parameters that determines the visuals of an element.
	struct visuals {
		std::vector<generic_visual_geometry> geometries; ///< The geometries used as the background of the element.
		transforms::generic transform; ///< The transform of the element.

		/// Renders this object as an independent set of geometries in the specified bounds.
		void render(rectd bounds, renderer_base &r) const {
			r.push_matrix_mult(matd3x3::translate(bounds.xmin_ymin()) * transform.get_matrix(bounds.size()));
			for (auto &&geom : geometries) {
				geom.draw(bounds.size(), r);
			}
			r.pop_matrix();
		}
	};
	/// Parser for \ref visuals.
	template <> struct managed_json_parser<visuals> {
	public:
		/// Initializes \ref _manager.
		explicit managed_json_parser(manager &m) : _manager(m) {
		}

		/// The parser interface.
		template <typename Value> std::optional<visuals> operator()(const Value&) const;
	protected:
		manager &_manager; ///< The associated \ref manager.
	};

	/// Parameters that determine the layout of an element.
	struct element_layout {
		thickness
			margin, ///< The element's margin.
			padding; ///< The element's internal padding.
		vec2d size; ///< The element's size.
		anchor elem_anchor = anchor::all; ///< The element's anchor.
		size_allocation_type
			width_alloc = size_allocation_type::automatic, ///< Determines how the element's width is allocated.
			height_alloc = size_allocation_type::automatic; ///< Determines how the element's height is allocated.
	};
}
namespace codepad::json {
	/// Parser for \ref ui::element_layout.
	template <> struct default_parser<ui::element_layout> {
		/// The parser interface.
		template <typename ValueType> std::optional<ui::element_layout> operator()(const ValueType&) const;
	protected:
		/// Parses the `width' or `height' field that specifies the size of an object in one direction.
		template <typename ValueType> static void _parse_size_component(
			const ValueType&, double&, ui::size_allocation_type&
		);
	};
}

namespace codepad::ui {
	/// Basic parameters that are used by all types of elements.
	struct element_parameters {
		visuals visual_parameters; ///< The \ref visuals.
		element_layout layout_parameters; ///< The \ref element_layout.
		visibility element_visibility = visibility::full; ///< The visibility of this element.
		cursor custom_cursor = cursor::not_specified; ///< The custom cursor of the element.
	};

	/// Contains configuration of an element's behavior.
	struct element_configuration {
	public:
		/// Used to uniquely identify an \ref info_event.
		struct event_identifier {
			/// Default constructor.
			event_identifier() = default;
			/// Constructs this struct with only the name of the event.
			explicit event_identifier(std::u8string n) : name(std::move(n)) {
			}
			/// Initializes all fields of this struct.
			event_identifier(std::u8string s, std::u8string n) : subject(std::move(s)), name(std::move(n)) {
			}

			/// Parses an \ref event_identifier from a string.
			static event_identifier parse_from_string(std::u8string_view);

			std::u8string
				/// The subject that owns and invokes this event. This may be empty if the subject is the element
				/// itself.
				subject,
				name; ///< The name of the event.
		};

		/// Stores the parameters used to start animations.
		struct animation_parameters {
			generic_keyframe_animation_definition definition; ///< The definition of the animation.
			animation_path::component_list subject; ///< The animation subject.
		};

		/// Contains information about an event trigger.
		struct event_trigger {
			event_identifier identifier; ///< Identifier of the event.
			std::vector<animation_parameters> animations; ///< The animations to play.
		};

		std::vector<event_trigger> event_triggers; ///< The list of event triggers.
		std::map<std::u8string, json::value_storage> attributes; ///< Attributes of this element.
	};
}
