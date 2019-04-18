// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "animation_path.h"

/// \file
/// Implementation of certain methods used when controlling the animations of elements.
///
/// \todo Use reflection.

#include "element.h"
#include "manager.h"

using namespace std;

namespace codepad::ui::animation_path::builder {
	/// Returns \p true if \ref component::type is empty or equals the given type.
	inline bool is_type(const component &comp, str_view_t target) {
		return comp.type.empty() || comp.type == target;
	}

	/// Checks that \ref component::type is either empty or the specified type.
	inline void check_type(const component &comp, str_view_t target) {
		if (!is_type(comp, target)) {
			logger::get().log_warning(CP_HERE, "invalid target type: ", target);
		}
	}
	/// Checks that this \ref component is the last one.
	inline void check_finished(component_list::const_iterator begin, component_list::const_iterator end) {
		if (++begin != end) {
			logger::get().log_warning(CP_HERE, "redundant properties");
		}
	}
	/// Checks that the component does not have an index.
	inline void check_no_index(const component &comp) {
		if (comp.index.has_value()) {
			logger::get().log_warning(CP_HERE, "unexpected index");
		}
	}

#define CP_APB_CONCAT(X, Y) X Y // workaround for MSVC
#define CP_APB_EXPAND_CALL(F, ...) CP_APB_CONCAT(F, (__VA_ARGS__))


#define CP_APB_MUST_TERMINATE check_finished(begin, end)

#define CP_APB_NO_INDEX check_no_index(*begin)

#define CP_APB_DO_CHECK_TYPE(TYPE, TYPENAME) check_type(*begin, u8 ## #TYPENAME)
#define CP_APB_CHECK_TYPE CP_APB_EXPAND_CALL(CP_APB_DO_CHECK_TYPE, CP_APB_CURRENT_TYPE)


#define CP_APB_MAY_TERMINATE_EARLY               \
	if (begin == end) {                          \
		return make_subject_creator<Type>(comp); \
	}

#define CP_APB_MUST_NOT_TERMINATE_EARLY                                            \
	if (begin == end) {                                                            \
		logger::get().log_warning(CP_HERE, "animation path terminated too early"); \
		return nullptr;                                                            \
	}


#define CP_APB_GETTER_NAME(TYPENAME) get_ ## TYPENAME ## _property
#define CP_APB_DO_DECLARE_GETTER(TYPE, TYPENAME)                                                   \
	template <                                                                                     \
		type Type, typename Comp                                                                   \
	> inline unique_ptr<subject_creator<typename Comp::input_type>> CP_APB_GETTER_NAME(TYPENAME) ( \
		component_list::const_iterator begin, component_list::const_iterator end, Comp comp        \
	)
#define CP_APB_DECLARE_GETTER CP_APB_EXPAND_CALL(CP_APB_DO_DECLARE_GETTER, CP_APB_CURRENT_TYPE)
#define CP_APB_START_GETTER CP_APB_DECLARE_GETTER {

#define CP_APB_END_GETTER                                       \
		logger::get().log_warning(CP_HERE, "invalid property"); \
		return nullptr;                                         \
	}


#define CP_APB_DO_TRY_FORWARD_MEMBER(TYPE, TYPENAME, PROP_NAME, PROP_TYPENAME)                \
	if (begin->property == u8 ## #PROP_NAME) {                                                \
		CP_APB_NO_INDEX;                                                                      \
		return CP_APB_GETTER_NAME(PROP_TYPENAME)<Type>(++begin, end, getter_components::pair( \
			comp, getter_components::member_component<&TYPE::PROP_NAME>()                     \
		));                                                                                   \
	}
#define CP_APB_TRY_FORWARD_MEMBER(PROP_NAME, PROP_TYPENAME) \
	CP_APB_EXPAND_CALL(CP_APB_DO_TRY_FORWARD_MEMBER, CP_APB_CURRENT_TYPE, PROP_NAME, PROP_TYPENAME)

#define CP_APB_DO_TRY_FORWARD_VARIANT(TYPE, TYPENAME, VARIANT_TYPE, PROP_NAME, TARGET_TYPE, TARGET_TYPENAME) \
	if (begin->type == u8 ## #TARGET_TYPENAME) {                                                             \
		CP_APB_NO_INDEX;                                                                                     \
		return CP_APB_GETTER_NAME(TARGET_TYPENAME)<Type>(begin, end, getter_components::pair(                \
			getter_components::pair(comp, getter_components::member_component<&TYPE::PROP_NAME>()),          \
			getter_components::variant_component<TYPE::VARIANT_TYPE, TARGET_TYPE>()                          \
		));                                                                                                  \
	}
