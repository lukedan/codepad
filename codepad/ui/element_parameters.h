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
}
namespace codepad::json::object_parsers {
	/// Parser for \ref ui::relative_vec2d.
	template <> struct parser<ui::relative_vec2d> {
		/// Parses a \ref ui::relative_vec2d. The node can either be its full representation
		/// (<tt>{"absolute": [x, y], "relative": [x, y]}</tt>), or a list of two vectors with the relative value in
		/// the front (<tt>[[absx, absy], [relx, rely]]</tt>), or a single vector indicating the relative value.
		template <typename Value> bool operator()(const Value &obj, ui::relative_vec2d &v) const {
			if (typename Value::object_t full; json::try_cast(obj, full)) {
				// {"absolute": [x, y], "relative": [x, y]}
				parse_member_result
					absres = try_parse_member(full, u8"absolute", v.absolute),
					relres = try_parse_member(full, u8"relative", v.relative);
				return absres == parse_member_result::success || relres == parse_member_result::success;
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
	};
	/// Parser for \ref ui::relative_double.
	template <> struct parser<ui::relative_double> {
		/// Parses a \ref ui::relative_double. The format is similar to that of \ref relative_vec2d.
		template <typename Value> bool operator()(const Value &obj, ui::relative_double &d) const {
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
	};
}

namespace codepad::ui {
	/// Various types of transforms.
	namespace transforms {
		/// The identity transform.
		struct identity {
			/// Returns the identity matrix.
			matd3x3 get_matrix(vec2d) const {
				matd3x3 res;
				res.set_identity();
				return res;
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
				return matrix.transform(pt);
			}
			/// Inverse transforms the given point.
			vec2d inverse_transform_point(vec2d pt, vec2d) const {
				return matrix.inverse().transform(pt);
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
				return std::visit([&unit](auto && trans) {
					return trans.get_matrix(unit);
					}, value);
			}
			/// Transforms the given point.
			vec2d transform_point(vec2d pt, vec2d unit) const {
				return std::visit([&pt, &unit](auto && trans) {
					return trans.transform_point(pt, unit);
					}, value);
			}
			/// Inverse transforms the given point.
			vec2d inverse_transform_point(vec2d pt, vec2d unit) const {
				return std::visit([&pt, &unit](auto && trans) {
					return trans.inverse_transform_point(pt, unit);
					}, value);
			}

			value_type value; ///< The value of this transformation.
		};


		inline matd3x3 collection::get_matrix(vec2d unit) const {
			matd3x3 res;
			res.set_identity();
			for (const generic &g : components) {
				res = g.get_matrix(unit) * res;
			}
			return res;
		}

		inline vec2d collection::transform_point(vec2d pt, vec2d unit) const {
			for (const generic &g : components) {
				pt = g.transform_point(pt, unit);
			}
			return pt;
		}

		inline vec2d collection::inverse_transform_point(vec2d pt, vec2d unit) const {
			for (auto it = components.rbegin(); it != components.rend(); ++it) {
				pt = it->inverse_transform_point(pt, unit);
			}
			return pt;
		}
	}
}

namespace codepad::json::object_parsers {
	/// Parser for \ref ui::transform::generic.
	template <> struct parser<ui::transforms::generic> {
		/// Parses a generic transform. The value can either be:
		///  - A list, whcih is interpreted as a transform collection.
		///  - An object with a member named either `trasnaltion', `scale', `rotation', or `children'. These members
		///    are checked in order and only the first one is handled.
		template <typename Value> bool operator()(const Value &val, ui::transforms::generic &v) const {
			if (val.is<null_t>()) {
				v.value.emplace<ui::transforms::identity>();
			}
			typename Value::array_t arr;
			bool has_array = false; // marks if arr is valid
			if (typename Value::object_t obj; try_cast(val, obj)) {
				parse_member_result res;
				{ // translation
					ui::relative_vec2d offset;
					res = try_parse_member(obj, u8"translation", offset);
					if (res == parse_member_result::success) {
						v.value.emplace<ui::transforms::translation>(offset);
						return true;
					}
					if (res == parse_member_result::parsing_failed) {
						return false;
					}
				}
				{ // scale
					ui::relative_vec2d center;
					vec2d scale;
					res = try_parse_member(obj, u8"scale", scale);
					if (res == parse_member_result::success) {
						try_parse_member(obj, u8"center", center);
						v.value.emplace<ui::transforms::scale>(center, scale);
						return true;
					}
					if (res == parse_member_result::parsing_failed) {
						return false;
					}
				}
				{ // rotation
					ui::relative_vec2d center;
					double angle;
					res = try_parse_member(obj, u8"rotation", angle);
					if (res == parse_member_result::success) {
						try_parse_member(obj, u8"center", center);
						v.value.emplace<ui::transforms::rotation>(center, angle);
						return true;
					}
					if (res == parse_member_result::parsing_failed) {
						return false;
					}
				}
				if (json::try_cast_member(obj, u8"children", arr)) {
					has_array = true;
				}
			}
			if (!has_array) {
				if (!json::try_cast(val, arr)) {
					return false;
				}
			}
			ui::transforms::collection res;
			for (auto &&trans : arr) {
				if (!(*this)(trans, res.components.emplace_back())) {
					return false;
				}
			}
			v.value.emplace<ui::transforms::collection>(std::move(res));
			return true;
		}
	};
}


namespace codepad::ui {
	/// Various types of brushes.
	namespace brushes {
		/// Corresponds to \ref brush_parameters::solid_color.
		struct solid_color {
			/// Returns the corresponding \ref brush_parameters::solid_color given the target region.
			brush_parameters::solid_color get_parameters(vec2d) const {
				return brush_parameters::solid_color(color);
			}

			colord color; ///< \sa brush_parameters::solid_color::color
		};
		/// Corresponds to \ref brush_parameters::linear_gradient.
		struct linear_gradient {
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
			return std::visit([this, &unit](auto && b) {
				return generic_brush_parameters(b.get_parameters(unit), transform.get_matrix(unit));
				}, value);
		}

		value_type value; ///< The value of this generic brush.
		transforms::generic transform; ///< The transform of this brush.
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


	/// Various types of geometries that can be used in the definition of an element's visuals.
	namespace geometries {
		/// A rectangular geometry.
		struct rectangle {
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
		/// An ellipse geometry.
		struct ellipse {
			relative_vec2d
				top_left, ///< The top-left corner of the bounding box of this ellipse.
				bottom_right; ///< The bottom-right corner of the bounding box of this ellipse.

			/// Draws this ellipse in the specified region with the specified bursh and pen.
			void draw(
				vec2d unit, renderer_base &r,
				const generic_brush_parameters &brush, const generic_pen_parameters &pen
			) const {
				auto rgn = rectd::from_corners(top_left.get_absolute_offset(unit), bottom_right.get_absolute_offset(unit));
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
					std::visit([&](auto && obj) {
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

	/// Describes a geometry and how it is rendered.
	struct generic_visual_geometry {
	public:
		using value_type = std::variant<
			geometries::rectangle, geometries::rounded_rectangle, geometries::ellipse, geometries::path
		>; ///< The definition of the value of this geometry.

		/// Draws this geometry in the specified region with the specified brush and pen.
		void draw(vec2d unit, renderer_base &r) const {
			r.push_matrix_mult(transform.get_matrix(unit));
			std::visit([&](auto && obj) {
				obj.draw(unit, r, fill.get_parameters(unit), stroke.get_parameters(unit));
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
		/// Used to uniquely identify an \ref info_event.
		struct event_identifier {
			/// Default constructor.
			event_identifier() = default;
			/// Constructs this struct with only the name of the event.
			explicit event_identifier(str_t n) : name(std::move(n)) {
			}
			/// Initializes all fields of this struct.
			event_identifier(str_t s, str_t n) : subject(std::move(s)), name(std::move(n)) {
			}

			/// Parses an \ref event_identifier from a string.
			inline static event_identifier parse_from_string(str_view_t s) {
				str_view_t::const_iterator sep1 = s.end(), sep2 = s.end();
				if (auto it = s.begin(); it != s.end()) {
					for (codepoint cp; it != s.end(); ) {
						auto begin = it;
						if (encodings::utf8::next_codepoint(it, s.end(), cp)) {
							if (cp == U'.') { // separator, only consider the first one
								return event_identifier(str_t(s.begin(), begin), str_t(it, s.end()));
							}
						}
					}
				}
				return event_identifier(str_t(s));
			}

			str_t
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

		element_parameters default_parameters; ///< The default parameters for elements of this class.
		std::vector<event_trigger> event_triggers; ///< The list of event triggers.
		/// Additional attributes that are dependent of the element's type.
		std::map<str_t, json::value_storage> additional_attributes;
	};
}
