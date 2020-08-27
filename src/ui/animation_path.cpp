// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/animation_path.h"

/// \file
/// Implementation of certain methods used when controlling the animations of elements.
///
/// \todo Use reflection.

#include "codepad/ui/element.h"
#include "codepad/ui/manager.h"
#include "codepad/ui/renderer.h"
#include "codepad/ui/json_parsers.h"

namespace codepad::ui::animation_path {
	std::u8string to_string(component_list::const_iterator begin, component_list::const_iterator end) {
		std::stringstream ss;
		bool first = true;
		for (auto it = begin; it != end; ++it) {
			if (first) {
				first = false;
			} else {
				ss << ".";
			}
			if (it->type.empty()) {
				ss << reinterpret_cast<const char*>(it->property.c_str());
			} else {
				ss << "(" << reinterpret_cast<const char*>(it->type.c_str()) << "." << reinterpret_cast<const char*>(it->property.c_str()) << ")";
			}
			if (it->index) {
				ss << "[" << it->index.value() << "]";
			}
		}
		return std::u8string(reinterpret_cast<const char8_t*>(ss.str().c_str()));
	}

	namespace builder {
		/// Checks that \ref component::type is either empty or the specified type.
		inline void check_type(const component &comp, std::u8string_view target) {
			if (!comp.is_type_or_empty(target)) {
				logger::get().log_warning(CP_HERE) << "invalid target type: " << target;
			}
		}
		/// Checks that this \ref component is the last one.
		inline void check_finished(component_list::const_iterator begin, component_list::const_iterator end) {
			if (++begin != end) {
				logger::get().log_warning(CP_HERE) << "redundant properties";
			}
		}
		/// Checks that the component does not have an index.
		inline void check_no_index(const component &comp) {
			if (comp.index.has_value()) {
				logger::get().log_warning(CP_HERE) << "unexpected index";
			}
		}

#define CP_APB_CONCAT(X, Y) X Y // workaround for MSVC
#define CP_APB_EXPAND_CALL(F, ...) CP_APB_CONCAT(F, (__VA_ARGS__))


#define CP_APB_MUST_TERMINATE check_finished(begin, end)

#define CP_APB_NO_INDEX check_no_index(*begin)

#define CP_APB_DO_CHECK_TYPE(TYPE, TYPENAME) check_type(*begin, u8 ## #TYPENAME)
#define CP_APB_CHECK_TYPE CP_APB_EXPAND_CALL(CP_APB_DO_CHECK_TYPE, CP_APB_CURRENT_TYPE)


#define CP_APB_SUBJECT_INFO_T member_information<typename Comp::input_type>

#define CP_APB_MAY_TERMINATE_EARLY                                                                 \
	if (begin == end) {                                                                            \
		CP_APB_SUBJECT_INFO_T res;                                                                 \
		res.member = std::make_unique<MemberAccess<Comp>>(comp);                                   \
		res.parser = std::make_unique<typed_animation_value_parser<typename Comp::output_type>>(); \
		return res;                                                                                \
	}

#define CP_APB_MUST_NOT_TERMINATE_EARLY                                              \
	if (begin == end) {                                                              \
		logger::get().log_warning(CP_HERE) << "animation path terminated too early"; \
		return CP_APB_SUBJECT_INFO_T();                                              \
	}


#define CP_APB_GETTER_NAME(TYPENAME) get_ ## TYPENAME ## _property
#define CP_APB_DO_DECLARE_GETTER(TYPE, TYPENAME)                                            \
	template <                                                                              \
		template <typename> typename MemberAccess, typename Comp                            \
	> inline CP_APB_SUBJECT_INFO_T CP_APB_GETTER_NAME(TYPENAME) (                           \
		component_list::const_iterator begin, component_list::const_iterator end, Comp comp \
	)
#define CP_APB_DECLARE_GETTER CP_APB_EXPAND_CALL(CP_APB_DO_DECLARE_GETTER, CP_APB_CURRENT_TYPE)
#define CP_APB_START_GETTER CP_APB_DECLARE_GETTER {

#define CP_APB_DO_DEFINE_PROPERTY_GETTER(TYPE, TYPENAME)                       \
	template <> member_information<TYPE> get_member_subject<TYPE>(             \
		component_list::const_iterator beg, component_list::const_iterator end \
	) {                                                                        \
		return CP_APB_GETTER_NAME(TYPENAME)<component_member_access>(          \
			beg, end, getter_components::dummy_component<TYPE>()               \
			);                                                                 \
	}
#define CP_APB_DEFINE_PROPERTY_GETTER CP_APB_EXPAND_CALL(CP_APB_DO_DEFINE_PROPERTY_GETTER, CP_APB_CURRENT_TYPE)

#define CP_APB_END_GETTER                                         \
		logger::get().log_warning(CP_HERE) << "invalid property"; \
		return CP_APB_SUBJECT_INFO_T();                           \
	}                                                             \
	CP_APB_DEFINE_PROPERTY_GETTER


#define CP_APB_DO_TRY_FORWARD_MEMBER(TYPE, TYPENAME, PROP_NAME, USE_PROP_NAME, PROP_TYPENAME)         \
	if (begin->property == u8 ## #USE_PROP_NAME) {                                                    \
		CP_APB_NO_INDEX;                                                                              \
		return CP_APB_GETTER_NAME(PROP_TYPENAME)<MemberAccess>(++begin, end, getter_components::pair( \
			comp, getter_components::member_component<&TYPE::PROP_NAME>()                             \
		));                                                                                           \
	}
#define CP_APB_TRY_FORWARD_MEMBER(PROP_NAME, PROP_TYPENAME) \
	CP_APB_EXPAND_CALL(CP_APB_DO_TRY_FORWARD_MEMBER, CP_APB_CURRENT_TYPE, PROP_NAME, PROP_NAME, PROP_TYPENAME)

#define CP_APB_TRY_FORWARD_MEMBER_CUSTOM_NAME(PROP_NAME, USE_PROP_NAME, PROP_TYPENAME) \
	CP_APB_EXPAND_CALL(CP_APB_DO_TRY_FORWARD_MEMBER, CP_APB_CURRENT_TYPE, PROP_NAME, USE_PROP_NAME, PROP_TYPENAME)

#define CP_APB_DO_TRY_FORWARD_VARIANT(TYPE, TYPENAME, VARIANT_TYPE, PROP_NAME, TARGET_TYPE, TARGET_TYPENAME) \
	if (begin->type == u8 ## #TARGET_TYPENAME) {                                                             \
		return CP_APB_GETTER_NAME(TARGET_TYPENAME)<MemberAccess>(begin, end, getter_components::pair(        \
			getter_components::pair(comp, getter_components::member_component<&TYPE::PROP_NAME>()),          \
			getter_components::variant_component<TYPE::VARIANT_TYPE, TARGET_TYPE>()                          \
		));                                                                                                  \
	}
#define CP_APB_TRY_FORWARD_VARIANT(TARGET_TYPE, TARGET_TYPENAME)                         \
	CP_APB_EXPAND_CALL(                                                                  \
		CP_APB_DO_TRY_FORWARD_VARIANT, CP_APB_CURRENT_TYPE, CP_APB_CURRENT_VARIANT_INFO, \
		TARGET_TYPE, TARGET_TYPENAME                                                     \
	)

#define CP_APB_DO_TRY_FORWARD_ARRAY(TYPE, TYPENAME, PROP_NAME, TARGET_TYPE, TARGET_TYPENAME)              \
	if (begin->property == u8 ## #PROP_NAME && begin->index.has_value()) {                                \
		return CP_APB_GETTER_NAME(TARGET_TYPENAME)<MemberAccess>(++begin, end, getter_components::pair(   \
			getter_components::pair(comp, getter_components::member_component<&TYPE::PROP_NAME>()),       \
			getter_components::array_component<TARGET_TYPE>(begin->index.value())                         \
		));                                                                                               \
	}
#define CP_APB_TRY_FORWARD_ARRAY(PROP_NAME, TARGET_TYPE, TARGET_TYPENAME) \
	CP_APB_EXPAND_CALL(CP_APB_DO_TRY_FORWARD_ARRAY, CP_APB_CURRENT_TYPE, PROP_NAME, TARGET_TYPE, TARGET_TYPENAME)



