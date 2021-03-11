// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Implementation of enum parsers.

#include "codepad/ui/misc.h"
#include "codepad/ui/hotkey_registry.h"
#include "codepad/ui/elements/label.h"

namespace codepad {
	// TODO case-insensitive comparison
	std::optional<ui::modifier_keys> enum_parser<ui::modifier_keys>::parse(std::u8string_view str) {
		if (str == u8"ctrl") {
			return ui::modifier_keys::control;
		} else if (str == u8"alt") {
			return ui::modifier_keys::alt;
		} else if (str == u8"shift") {
			return ui::modifier_keys::shift;
		} else if (str == u8"super") {
			return ui::modifier_keys::super;
		}
		return std::nullopt;
	}


	std::optional<ui::mouse_button> enum_parser<ui::mouse_button>::parse(std::u8string_view text) {
		if (text == u8"primary" || text == u8"m1") {
			return ui::mouse_button::primary;
		} else if (text == u8"secondary" || text == u8"m2") {
			return ui::mouse_button::secondary;
		} else if (text == u8"tertiary" || text == u8"middle") {
			return ui::mouse_button::tertiary;
		}
		return std::nullopt;
	}


	std::optional<ui::key> enum_parser<ui::key>::parse(std::u8string_view str) {
		if (str.length() == 1) {
			if (str[0] >= 'a' && str[0] <= 'z') {
				return static_cast<ui::key>(static_cast<std::size_t>(ui::key::a) + (str[0] - 'a'));
			}
			switch (str[0]) {
			case ' ':
				return ui::key::space;
			case '+':
				return ui::key::add;
			case '-':
				return ui::key::subtract;
			case '*':
				return ui::key::multiply;
			case '/':
				return ui::key::divide;
			}
		}
		if (str == u8"left") {
			return ui::key::left;
		} else if (str == u8"right") {
			return ui::key::right;
		} else if (str == u8"up") {
			return ui::key::up;
		} else if (str == u8"down") {
			return ui::key::down;
		} else if (str == u8"space") {
			return ui::key::space;
		} else if (str == u8"insert") {
			return ui::key::insert;
		} else if (str == u8"delete") {
			return ui::key::del;
		} else if (str == u8"backspace") {
			return ui::key::backspace;
		} else if (str == u8"home") {
			return ui::key::home;
		} else if (str == u8"end") {
			return ui::key::end;
		} else if (str == u8"enter") {
			return ui::key::enter;
		}
		return std::nullopt;
	}


	std::optional<ui::size_allocation_type> enum_parser<ui::size_allocation_type>::parse(std::u8string_view str) {
		if (str == u8"fixed" || str == u8"pixels" || str == u8"px") {
			return ui::size_allocation_type::fixed;
		} else if (str == u8"proportion" || str == u8"prop" || str == u8"*") {
			return ui::size_allocation_type::proportion;
		} else if (str == u8"automatic" || str == u8"auto") {
			return ui::size_allocation_type::automatic;
		}
		return std::nullopt;
	}


	std::optional<ui::font_style> enum_parser<ui::font_style>::parse(std::u8string_view str) {
		if (str == u8"normal") {
			return ui::font_style::normal;
		} else if (str == u8"italic") {
			return ui::font_style::italic;
		} else if (str == u8"oblique") {
			return ui::font_style::oblique;
		}
		return std::nullopt;
	}


	std::optional<ui::font_weight> enum_parser<ui::font_weight>::parse(std::u8string_view str) {
		if (str == u8"normal") {
			return ui::font_weight::normal;
		} else if (str == u8"bold") {
			return ui::font_weight::bold;
		}
		// TODO
		return std::nullopt;
	}


	std::optional<ui::wrapping_mode> enum_parser<ui::wrapping_mode>::parse(
		std::u8string_view str
	) {
		if (str == u8"none") {
			return ui::wrapping_mode::none;
		} else if (str == u8"wrap") {
			return ui::wrapping_mode::wrap;
		}
		return std::nullopt;
	}


	std::optional<ui::horizontal_text_alignment> enum_parser<ui::horizontal_text_alignment>::parse(
		std::u8string_view str
	) {
		if (str == u8"front") {
			return ui::horizontal_text_alignment::front;
		} else if (str == u8"center") {
			return ui::horizontal_text_alignment::center;
		} else if (str == u8"rear") {
			return ui::horizontal_text_alignment::rear;
		}
		return std::nullopt;
	}


	std::optional<ui::vertical_text_alignment> enum_parser<ui::vertical_text_alignment>::parse(
		std::u8string_view str
	) {
		if (str == u8"top") {
			return ui::vertical_text_alignment::top;
		} else if (str == u8"center") {
			return ui::vertical_text_alignment::center;
		} else if (str == u8"bottom") {
			return ui::vertical_text_alignment::bottom;
		}
		return std::nullopt;
	}


	std::optional<ui::label::wrapping_width_mode> enum_parser<ui::label::wrapping_width_mode>::parse(
		std::u8string_view str
	) {
		if (str == u8"client") {
			return ui::label::wrapping_width_mode::client;
		} else if (str == u8"custom") {
			return ui::label::wrapping_width_mode::custom;
		}
		return std::nullopt;
	}
}
