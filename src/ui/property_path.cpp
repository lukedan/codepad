// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/property_path.h"

/// \file
/// Implementation of certain methods used to access properties of elements.
///
/// \todo Use reflection.

#include "codepad/ui/element.h"
#include "codepad/ui/manager.h"
#include "codepad/ui/renderer.h"
#include "codepad/ui/json_parsers.inl"

namespace codepad::ui {
	namespace property_path {
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
	}

	namespace property_finders {
		template <> property_info find_property_info<vec2d>(component_property_accessor_builder &builder) {
			if (!builder.move_next()) {
				return builder.finish_and_create_property_info<vec2d>();
			}
			builder.expect_type(u8"vec2d");
			if (builder.current_component().property == u8"x") {
				return builder.append_member_and_find_property_info<&vec2d::x>();
			}
			if (builder.current_component().property == u8"y") {
				return builder.append_member_and_find_property_info<&vec2d::y>();
			}
			return builder.fail();
		}

		template <> property_info find_property_info<colord>(component_property_accessor_builder &builder) {
			if (!builder.move_next()) {
				return builder.finish_and_create_property_info<colord>();
			}
			builder.expect_type(u8"color");
			if (builder.current_component().property == u8"r") {
				return builder.append_member_and_find_property_info<&colord::r>();
			}
			if (builder.current_component().property == u8"g") {
				return builder.append_member_and_find_property_info<&colord::g>();
			}
			if (builder.current_component().property == u8"b") {
				return builder.append_member_and_find_property_info<&colord::b>();
			}
			if (builder.current_component().property == u8"a") {
				return builder.append_member_and_find_property_info<&colord::a>();
			}
			return builder.fail();
		}

		template <> property_info find_property_info<thickness>(component_property_accessor_builder &builder) {
			if (!builder.move_next()) {
				return builder.finish_and_create_property_info<thickness>();
			}
			builder.expect_type(u8"thickness");
			if (builder.current_component().property == u8"left") {
				return builder.append_member_and_find_property_info<&thickness::left>();
			}
			if (builder.current_component().property == u8"top") {
				return builder.append_member_and_find_property_info<&thickness::top>();
			}
			if (builder.current_component().property == u8"right") {
				return builder.append_member_and_find_property_info<&thickness::right>();
			}
			if (builder.current_component().property == u8"bottom") {
				return builder.append_member_and_find_property_info<&thickness::bottom>();
			}
			return builder.fail();
		}

		template <> property_info find_property_info<font_parameters>(component_property_accessor_builder &builder) {
			if (!builder.move_next()) {
				return builder.finish_and_create_property_info<font_parameters>();
			}
			builder.expect_type(u8"font");
			if (builder.current_component().property == u8"family") {
				return builder.append_member_and_find_property_info<&font_parameters::family>();
			}
			if (builder.current_component().property == u8"size") {
				return builder.append_member_and_find_property_info<&font_parameters::size>();
			}
			if (builder.current_component().property == u8"style") {
				return builder.append_member_and_find_property_info<&font_parameters::style>();
			}
			if (builder.current_component().property == u8"weight") {
				return builder.append_member_and_find_property_info<&font_parameters::weight>();
			}
			if (builder.current_component().property == u8"stretch") {
				return builder.append_member_and_find_property_info<&font_parameters::stretch>();
			}
			return builder.fail();
		}

		template <> property_info find_property_info<gradient_stop>(component_property_accessor_builder &builder) {
			if (!builder.move_next()) {
				return builder.finish_and_create_property_info<gradient_stop>();
			}
			builder.expect_type(u8"gradient_stop");
			if (builder.current_component().property == u8"color") {
				return builder.append_member_and_find_property_info<&gradient_stop::color>();
			}
			if (builder.current_component().property == u8"position") {
				return builder.append_member_and_find_property_info<&gradient_stop::position>();
			}
			return builder.fail();
		}

		template <> property_info find_property_info<relative_vec2d>(component_property_accessor_builder &builder) {
			if (!builder.move_next()) {
				return builder.finish_and_create_property_info<relative_vec2d>();
			}
			builder.expect_type(u8"rel_vec2d");
			if (builder.current_component().property == u8"relative") {
				return builder.append_member_and_find_property_info<&relative_vec2d::relative>();
			}
			if (builder.current_component().property == u8"absolute") {
				return builder.append_member_and_find_property_info<&relative_vec2d::absolute>();
			}
			return builder.fail();
		}

