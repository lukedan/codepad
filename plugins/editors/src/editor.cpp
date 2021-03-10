// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/editor.h"

/// \file
/// Implementation of the editor.

#include "codepad/ui/json_parsers.inl"
#include "codepad/ui/manager.h"
#include "codepad/editors/manager.h"
#include "details.h"

namespace codepad::editors {
	bool contents_region_base::_register_edit_mode_changed_event(
		std::u8string_view name, std::function<void()> &callback
	) {
		// not using _event_helpers::try_register_event() here
		// since it takes a non-const reference to the callback
		if (name == u8"mode_changed_insert") {
			edit_mode_changed += [this, cb = std::move(callback)]() {
				if (is_insert_mode()) {
					cb();
				}
			};
			return true;
		}
		if (name == u8"mode_changed_overwrite") {
			edit_mode_changed += [this, cb = std::move(callback)]() {
				if (!is_insert_mode()) {
					cb();
				}
			};
			return true;
		}
		return false;
	}

	ui::property_info contents_region_base::_find_property_path(
		const ui::property_path::component_list &path
	) const {
		if (path.front().is_type_or_empty(u8"contents_region_base")) {
			if (path.front().property == u8"caret_visuals") {
				return ui::property_info::find_member_pointer_property_info_managed<
					&contents_region_base::_caret_visuals, element
				>(
					path, get_manager(),
					ui::property_info::make_typed_modification_callback<element, contents_region_base>(
						[](contents_region_base &rgn) {
							rgn.invalidate_visual();
						}
					)
				);
			}
			if (path.front().property == u8"selection_renderer") {
				ui::component_property_accessor_builder builder(
					path.begin(), path.end(),
					ui::property_info::make_typed_modification_callback<element, contents_region_base>(
						[](contents_region_base &rgn) {
							rgn.invalidate_visual();
						}
					)
				);
				builder.make_append_accessor_component<
					ui::property_path::address_accessor_components::dynamic_cast_component<
						contents_region_base, element
					>
				>();
				builder.make_append_member_pointer_component<&contents_region_base::_selection_renderer>();
				return decoration_renderer::find_property_info_handler(
					builder, get_manager(), _details::get_manager()
				);
			}
		}
		return ui::element::_find_property_path(path);
	}


	settings::retriever_parser<double> &editor::get_font_size_setting(settings &set) {
		static setting<double> _setting(
			{ u8"editor", u8"font_size" }, settings::basic_parsers::basic_type_with_default<double>(12.0)
		);
		return _setting.get(set);
	}

	settings::retriever_parser<std::u8string> &editor::get_font_family_setting(settings &set) {
		static setting<std::u8string> _setting(
			{ u8"editor", u8"font_family" },
			settings::basic_parsers::basic_type_with_default<std::u8string>(u8"Courier New")
		);
		return _setting.get(set);
	}

	settings::retriever_parser<std::vector<std::u8string>> &editor::get_interaction_modes_setting(
		settings &set
	) {
		static setting<std::vector<std::u8string>> _setting(
			{ u8"editor", u8"interaction_modes" },
			settings::basic_parsers::basic_type_with_default(
				std::vector<std::u8string>(), json::array_parser<std::u8string>()
			)
		);
		return _setting.get(set);
	}

	void editor::_initialize(std::u8string_view cls) {
		panel::_initialize(cls);

		_vert_scroll->actual_value_changed += [this](ui::scrollbar::value_changed_info&) {
			vertical_viewport_changed.invoke();
			invalidate_visual();
		};
		_hori_scroll->actual_value_changed += [this](ui::scrollbar::value_changed_info&) {
			horizontal_viewport_changed.invoke();
			invalidate_visual();
		};

		_contents->layout_changed += [this]() {
			vertical_viewport_changed.invoke();
			horizontal_viewport_changed.invoke();
			_reset_scrollbars();
		};
		_contents->content_visual_changed += [this]() {
			_reset_scrollbars();
		};
	}
}