		// primitive types
#define CP_APB_CURRENT_TYPE bool, bool
		CP_APB_START_GETTER
			CP_APB_MAY_TERMINATE_EARLY;
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE int, int
		CP_APB_START_GETTER
			CP_APB_MAY_TERMINATE_EARLY;
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE double, double
		CP_APB_START_GETTER
			CP_APB_MAY_TERMINATE_EARLY;
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE std::u8string, string
		CP_APB_START_GETTER
			CP_APB_MAY_TERMINATE_EARLY;
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE


		// enums
#define CP_APB_CURRENT_TYPE anchor, anchor
		CP_APB_START_GETTER
			CP_APB_MAY_TERMINATE_EARLY;
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE size_allocation_type, size_allocation_type
		CP_APB_START_GETTER
			CP_APB_MAY_TERMINATE_EARLY;
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE cursor, cursor
		CP_APB_START_GETTER
			CP_APB_MAY_TERMINATE_EARLY;
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE visibility, visibility
		CP_APB_START_GETTER
			CP_APB_MAY_TERMINATE_EARLY;
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE orientation, orientation
		CP_APB_START_GETTER
			CP_APB_MAY_TERMINATE_EARLY;
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE font_style, font_style
		CP_APB_START_GETTER
			CP_APB_MAY_TERMINATE_EARLY;
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE font_weight, font_weight
		CP_APB_START_GETTER
			CP_APB_MAY_TERMINATE_EARLY;
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE font_stretch, font_stretch
		CP_APB_START_GETTER
			CP_APB_MAY_TERMINATE_EARLY;
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE


		// other basic types
#define CP_APB_CURRENT_TYPE std::shared_ptr<bitmap>, bitmap
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
			CP_APB_MAY_TERMINATE_EARLY;
			CP_APB_CHECK_TYPE;

			CP_APB_TRY_FORWARD_MEMBER(relative, double);
			CP_APB_TRY_FORWARD_MEMBER(absolute, double);
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE relative_vec2d, rel_vec2d
		CP_APB_START_GETTER
			CP_APB_MAY_TERMINATE_EARLY;
			CP_APB_CHECK_TYPE;

			CP_APB_TRY_FORWARD_MEMBER(relative, vec2d);
			CP_APB_TRY_FORWARD_MEMBER(absolute, vec2d);
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE font_parameters, font_param
		CP_APB_START_GETTER
			CP_APB_MUST_NOT_TERMINATE_EARLY;
			CP_APB_CHECK_TYPE;

			CP_APB_TRY_FORWARD_MEMBER(family, string);
			CP_APB_TRY_FORWARD_MEMBER(size, double);
			CP_APB_TRY_FORWARD_MEMBER(style, font_style);
			CP_APB_TRY_FORWARD_MEMBER(weight, font_weight);
			CP_APB_TRY_FORWARD_MEMBER(stretch, font_stretch);
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE


		// transforms
#define CP_APB_CURRENT_TYPE transform_parameters::generic, transform
		template <
			template <typename> typename, typename Comp, std::size_t = 0
		> inline CP_APB_SUBJECT_INFO_T CP_APB_GETTER_NAME(transform) (
			component_list::const_iterator, component_list::const_iterator, Comp
		);
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE transform_parameters::translation, translation_transform
		CP_APB_START_GETTER
			CP_APB_MUST_NOT_TERMINATE_EARLY;
			CP_APB_CHECK_TYPE;

			CP_APB_TRY_FORWARD_MEMBER(offset, rel_vec2d);
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE transform_parameters::scale, scale_transform
		CP_APB_START_GETTER
			CP_APB_MUST_NOT_TERMINATE_EARLY;
			CP_APB_CHECK_TYPE;