		template <> property_info find_property_info<relative_double>(component_property_accessor_builder &builder) {
			if (!builder.move_next()) {
				return builder.finish_and_create_property_info<relative_double>();
			}
			builder.expect_type(u8"rel_double");
			if (builder.current_component().property == u8"relative") {
				return builder.append_member_and_find_property_info<&relative_double::relative>();
			}
			if (builder.current_component().property == u8"absolute") {
				return builder.append_member_and_find_property_info<&relative_double::absolute>();
			}
			return builder.fail();
		}


		template <> property_info find_property_info<transform_parameters::translation>(
			component_property_accessor_builder &builder
		) {
			if (!builder.move_next()) {
				return builder.fail(); // no parser for transform sub-types
			}
			builder.expect_type(u8"translation_transform");
			if (builder.current_component().property == u8"offset") {
				return builder.append_member_and_find_property_info<&transform_parameters::translation::offset>();
			}
			return builder.fail();
		}

		template <> property_info find_property_info<transform_parameters::scale>(
			component_property_accessor_builder &builder
		) {
			if (!builder.move_next()) {
				return builder.fail(); // no parser for transform sub-types
			}
			builder.expect_type(u8"scale_transform");
			if (builder.current_component().property == u8"center") {
				return builder.append_member_and_find_property_info<&transform_parameters::scale::center>();
			}
			if (builder.current_component().property == u8"scale_factor") {
				return builder.append_member_and_find_property_info<&transform_parameters::scale::scale_factor>();
			}
			return builder.fail();
		}

		template <> property_info find_property_info<transform_parameters::rotation>(
			component_property_accessor_builder &builder
		) {
			if (!builder.move_next()) {
				return builder.fail(); // no parser for transform sub-types
			}
			builder.expect_type(u8"rotation_transform");
			if (builder.current_component().property == u8"center") {
				return builder.append_member_and_find_property_info<&transform_parameters::rotation::center>();
			}
			if (builder.current_component().property == u8"angle") {
				return builder.append_member_and_find_property_info<&transform_parameters::rotation::angle>();
			}
			return builder.fail();
		}

		template <> property_info find_property_info<transform_parameters::collection>(
			component_property_accessor_builder &builder
		) {
			if (!builder.move_next()) {
				return builder.fail(); // no parser for transform sub-types
			}
			builder.expect_type(u8"transform_collection");
			if (builder.current_component().property == u8"children") {
				builder.make_append_member_pointer_component<&transform_parameters::collection::components>();
				return find_array_property_info<transform_parameters::generic>(builder);
			}
			return builder.fail();
		}

		template <> property_info find_property_info<transform_parameters::generic>(
			component_property_accessor_builder &builder
		) {
			auto *next_comp = builder.peek_next();
			if (!next_comp) {
				return builder.finish_and_create_property_info<transform_parameters::generic>();
			}
			if (next_comp->is_type_or_empty(u8"transform")) {
				builder.move_next();
				if (builder.current_component().property != u8"value") {
					return builder.fail();
				}
				next_comp = builder.peek_next();
				if (!next_comp) {
					return builder.fail();
				}
			}
			if (next_comp->type == u8"translation_transform") {
				return builder.append_variant_and_find_property_info<
					&transform_parameters::generic::value, transform_parameters::translation
				>();
			}
			if (next_comp->type == u8"scale_transform") {
				return builder.append_variant_and_find_property_info<
					&transform_parameters::generic::value, transform_parameters::scale
				>();
			}
			if (next_comp->type == u8"rotation_transform") {
				return builder.append_variant_and_find_property_info<
					&transform_parameters::generic::value, transform_parameters::rotation
				>();
			}
			if (next_comp->type == u8"transform_collection") {
				return builder.append_variant_and_find_property_info<
					&transform_parameters::generic::value, transform_parameters::collection
				>();
			}
			return builder.fail();
		}


		template <> property_info find_property_info<brush_parameters::solid_color>(
			component_property_accessor_builder &builder
		) {
			if (!builder.move_next()) {
				return builder.finish_and_create_property_info<brush_parameters::solid_color>();
			}
			builder.expect_type(u8"solid_color_brush");
			if (builder.current_component().property == u8"color") {
				return builder.append_member_and_find_property_info<&brush_parameters::solid_color::color>();
			}
			return builder.fail();
		}

