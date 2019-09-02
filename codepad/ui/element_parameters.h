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
		template <typename Value> std::optional<ui::relative_vec2d> operator()(const Value &val) const {
			if (auto arr = val.template try_cast<typename Value::array_t>()) {
				if (arr->size() >= 2) {
					if (arr->size() > 2) {
						val.template log<log_level::warning>(CP_HERE) <<
						    "redundant members in relative vec2d definition";
					}
					if (auto x = arr->at(0).template try_cast<double>()) { // only absolute component
						if (auto y = arr->at(1).template cast<double>()) {
							return ui::relative_vec2d(vec2d(), vec2d(x.value(), y.value()));
						}
					} else if (auto rel_vec = arr->at(0).template try_parse<vec2d>()) { // array representation
						if (auto abs_vec = arr->at(1).template parse<vec2d>()) {
							return ui::relative_vec2d(rel_vec.value(), abs_vec.value());
						}
					} else {
						val.template log<log_level::error>(CP_HERE) << "invalid relative vec2d component format";
					}
				} else {
					val.template log<log_level::error>(CP_HERE) << "not enough entries in relative vec2d definition";
				}
			} else if (auto full = val.template try_cast<typename Value::object_t>()) { // full representation
				if (auto abs = full->template parse_member<vec2d>(u8"absolute")) {
					if (auto rel = full->template parse_member<vec2d>(u8"relative")) {
						return ui::relative_vec2d(rel.value(), abs.value());
					}
				}
			} else {
				val.template log<log_level::error>(CP_HERE) << "invalid relative vec2d format";
			}
			return std::nullopt;
		}
	};
	/// Parser for \ref ui::relative_double.
	template <> struct default_parser<ui::relative_double> {
		/// Parses a \ref ui::relative_double. The format is similar to that of \ref relative_vec2d.
		template <typename Value> std::optional<ui::relative_double> operator()(const Value &val) const {
			if (auto full = val.template try_cast<typename Value::object_t>()) { // full representation
				if (auto abs = full->template parse_member<double>(u8"absolute")) {
					if (auto rel = full->template parse_member<double>(u8"relative")) {
						if (full->size() > 2) {
							val.template log<log_level::warning>(CP_HERE) << "redundant fields in relative double";
						}
						return ui::relative_double(rel.value(), abs.value());
					}
				}
			} else if (auto arr = val.template try_cast<typename Value::array_t>()) { // a list of two doubles
				if (arr->size() >= 2) {
					if (arr->size() > 2) {
						val.template log<log_level::warning>(CP_HERE) << "redundant elements in relative double";
					}
					auto
						rel = arr->at(0).template cast<double>(),
						abs = arr->at(1).template cast<double>();
					if (rel && abs) {
						return ui::relative_double(rel.value(), abs.value());
					}
				} else {
					val.template log<log_level::error>(CP_HERE) << "too few elements in relative double";
				}
			} else if (auto abs = val.template try_cast<double>()) { // absolute only
				return ui::relative_double(0.0, abs.value());
			} else {
				val.template log<log_level::error>(CP_HERE) << "invalid relative double format";
			}
			return std::nullopt;
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

			/// Creates a new generic trasnform that contains the given specific type of transform.
			template <typename Trans, typename ...Args> inline static generic make(Args &&...args) {
				generic res;
				res.value.emplace<Trans>(std::forward<Args>(args)...);
				return res;
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
namespace codepad::json {
	/// Parser for \ref ui::transform::generic.
	template <> struct default_parser<ui::transforms::generic> {
		/// Parses a generic transform. The value can either be:
		///  - A list, whcih is interpreted as a transform collection.
		///  - An object with a member named either `trasnaltion', `scale', `rotation', or `children'. These members
		///    are checked in order and only the first one is handled.
		template <typename Value> std::optional<ui::transforms::generic> operator()(const Value &val) const {
			if (val.template is<null_t>()) {
				return ui::transforms::generic::make<ui::transforms::identity>();
			}
			std::optional<typename Value::array_t> group;
			if (auto obj = val.template try_cast<typename Value::object_t>()) {
				if (auto offset = obj->template parse_optional_member<ui::relative_vec2d>(u8"translation")) {
					return ui::transforms::generic::make<ui::transforms::translation>(offset.value());
				}
				if (auto scale = obj->template parse_optional_member<vec2d>(u8"scale")) {
					if (auto center = obj->template parse_member<ui::relative_vec2d>(u8"center")) {
						return ui::transforms::generic::make<ui::transforms::scale>(center.value(), scale.value());
					}
				}
				if (auto rotation = obj->template parse_optional_member<double>(u8"rotation")) {
					if (auto center = obj->template parse_member<ui::relative_vec2d>(u8"center")) {
						return ui::transforms::generic::make<ui::transforms::rotation>(
							center.value(), rotation.value()
							);
					}
				}
				group = obj->template parse_optional_member<typename Value::array_t>(u8"children");
			} else { // try to parse transform collection
				group = val.template try_cast<typename Value::array_t>();
			}
			if (group) {
				ui::transforms::collection res;
				for (auto &&trans : group.value()) {
					if (auto child = trans.template parse<ui::transforms::generic>()) {
						res.components.emplace_back(std::move(child.value()));
					}
				}
				return ui::transforms::generic::make<ui::transforms::collection>(std::move(res));
			} else {
				val.template log<log_level::error>(CP_HERE) << "invalid transform format";
			}
			return std::nullopt;
		}
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
		template <typename Value> std::optional<ui::brushes::solid_color> operator()(const Value &val) const {
			if (auto obj = val.template cast<typename Value::object_t>()) {
				if (auto color = obj->template parse_member<colord>(u8"color")) {
					return ui::brushes::solid_color(color.value());
				}
			}
			return std::nullopt;
		}
	};
	/// Parser for \ref ui::brushes::linear_gradient.
	template <> struct default_parser<ui::brushes::linear_gradient> {
		/// The parser interface.
		template <typename Value> std::optional<ui::brushes::linear_gradient> operator()(const Value &val) const {
			if (auto obj = val.template cast<typename Value::object_t>()) {
				if (auto from = obj->template parse_member<ui::relative_vec2d>(u8"from")) {
					if (auto to = obj->template parse_member<ui::relative_vec2d>(u8"to")) {
						if (auto stops = obj->template parse_member<ui::gradient_stop_collection>(
							u8"gradient_stops", array_parser<ui::gradient_stop>()
							)) {
							return ui::brushes::linear_gradient(from.value(), to.value(), std::move(stops.value()));
						}
					}
				}
			}
			return std::nullopt;
		}
	};
	/// Parser for \ref ui::brushes::radial_gradient.
	template <> struct default_parser<ui::brushes::radial_gradient> {
		/// The parser interface.
		template <typename Value> std::optional<ui::brushes::radial_gradient> operator()(const Value &val) const {
			if (auto obj = val.template cast<typename Value::object_t>()) {
				if (auto center = obj->template parse_member<ui::relative_vec2d>(u8"center")) {
					if (auto radius = obj->template parse_member<double>(u8"radius")) {
						if (auto stops = obj->template parse_member<ui::gradient_stop_collection>(
							u8"gradient_stops", array_parser<ui::gradient_stop>()
							)) {
							return ui::brushes::radial_gradient(
								center.value(), radius.value(), std::move(stops.value())
								);
						}
					}
				}
			}
			return std::nullopt;
		}
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
			return std::visit([this, &unit](auto && b) {
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
		template <typename Value> std::optional<generic_brush> operator()(const Value &val) const {
			if (auto obj = val.template try_cast<typename Value::object_t>()) {
				generic_brush result;
				if (auto type = obj->template parse_member<str_view_t>(u8"type")) {
					if (type.value() == u8"solid") {
						if (auto brush = val.template parse<brushes::solid_color>()) {
							result.value.emplace<brushes::solid_color>(std::move(brush.value()));
						}
					} else if (type.value() == u8"linear_gradient") {
						if (auto brush = val.template parse<brushes::linear_gradient>()) {
							result.value.emplace<brushes::linear_gradient>(std::move(brush.value()));
						}
					} else if (type.value() == u8"radial_gradient") {
						if (auto brush = val.template parse<brushes::radial_gradient>()) {
							result.value.emplace<brushes::radial_gradient>(std::move(brush.value()));
						}
					} else if (type.value() == u8"radial_gradient") {
						if (auto brush = val.template parse<brushes::radial_gradient>()) {
							result.value.emplace<brushes::radial_gradient>(std::move(brush.value()));
						}
					} else if (type.value() == u8"bitmap") {
						if (auto brush = val.template parse<brushes::bitmap_pattern>(
							managed_json_parser<brushes::bitmap_pattern>(_manager)
							)) {
							result.value.emplace<brushes::bitmap_pattern>(std::move(brush.value()));
						}
					} else if (type.value() != u8"none") {
						val.template log<log_level::error>(CP_HERE) << "invalid brush type";
						return std::nullopt;
					}
				}
				if (auto trans = obj->template parse_optional_member<transforms::generic>(u8"transform")) {
					result.transform = std::move(trans.value());
				}
				return result;
			} else if (auto color = val.template parse<colord>()) {
				generic_brush result;
				result.value.emplace<brushes::solid_color>(color.value());
				return result;
			}
			return std::nullopt;
		}
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
		template <typename Value> std::optional<generic_pen> operator()(const Value &val) const {
			if (auto brush = val.template parse<generic_brush>(managed_json_parser<generic_brush>(_manager))) {
				generic_pen result;
				result.brush = brush.value();
				if (auto obj = val.template cast<typename Value::object_t>()) {
					result.thickness =
						obj->template parse_optional_member<double>(u8"thickness").value_or(result.thickness);
				}
				return result;
			}
			return std::nullopt;
		}
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
			template <typename Value> std::optional<ui::geometries::rectangle> operator()(const Value &val) const {
				if (auto obj = val.template cast<typename Value::object_t>()) {
					if (auto top_left = obj->template parse_member<ui::relative_vec2d>(u8"top_left")) {
						if (auto bottom_right = obj->template parse_member<ui::relative_vec2d>(u8"bottom_right")) {
							return ui::geometries::rectangle(top_left.value(), bottom_right.value());
						}
					}
				}
				return std::nullopt;
			}
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
				const Value &val
				) const {
				if (auto obj = val.template cast<typename Value::object_t>()) {
					if (auto top_left = obj->template parse_member<ui::relative_vec2d>(u8"top_left")) {
						if (auto bottom_right = obj->template parse_member<ui::relative_vec2d>(u8"bottom_right")) {
							if (auto rx = obj->template parse_member<ui::relative_double>(u8"radiusx")) {
								if (auto ry = obj->template parse_member<ui::relative_double>(u8"radiusy")) {
									return ui::geometries::rounded_rectangle(
										top_left.value(), bottom_right.value(), rx.value(), ry.value()
									);
								}
							}
						}
					}
				}
				return std::nullopt;
			}
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
			template <typename Value> std::optional<ui::geometries::ellipse> operator()(const Value &val) const {
				if (auto obj = val.template cast<typename Value::object_t>()) {
					if (auto top_left = obj->template parse_member<ui::relative_vec2d>(u8"top_left")) {
						if (auto bottom_right = obj->template parse_member<ui::relative_vec2d>(u8"bottom_right")) {
							return ui::geometries::ellipse(top_left.value(), bottom_right.value());
						}
					}
				}
				return std::nullopt;
			}
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
	namespace json {
		/// Parser for \ref ui::geometries::path::part.
		template <> struct default_parser<ui::geometries::path::part> {
			/// The parser interface.
			template <typename Value> std::optional<ui::geometries::path::part> operator()(const Value &val) const {
				if (auto obj = val.template cast<typename Value::object_t>()) {
					if (obj->size() > 0) {
						if (obj->size() > 1) {
							val.template log<log_level::warning>(CP_HERE) << "too many fields in subpath part";
						}
						auto member = obj->member_begin();
						if (member.name() == u8"line_to") {
							if (auto to = member.value().template parse<ui::relative_vec2d>()) {
								ui::geometries::path::part res;
								res.value.emplace<ui::geometries::path::segment>().to = to.value();
								return res;
							}
						} else if (member.name() == u8"arc") {
							if (auto part_obj = member.value().template cast<typename Value::object_t>()) {
								if (auto to = part_obj->template parse_member<ui::relative_vec2d>(u8"to")) {
									if (auto it = part_obj->find_member(u8"radius"); it != part_obj->member_end()) {
										ui::geometries::path::part res;
										auto &arc = res.value.emplace<ui::geometries::path::arc>();
										if (auto r = it.value().template try_cast<double>()) {
											arc.radius.absolute = vec2d(r.value(), r.value());
										} else if (auto rval = it.value().template parse<ui::relative_vec2d>()) {
											arc.radius = rval.value();
										} else {
											it.value().template log<log_level::error>(CP_HERE) <<
												"invalid radius format";
											return std::nullopt;
										}
										arc.direction =
											part_obj->template parse_member<bool>(u8"clockwise").value_or(false) ?
											ui::sweep_direction::clockwise : ui::sweep_direction::counter_clockwise;
										arc.type =
											part_obj->template parse_member<bool>(u8"major").value_or(false) ?
											ui::arc_type::major : ui::arc_type::minor;
										if (
											auto rot = part_obj->template parse_optional_member<double>(u8"rotation")
										) {
											arc.rotation = rot.value();
										}
										return res;
									}
								}
							}
						} else if (member.name() == u8"bezier") {
							if (auto part_obj = member.value().template cast<typename Value::object_t>()) {
								if (auto to = part_obj->template parse_member<ui::relative_vec2d>(u8"to")) {
									if (auto c1 = part_obj->template parse_member<ui::relative_vec2d>(
										u8"control1"
									)) {
										if (auto c2 = part_obj->template parse_member<ui::relative_vec2d>(
											u8"control2"
										)) {
											ui::geometries::path::part res;
											auto &bezier = res.value.emplace<ui::geometries::path::cubic_bezier>();
											bezier.to = to.value();
											bezier.control1 = c1.value();
											bezier.control2 = c2.value();
											return res;
										}
									}
								}
							}
						} else {
							val.template log<log_level::error>(CP_HERE) << "invalid subpath part type";
						}
					} else {
						val.template log<log_level::error>(CP_HERE) << "empty subpath part";
					}
				}
				return std::nullopt;
			}
		};

		/// Parser for \ref ui::geometries::path::subpath.
		template <> struct default_parser<ui::geometries::path::subpath> {
			/// The parser interface.
			template <typename Value> std::optional<ui::geometries::path::subpath> operator()(
				const Value &val
				) const {
				if (auto obj = val.template try_cast<typename Value::object_t>()) {
					if (auto start = obj->template parse_member<ui::relative_vec2d>(u8"start")) {
						if (auto parts = obj->template parse_member<std::vector<ui::geometries::path::part>>(
							u8"parts", array_parser<ui::geometries::path::part>()
							)) {
							if (auto closed = obj->template parse_member<bool>(u8"closed")) {
								ui::geometries::path::subpath res;
								res.starting_point = start.value();
								res.parts = std::move(parts.value());
								res.closed = closed.value();
								return res;
							}
						}
					}
				} else if (auto arr = val.template try_cast<typename Value::array_t>()) {
					if (arr->size() > 2) {
						auto beg = arr->begin(), end = arr->end() - 1;
						if (auto start = beg->template parse<ui::relative_vec2d>()) {
							if (auto final_obj = end->template cast<typename Value::object_t>()) {
								if (auto closed = final_obj->template parse_member<bool>(u8"closed")) {
									ui::geometries::path::subpath res;
									res.starting_point = start.value();
									res.closed = closed.value();
									for (++beg; beg != end; ++beg) {
										if (auto part = beg->template parse<ui::geometries::path::part>()) {
											res.parts.emplace_back(part.value());
										}
									}
									return res;
								}
							}
						}
					} else {
						val.template log<log_level::error>(CP_HERE) << "too few elements in subpath";
					}
				} else {
					val.template log<log_level::error>(CP_HERE) << "invalid subpath format";
				}
				return std::nullopt;
			}
		};

		/// Parser for \ref ui::geometries::path.
		template <> struct default_parser<ui::geometries::path> {
			/// The parser interface.
			template <typename Value> std::optional<ui::geometries::path> operator()(const Value &val) const {
				if (auto obj = val.template cast<typename Value::object_t>()) {
					if (auto subpaths = obj->template parse_member<std::vector<ui::geometries::path::subpath>>(
						u8"subpaths", array_parser<ui::geometries::path::subpath>()
						)) {
						ui::geometries::path result;
						result.subpaths = std::move(subpaths.value());
						return result;
					}
				}
				return std::nullopt;
			}
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
	/// Parser for \ref generic_visual_geometry.
	template <> struct managed_json_parser<generic_visual_geometry> {
	public:
		/// Initializes \ref _manager.
		explicit managed_json_parser(manager &m) : _manager(m) {
		}

		/// The parser interface.
		template <typename Value> std::optional<generic_visual_geometry> operator()(const Value &val) const {
			if (auto obj = val.template cast<typename Value::object_t>()) {
				if (auto type = obj->template parse_member<str_view_t>(u8"type")) {
					generic_visual_geometry result;
					if (type.value() == u8"rectangle") {
						if (auto rect = val.template parse<geometries::rectangle>()) {
							result.value.emplace<geometries::rectangle>(rect.value());
						} else {
							return std::nullopt;
						}
					} else if (type.value() == u8"rounded_rectangle") {
						if (auto rrect = val.template parse<geometries::rounded_rectangle>()) {
							result.value.emplace<geometries::rounded_rectangle>(rrect.value());
						} else {
							return std::nullopt;
						}
					} else if (type.value() == u8"ellipse") {
						if (auto ellip = val.template parse<geometries::ellipse>()) {
							result.value.emplace<geometries::ellipse>(ellip.value());
						} else {
							return std::nullopt;
						}
					} else if (type.value() == u8"path") {
						if (auto path = val.template parse<geometries::path>()) {
							result.value.emplace<geometries::path>(std::move(path.value()));
						} else {
							return std::nullopt;
						}
					} else {
						val.template log<log_level::error>(CP_HERE) << "invalid geometry type";
						return std::nullopt;
					}
					if (auto trans = obj->template parse_optional_member<transforms::generic>(u8"transform")) {
						result.transform = std::move(trans.value());
					}
					if (auto fill = obj->template parse_optional_member<generic_brush>(
						u8"fill", managed_json_parser<generic_brush>(_manager)
						)) {
						result.fill = fill.value();
					}
					if (auto stroke = obj->template parse_optional_member<generic_pen>(
						u8"stroke", managed_json_parser<generic_pen>(_manager)
						)) {
						result.stroke = stroke.value();
					}
					return result;
				}
			}
			return std::nullopt;
		}
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
		template <typename Value> std::optional<visuals> operator()(const Value &val) const {
			auto geom_parser = json::array_parser<
				generic_visual_geometry, managed_json_parser<generic_visual_geometry>
			>(managed_json_parser<generic_visual_geometry>(_manager));
			if (auto obj = val.template try_cast<typename Value::object_t>()) {
				if (auto geoms = obj->template parse_member<std::vector<generic_visual_geometry>>(
					u8"geometries", geom_parser
					)) {
					visuals res;
					res.geometries = std::move(geoms.value());
					if (auto trans = obj->template parse_optional_member<transforms::generic>(u8"transform")) {
						res.transform = std::move(trans.value());
					}
					return res;
				}
			} else if (val.template is<typename Value::array_t>()) {
				if (auto geoms = val.template parse<std::vector<generic_visual_geometry>>(geom_parser)) {
					visuals res;
					res.geometries = std::move(geoms.value());
					return res;
				}
			} else {
				val.template log<log_level::error>(CP_HERE) << "invalid visuals format";
			}
			return std::nullopt;
		}
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
	/// Parser for \ref element_layout.
	template <> struct default_parser<ui::element_layout> {
		/// The parser interface.
		template <typename ValueType> std::optional<ui::element_layout> operator()(const ValueType &val) const {
			if (auto obj = val.template cast<typename ValueType::object_t>()) {
				ui::element_layout result;
				result.margin =
					obj->template parse_optional_member<ui::thickness>(u8"margin").value_or(result.margin);
				result.padding =
					obj->template parse_optional_member<ui::thickness>(u8"padding").value_or(result.padding);
				result.elem_anchor =
					obj->template parse_optional_member<ui::anchor>(u8"anchor").value_or(result.elem_anchor);
				// parse size
				result.width_alloc = obj->template parse_optional_member<ui::size_allocation_type>(
					u8"width_alloc"
					).value_or(result.width_alloc);
				result.height_alloc = obj->template parse_optional_member<ui::size_allocation_type>(
					u8"height_alloc"
					).value_or(result.height_alloc);
				auto w = obj->find_member(u8"width"), h = obj->find_member(u8"height");
				if (w != obj->member_end() || h != obj->member_end()) { // parse both components
					if (w != obj->member_end()) {
						_parse_size_component(w.value(), result.size.x, result.width_alloc);
					}
					if (h != obj->member_end()) {
						_parse_size_component(h.value(), result.size.y, result.height_alloc);
					}
				} else { // parse a single size
					result.size = obj->template parse_optional_member<vec2d>(u8"size").value_or(result.size);
				}
				return result;
			}
			return std::nullopt;
		}
	protected:
		/// Parses the `width' or `height' field that specifies the size of an object in one direction.
		template <typename ValueType> inline static void _parse_size_component(
			const ValueType &val, double &v, ui::size_allocation_type &ty
		) {
			if (auto str = val.template try_cast<str_view_t>()) {
				if (str.value() == u8"auto" || str.value() == u8"Auto") {
					v = 0.0;
					ty = ui::size_allocation_type::automatic;
					return;
				}
			}
			if (auto alloc = val.template parse<ui::size_allocation>()) {
				v = alloc->value;
				ty = alloc->is_pixels ? ui::size_allocation_type::fixed : ui::size_allocation_type::proportion;
				return;
			}
			val.template log<log_level::error>(CP_HERE) << "failed to parse size component";
		}
	};
}

namespace codepad::ui {
	/// Basic parameters that are used by all types of elements.
	struct element_parameters {
		visuals visual_parameters; ///< The \ref visuals.
		element_layout layout_parameters; ///< The \ref element_layout.
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