			CP_APB_TRY_FORWARD_MEMBER(center, rel_vec2d);
			CP_APB_TRY_FORWARD_MEMBER(scale_factor, vec2d);
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE transform_parameters::rotation, rotation_transform
		CP_APB_START_GETTER
			CP_APB_MUST_NOT_TERMINATE_EARLY;
			CP_APB_CHECK_TYPE;

			CP_APB_TRY_FORWARD_MEMBER(center, rel_vec2d);
			CP_APB_TRY_FORWARD_MEMBER(angle, double);
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE transform_parameters::collection, transform_collection
		template <
			template <typename> typename MemberAccess, typename Comp, std::size_t Count = 0
		> inline CP_APB_SUBJECT_INFO_T CP_APB_GETTER_NAME(transform_collection) (
			component_list::const_iterator begin, component_list::const_iterator end, [[maybe_unused]] Comp comp
		) {
			CP_APB_MUST_NOT_TERMINATE_EARLY;
			CP_APB_CHECK_TYPE;

			if constexpr (Count < 1) {
				if (begin->property == u8"children" && begin->index.has_value()) {
					auto nextcomp = getter_components::pair(
						getter_components::pair(
							comp, getter_components::member_component<&transform_parameters::collection::components>()
						),
						getter_components::array_component<transform_parameters::generic>(begin->index.value())
					);
					return CP_APB_GETTER_NAME(transform)<
						MemberAccess, std::decay_t<decltype(nextcomp)>, Count + 1
					>(++begin, end, nextcomp);
				}
			}
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE transform_parameters::generic, transform
#define CP_APB_CURRENT_VARIANT_INFO value_type, value
		template <
			template <typename> typename MemberAccess, typename Comp, std::size_t Count
		> inline CP_APB_SUBJECT_INFO_T CP_APB_GETTER_NAME(transform) (
			component_list::const_iterator begin, component_list::const_iterator end, Comp comp
		) {
			CP_APB_MUST_NOT_TERMINATE_EARLY;

			if (begin->is_type_or_empty(u8"transform") && begin->property == u8"value") {
				++begin;
				CP_APB_MUST_NOT_TERMINATE_EARLY;
			}

			CP_APB_TRY_FORWARD_VARIANT(transform_parameters::translation, translation_transform);
			CP_APB_TRY_FORWARD_VARIANT(transform_parameters::scale, scale_transform);
			CP_APB_TRY_FORWARD_VARIANT(transform_parameters::rotation, rotation_transform);
			/*if (begin->type == u8"transform_collection") {
				CP_APB_NO_INDEX;
				auto &&nextcomp = getter_components::pair(
					getter_components::pair(
						comp, getter_components::member_component<&transform_parameters::generic::value>()
					),
					getter_components::variant_component<transform_parameters::generic::value_type, transform_parameters::collection>()
				);
				return CP_APB_GETTER_NAME(transform_collection)<
					MemberAccess, std::decay_t<decltype(nextcomp)>, Count
				>(begin, end, nextcomp);
			}*/
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

#define CP_APB_CURRENT_TYPE brush_parameters::solid_color, solid_color_brush
		CP_APB_START_GETTER
			CP_APB_MUST_NOT_TERMINATE_EARLY;
			CP_APB_CHECK_TYPE;

			CP_APB_TRY_FORWARD_MEMBER(color, color);
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE brush_parameters::linear_gradient, linear_gradient_brush
		CP_APB_START_GETTER
			CP_APB_MUST_NOT_TERMINATE_EARLY;
			CP_APB_CHECK_TYPE;

			CP_APB_TRY_FORWARD_ARRAY(gradient_stops, gradient_stop, gradient_stop);
			CP_APB_TRY_FORWARD_MEMBER(from, rel_vec2d);
			CP_APB_TRY_FORWARD_MEMBER(to, rel_vec2d);
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE brush_parameters::radial_gradient, radial_gradient_brush
		CP_APB_START_GETTER
			CP_APB_MUST_NOT_TERMINATE_EARLY;
			CP_APB_CHECK_TYPE;

			CP_APB_TRY_FORWARD_ARRAY(gradient_stops, gradient_stop, gradient_stop);
			CP_APB_TRY_FORWARD_MEMBER(center, rel_vec2d);
			CP_APB_TRY_FORWARD_MEMBER(radius, double);
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE brush_parameters::bitmap_pattern, bitmap_brush
		CP_APB_START_GETTER
			CP_APB_MUST_NOT_TERMINATE_EARLY;
			CP_APB_CHECK_TYPE;

			CP_APB_TRY_FORWARD_MEMBER(image, bitmap);
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE generic_brush_parameters, brush
#define CP_APB_CURRENT_VARIANT_INFO value_type, value
		CP_APB_START_GETTER
			CP_APB_MUST_NOT_TERMINATE_EARLY;

			if (begin->is_type_or_empty(u8"brush")) {
				CP_APB_TRY_FORWARD_MEMBER(transform, transform);
				if (begin->property == u8"value") {
					++begin;
					CP_APB_MUST_NOT_TERMINATE_EARLY;
				}
			}

			CP_APB_TRY_FORWARD_VARIANT(brush_parameters::solid_color, solid_color_brush);
			CP_APB_TRY_FORWARD_VARIANT(brush_parameters::linear_gradient, linear_gradient_brush);
			CP_APB_TRY_FORWARD_VARIANT(brush_parameters::radial_gradient, radial_gradient_brush);
			CP_APB_TRY_FORWARD_VARIANT(brush_parameters::bitmap_pattern, bitmap_brush);
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE generic_pen_parameters, pen
		CP_APB_START_GETTER
			CP_APB_MUST_NOT_TERMINATE_EARLY;

			if (begin->is_type_or_empty(u8"pen")) {
				CP_APB_TRY_FORWARD_MEMBER(thickness, double);
				if (begin->property == u8"brush") {
					++begin;
					CP_APB_MUST_NOT_TERMINATE_EARLY;
				}
			}

			return CP_APB_GETTER_NAME(brush)<MemberAccess>(begin, end, getter_components::pair(
				comp, getter_components::member_component<&generic_pen_parameters::brush>()
			));
		}
		CP_APB_DEFINE_PROPERTY_GETTER
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
			CP_APB_TRY_FORWARD_MEMBER(radius, rel_vec2d);
			CP_APB_TRY_FORWARD_MEMBER(rotation, double);
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

			if (begin->is_type_or_empty(u8"subpath_part") && begin->property == u8"value") {
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

			CP_APB_TRY_FORWARD_MEMBER(transform, transform);
			CP_APB_TRY_FORWARD_MEMBER(fill, brush);
			CP_APB_TRY_FORWARD_MEMBER(stroke, pen);
			if (begin->is_type_or_empty(u8"geometry")) {
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


#define CP_APB_CURRENT_TYPE visuals, visuals
		CP_APB_START_GETTER
			CP_APB_MUST_NOT_TERMINATE_EARLY;
			CP_APB_CHECK_TYPE;

			CP_APB_TRY_FORWARD_ARRAY(geometries, generic_visual_geometry, geometry);
			CP_APB_TRY_FORWARD_MEMBER(transform, transform);
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE

#define CP_APB_CURRENT_TYPE element_layout, element_layout
		CP_APB_START_GETTER
			CP_APB_MUST_NOT_TERMINATE_EARLY;
			CP_APB_CHECK_TYPE;

			CP_APB_TRY_FORWARD_MEMBER(margin, thickness);
			CP_APB_TRY_FORWARD_MEMBER(padding, thickness);
			CP_APB_TRY_FORWARD_MEMBER(size, vec2d);
			CP_APB_TRY_FORWARD_MEMBER_CUSTOM_NAME(elem_anchor, anchor, anchor);
			CP_APB_TRY_FORWARD_MEMBER(width_alloc, size_allocation_type);
			CP_APB_TRY_FORWARD_MEMBER(height_alloc, size_allocation_type);
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE
	}
}