		template <> property_info find_property_info<brush_parameters::linear_gradient>(
			component_property_accessor_builder &builder
		) {
			if (!builder.move_next()) {
				return builder.finish_and_create_property_info<brush_parameters::linear_gradient>();
			}
			builder.expect_type(u8"linear_gradient_brush");
			if (builder.current_component().property == u8"gradient_stops") {
				builder.make_append_member_pointer_component<&brush_parameters::linear_gradient::gradient_stops>();
				return find_array_property_info<gradient_stop>(builder);
			}
			if (builder.current_component().property == u8"from") {
				return builder.append_member_and_find_property_info<&brush_parameters::linear_gradient::from>();
			}
			if (builder.current_component().property == u8"to") {
				return builder.append_member_and_find_property_info<&brush_parameters::linear_gradient::to>();
			}
			return builder.fail();
		}

		template <> property_info find_property_info<brush_parameters::radial_gradient>(
			component_property_accessor_builder &builder
		) {
			if (!builder.move_next()) {
				return builder.finish_and_create_property_info<brush_parameters::radial_gradient>();
			}
			builder.expect_type(u8"radial_gradient_brush");
			if (builder.current_component().property == u8"gradient_stops") {
				builder.make_append_member_pointer_component<&brush_parameters::radial_gradient::gradient_stops>();
				return find_array_property_info<gradient_stop>(builder);
			}
			if (builder.current_component().property == u8"center") {
				return builder.append_member_and_find_property_info<&brush_parameters::radial_gradient::center>();
			}
			if (builder.current_component().property == u8"radius") {
				return builder.append_member_and_find_property_info<&brush_parameters::radial_gradient::radius>();
			}
			return builder.fail();
		}

		template <> property_info find_property_info_managed<brush_parameters::bitmap_pattern>(
			component_property_accessor_builder &builder, manager &man
		) {
			if (!builder.move_next()) {
				return builder.finish_and_create_property_info_managed<brush_parameters::bitmap_pattern>(man);
			}
			builder.expect_type(u8"bitmap_brush");
			if (builder.current_component().property == u8"image") {
				return builder.append_member_and_find_property_info_managed<
					&brush_parameters::bitmap_pattern::image
				>(man);
			}
			return builder.fail();
		}

		template <> property_info find_property_info_managed<generic_brush_parameters>(
			component_property_accessor_builder &builder, manager &man
		) {
			auto *next_comp = builder.peek_next();
			if (!next_comp) {
				return builder.finish_and_create_property_info_managed<generic_brush_parameters>(man);
			}
			if (next_comp->is_type_or_empty(u8"brush")) {
				builder.move_next();
				if (builder.current_component().property == u8"transform") {
					return builder.append_member_and_find_property_info<&generic_brush_parameters::transform>();
				}
				// handle variant
				if (builder.current_component().property != u8"value") {
					return builder.fail();
				}
				next_comp = builder.peek_next();
				if (!next_comp) {
					return builder.fail();
				}
			}
			if (next_comp->type == u8"solid_color_brush") {
				return builder.append_variant_and_find_property_info<
					&generic_brush_parameters::value, brush_parameters::solid_color
				>();
			}
			if (next_comp->type == u8"linear_gradient_brush") {
				return builder.append_variant_and_find_property_info<
					&generic_brush_parameters::value, brush_parameters::linear_gradient
				>();
			}
			if (next_comp->type == u8"radial_gradient_brush") {
				return builder.append_variant_and_find_property_info<
					&generic_brush_parameters::value, brush_parameters::radial_gradient
				>();
			}
			if (next_comp->type == u8"bitmap_brush") {
				return builder.append_variant_and_find_property_info_managed<
					&generic_brush_parameters::value, brush_parameters::bitmap_pattern
				>(man);
			}
			return builder.fail();
		}

		template <> property_info find_property_info_managed<generic_pen_parameters>(
			component_property_accessor_builder &builder, manager &man
		) {
			auto *next_comp = builder.peek_next();
			if (!next_comp) {
				return builder.finish_and_create_property_info_managed<generic_pen_parameters>(man);
			}
			if (next_comp->is_type_or_empty(u8"pen")) {
				builder.move_next();
				if (builder.current_component().property == u8"thickness") {
					return builder.append_member_and_find_property_info<&generic_pen_parameters::thickness>();
				}
				// forward to generic_brush_parameters
				if (builder.current_component().property != u8"brush") {
					return builder.fail();
				}
			}
			return builder.append_member_and_find_property_info_managed<&generic_pen_parameters::brush>(man);
		}