#define CP_APB_TRY_FORWARD_VARIANT(TARGET_TYPE, TARGET_TYPENAME)                         \
	CP_APB_EXPAND_CALL(                                                                  \
		CP_APB_DO_TRY_FORWARD_VARIANT, CP_APB_CURRENT_TYPE, CP_APB_CURRENT_VARIANT_INFO, \
		TARGET_TYPE, TARGET_TYPENAME                                                     \
	)

#define CP_APB_DO_TRY_FORWARD_ARRAY(TYPE, TYPENAME, PROP_NAME, TARGET_TYPE, TARGET_TYPENAME)        \
	if (begin->property == u8 ## #PROP_NAME && begin->index.has_value()) {                          \
		return CP_APB_GETTER_NAME(TARGET_TYPENAME)<Type>(++begin, end, getter_components::pair(     \
			getter_components::pair(comp, getter_components::member_component<&TYPE::PROP_NAME>()), \
			getter_components::array_component<TARGET_TYPE>(begin->index.value())                   \
		));                                                                                         \
	}
#define CP_APB_TRY_FORWARD_ARRAY(PROP_NAME, TARGET_TYPE, TARGET_TYPENAME) \
	CP_APB_EXPAND_CALL(CP_APB_DO_TRY_FORWARD_ARRAY, CP_APB_CURRENT_TYPE, PROP_NAME, TARGET_TYPE, TARGET_TYPENAME)


	// primitive types
#define CP_APB_CURRENT_TYPE double, double
	CP_APB_START_GETTER
		CP_APB_MAY_TERMINATE_EARLY;
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE shared_ptr<bitmap>, bitmap
	CP_APB_START_GETTER
		CP_APB_MAY_TERMINATE_EARLY;
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE thickness, thickness
	CP_APB_START_GETTER
		CP_APB_MAY_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_MEMBER(left, double);
		CP_APB_TRY_FORWARD_MEMBER(top, double);
		CP_APB_TRY_FORWARD_MEMBER(right, double);
		CP_APB_TRY_FORWARD_MEMBER(bottom, double);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE vec2d, vec2d
	CP_APB_START_GETTER
		CP_APB_MAY_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_MEMBER(x, double);
		CP_APB_TRY_FORWARD_MEMBER(y, double);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE colord, color
	CP_APB_START_GETTER
		CP_APB_MAY_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_MEMBER(r, double);
		CP_APB_TRY_FORWARD_MEMBER(g, double);
		CP_APB_TRY_FORWARD_MEMBER(b, double);
		CP_APB_TRY_FORWARD_MEMBER(a, double);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE relative_double, rel_double
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_MEMBER(relative, double);
		CP_APB_TRY_FORWARD_MEMBER(absolute, double);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE relative_vec2d, rel_vec2d
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_MEMBER(relative, vec2d);
		CP_APB_TRY_FORWARD_MEMBER(absolute, vec2d);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

	// transforms
