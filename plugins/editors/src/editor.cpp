// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/editor.h"

/// \file
/// Implementation of the editor.

#include "codepad/ui/json_parsers.inl"

namespace codepad::editors {
	const ui::property_mapping &contents_region_base::get_properties() const {
		return get_properties_static();
	}

	const ui::property_mapping &contents_region_base::get_properties_static() {
		static ui::property_mapping mapping;
		if (mapping.empty()) {
			mapping = ui::element::get_properties_static();
			mapping.emplace(
				u8"caret_visuals",
				std::make_shared<ui::member_pointer_property<&contents_region_base::_caret_visuals>>(
					[](contents_region_base &c) {
						c.invalidate_visual();
					}
					)
			);
			mapping.emplace(
				u8"selection_brush",
				std::make_shared<ui::member_pointer_property<&contents_region_base::_selection_brush>>(
					[](contents_region_base &c) {
						c.invalidate_visual();
					}
					)
			);
			mapping.emplace(
				u8"selection_pen",
				std::make_shared<ui::member_pointer_property<&contents_region_base::_selection_pen>>(
					[](contents_region_base &c) {
						c.invalidate_visual();
					}
					)
			);
		}

		return mapping;
	}

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