		template <> property_info find_property_info<geometries::rectangle>(
			component_property_accessor_builder &builder
		) {
			if (!builder.move_next()) {
				return builder.finish_and_create_property_info<geometries::rectangle>();
			}
			builder.expect_type(u8"rectangle");
			if (builder.current_component().property == u8"top_left") {
				return builder.append_member_and_find_property_info<&geometries::rectangle::top_left>();
			}
			if (builder.current_component().property == u8"bottom_right") {
				return builder.append_member_and_find_property_info<&geometries::rectangle::bottom_right>();
			}
			return builder.fail();
		}

		template <> property_info find_property_info_managed<generic_visual_geometry>(
			component_property_accessor_builder &builder, manager &man
		) {
			auto next_comp = builder.peek_next();
			if (!next_comp) {
				return builder.finish_and_create_property_info_managed<visuals>(man);
			}
			if (next_comp->is_type_or_empty(u8"geometry")) {
				builder.move_next();
				if (builder.current_component().property == u8"transform") {
					return builder.append_member_and_find_property_info<&generic_visual_geometry::transform>();
				}
				if (builder.current_component().property == u8"fill") {
					return builder.append_member_and_find_property_info_managed<&generic_visual_geometry::fill>(man);
				}
				if (builder.current_component().property == u8"stroke") {
					return
						builder.append_member_and_find_property_info_managed<&generic_visual_geometry::stroke>(man);
				}
				// handle variant
				if (builder.current_component().property != u8"value") {
					return builder.fail();
				}
				next_comp = builder.peek_next();
				if (!next_comp) {
					return builder.fail();
				}
			}
			if (next_comp->type == u8"rectangle") {
				return builder.append_variant_and_find_property_info<
					&generic_visual_geometry::value, geometries::rectangle
				>();
			}
			if (next_comp->type == u8"rounded_rectangle") {
				return builder.append_variant_and_find_property_info<
					&generic_visual_geometry::value, geometries::rounded_rectangle
				>();
			}
			if (next_comp->type == u8"ellipse") {
				return builder.append_variant_and_find_property_info<
					&generic_visual_geometry::value, geometries::ellipse
				>();
			}
			if (next_comp->type == u8"path") {
				return builder.append_variant_and_find_property_info<
					&generic_visual_geometry::value, geometries::path
				>();
			}
			return builder.fail();
		}

		template <> property_info find_property_info_managed<visuals>(
			component_property_accessor_builder &builder, manager &man
		) {
			if (!builder.move_next()) {
				return builder.finish_and_create_property_info_managed<visuals>(man);
			}
			builder.expect_type(u8"element_layout");
			if (builder.current_component().property == u8"geometries") {
				builder.make_append_member_pointer_component<&visuals::geometries>();
				return find_array_property_info<
					generic_visual_geometry, find_property_info_managed<generic_visual_geometry>
				>(builder, man);
			}
			if (builder.current_component().property == u8"transform") {
				return builder.append_member_and_find_property_info<&visuals::transform>();
			}
			return builder.fail();
		}

		template <> property_info find_property_info<element_layout>(component_property_accessor_builder &builder) {
			if (!builder.move_next()) {
				return builder.finish_and_create_property_info<element_layout>();
			}
			builder.expect_type(u8"element_layout");
			if (builder.current_component().property == u8"margin") {
				return builder.append_member_and_find_property_info<&element_layout::margin>();
			}
			if (builder.current_component().property == u8"padding") {
				return builder.append_member_and_find_property_info<&element_layout::padding>();
			}
			if (builder.current_component().property == u8"size") {
				return builder.append_member_and_find_property_info<&element_layout::size>();
			}
			if (builder.current_component().property == u8"anchor") {
				return builder.append_member_and_find_property_info<&element_layout::elem_anchor>();
			}
			if (builder.current_component().property == u8"width_alloc") {
				return builder.append_member_and_find_property_info<&element_layout::width_alloc>();
			}
			if (builder.current_component().property == u8"height_alloc") {
				return builder.append_member_and_find_property_info<&element_layout::height_alloc>();
			}
			return builder.fail();
		}
	}

/*	namespace builder {
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
		res.parser = std::make_unique<typed_property_value_parser<typename Comp::output_type>>(); \
		return res;                                                                                \
	}

#define CP_APB_MUST_NOT_TERMINATE_EARLY                                              \
	if (begin == end) {                                                              \
		logger::get().log_warning(CP_HERE) << "property path terminated too early"; \
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


		// other basic types
#define CP_APB_CURRENT_TYPE std::shared_ptr<bitmap>, bitmap
		CP_APB_START_GETTER
			CP_APB_MAY_TERMINATE_EARLY;
		CP_APB_END_GETTER
#undef CP_APB_CURRENT_TYPE


		// brushes
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
	}*/
}