#define CP_APB_CURRENT_TYPE transforms::generic, transform
	template <
		type, size_t = 0, typename Comp
	> inline unique_ptr<subject_creator<typename Comp::input_type>> CP_APB_GETTER_NAME(transform) (
		component_list::const_iterator, component_list::const_iterator, Comp
	);
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE transforms::translation, translation_transform
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_MEMBER(offset, rel_vec2d);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE transforms::scale, scale_transform
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_MEMBER(center, rel_vec2d);
		CP_APB_TRY_FORWARD_MEMBER(scale_factor, vec2d);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE transforms::rotation, rotation_transform
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_MEMBER(center, rel_vec2d);
		CP_APB_TRY_FORWARD_MEMBER(angle, double);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE transforms::collection, transform_collection
	template <
		type Type, size_t Count, typename Comp
	> inline unique_ptr<subject_creator<typename Comp::input_type>> CP_APB_GETTER_NAME(transform_collection) (
		component_list::const_iterator begin, component_list::const_iterator end, [[maybe_unused]] Comp comp
	) {
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		if constexpr (Count < 2) {
			if (begin->property == u8"children" && begin->index.has_value()) {
				return CP_APB_GETTER_NAME(transform)<Type, Count + 1>(++begin, end, getter_components::pair(
					getter_components::pair(
						comp, getter_components::member_component<&transforms::collection::components>()
					), getter_components::array_component<transforms::generic>(begin->index.value())
				));
			}
		}
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE transforms::generic, transform
#define CP_APB_CURRENT_VARIANT_INFO value_type, value
	template <
		type Type, size_t Count = 0, typename Comp
	> inline unique_ptr<subject_creator<typename Comp::input_type>> CP_APB_GETTER_NAME(transform) (
		component_list::const_iterator begin, component_list::const_iterator end, Comp comp
	) {
		CP_APB_MUST_NOT_TERMINATE_EARLY;

		if (is_type(*begin, u8"transform") && begin->property == u8"value") {
			++begin;
			CP_APB_MUST_NOT_TERMINATE_EARLY;
		}

		CP_APB_TRY_FORWARD_VARIANT(transforms::translation, translation_transform);
		CP_APB_TRY_FORWARD_VARIANT(transforms::scale, scale_transform);
		CP_APB_TRY_FORWARD_VARIANT(transforms::rotation, rotation_transform);
		if (begin->type == u8"transform_collection") {
			CP_APB_NO_INDEX;
			return CP_APB_GETTER_NAME(transform_collection)<Type, Count>(begin, end, getter_components::pair(
				getter_components::pair(comp, getter_components::member_component<&transforms::generic::value>()),
				getter_components::variant_component<transforms::generic::value_type, transforms::collection>()
			));
		}
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

	// brushes
#define CP_APB_CURRENT_TYPE gradient_stop, gradient_stop
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_MEMBER(color, color);
		CP_APB_TRY_FORWARD_MEMBER(position, double);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE brushes::solid_color, solid_color_brush
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_MEMBER(color, color);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE brushes::linear_gradient, linear_gradient_brush
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_ARRAY(gradient_stops, gradient_stop, gradient_stop);
		CP_APB_TRY_FORWARD_MEMBER(from, rel_vec2d);
		CP_APB_TRY_FORWARD_MEMBER(to, rel_vec2d);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE brushes::radial_gradient, radial_gradient_brush
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_ARRAY(gradient_stops, gradient_stop, gradient_stop);
		CP_APB_TRY_FORWARD_MEMBER(center, rel_vec2d);
		CP_APB_TRY_FORWARD_MEMBER(radius, double);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE brushes::bitmap_pattern, bitmap_brush
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_MEMBER(image, bitmap);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE generic_brush, brush
#define CP_APB_CURRENT_VARIANT_INFO value_type, value
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;

		if (is_type(*begin, u8"brush")) {
			CP_APB_TRY_FORWARD_MEMBER(transform, transform);
			if (begin->property == u8"value") {
				++begin;
				CP_APB_MUST_NOT_TERMINATE_EARLY;
			}
		}

		CP_APB_TRY_FORWARD_VARIANT(brushes::solid_color, solid_color_brush);
		CP_APB_TRY_FORWARD_VARIANT(brushes::linear_gradient, linear_gradient_brush);
		CP_APB_TRY_FORWARD_VARIANT(brushes::radial_gradient, radial_gradient_brush);
		CP_APB_TRY_FORWARD_VARIANT(brushes::bitmap_pattern, bitmap_brush);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE generic_pen, pen
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;

		if (is_type(*begin, u8"pen")) {
			CP_APB_TRY_FORWARD_MEMBER(thickness, double);
			if (begin->property == u8"brush") {
				++begin;
				CP_APB_MUST_NOT_TERMINATE_EARLY;
			}
		}

		return CP_APB_GETTER_NAME(brush)<Type>(begin, end, getter_components::pair(
			comp, getter_components::member_component<&generic_pen::brush>()
		));
	}
#undef CP_APB_CURRENT_TYPE

	// geometries
#define CP_APB_CURRENT_TYPE geometries::rectangle, rectangle
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_MEMBER(top_left, rel_vec2d);
		CP_APB_TRY_FORWARD_MEMBER(bottom_right, rel_vec2d);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE geometries::rounded_rectangle, rounded_rectangle
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_MEMBER(top_left, rel_vec2d);
		CP_APB_TRY_FORWARD_MEMBER(bottom_right, rel_vec2d);
		CP_APB_TRY_FORWARD_MEMBER(radiusx, rel_double);
		CP_APB_TRY_FORWARD_MEMBER(radiusy, rel_double);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE geometries::ellipse, ellipse
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_MEMBER(top_left, rel_vec2d);
		CP_APB_TRY_FORWARD_MEMBER(bottom_right, rel_vec2d);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

	// path
#define CP_APB_CURRENT_TYPE geometries::path::segment, segment
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_MEMBER(to, rel_vec2d);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE geometries::path::arc, arc
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_MEMBER(to, rel_vec2d);
		CP_APB_TRY_FORWARD_MEMBER(radius, double);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE geometries::path::cubic_bezier, bezier
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_MEMBER(to, rel_vec2d);
		CP_APB_TRY_FORWARD_MEMBER(control1, rel_vec2d);
		CP_APB_TRY_FORWARD_MEMBER(control2, rel_vec2d);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE geometries::path::part, subpath_part
#define CP_APB_CURRENT_VARIANT_INFO value_type, value
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;

		if (is_type(*begin, u8"subpath_part") && begin->property == u8"value") {
			++begin;
			CP_APB_MUST_NOT_TERMINATE_EARLY;
		}

		CP_APB_TRY_FORWARD_VARIANT(geometries::path::segment, segment);
		CP_APB_TRY_FORWARD_VARIANT(geometries::path::arc, arc);
		CP_APB_TRY_FORWARD_VARIANT(geometries::path::cubic_bezier, bezier);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE geometries::path::subpath, subpath
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_MEMBER(starting_point, rel_vec2d);
		CP_APB_TRY_FORWARD_ARRAY(parts, geometries::path::part, subpath_part);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE geometries::path, path
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_ARRAY(subpaths, geometries::path::subpath, subpath);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE generic_visual_geometry, geometry
#define CP_APB_CURRENT_VARIANT_INFO value_type, value
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;

		if (is_type(*begin, u8"geometry")) {
			CP_APB_TRY_FORWARD_MEMBER(transform, transform);
			CP_APB_TRY_FORWARD_MEMBER(fill, brush);
			CP_APB_TRY_FORWARD_MEMBER(stroke, pen);
			if (begin->property == u8"value") {
				++begin;
				CP_APB_MUST_NOT_TERMINATE_EARLY;
			}
		}

		CP_APB_TRY_FORWARD_VARIANT(geometries::rectangle, rectangle);
		CP_APB_TRY_FORWARD_VARIANT(geometries::rounded_rectangle, rounded_rectangle);
		CP_APB_TRY_FORWARD_VARIANT(geometries::ellipse, ellipse);
		CP_APB_TRY_FORWARD_VARIANT(geometries::path, path);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE


#define CP_APB_CURRENT_TYPE element_visual_parameters, element_visual_parameters
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_ARRAY(geometries, generic_visual_geometry, geometry);
		CP_APB_TRY_FORWARD_MEMBER(transform, transform);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE element_layout_parameters, element_layout_parameters
	CP_APB_START_GETTER
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		CP_APB_CHECK_TYPE;

		CP_APB_TRY_FORWARD_MEMBER(margin, thickness);
		CP_APB_TRY_FORWARD_MEMBER(padding, thickness);
		CP_APB_TRY_FORWARD_MEMBER(size, vec2d);
	CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

	namespace getter_components {
		/// Used to obtain the \ref element_parameters of an element.
		struct element_parameters_getter_component {
			using input_type = element; ///< The input type.
			using output_type = element_parameters; ///< The output type.

			/// Returns the member.
			inline static output_type *get(input_type *input) {
				if (input) {
					return &input->_params;
				}
				return nullptr;
			}

			/// Two instances are always equal.
			friend bool operator==(
				const element_parameters_getter_component&,
				const element_parameters_getter_component&
				) {
				return true;
			}
		};
	}

	unique_ptr<subject_creator<element>> get_common_element_property(
		component_list::const_iterator begin, component_list::const_iterator end
	) {
		CP_APB_MUST_NOT_TERMINATE_EARLY;
		check_type(*begin, "element");

		if (begin->property == "visuals") {
			return get_element_visual_parameters_property<type::visual_only>(
				begin, end, getter_components::pair(
					getter_components::element_parameters_getter_component(),
					getter_components::member_component<&element_parameters::visual_parameters>()
				));
		}
		if (begin->property == "layout") {
			return get_element_layout_parameters_property<type::affects_layout>(
				begin, end, getter_components::pair(
					getter_components::element_parameters_getter_component(),
					getter_components::member_component<&element_parameters::layout_parameters>()
				));
		}
		return nullptr;
	}
}
