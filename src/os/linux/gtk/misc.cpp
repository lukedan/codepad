// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/os/linux/gtk/misc.h"

namespace codepad::os::_details {
	const GdkCursorType cursor_set::cursor_ids[cursor_count] = {
		GDK_LEFT_PTR,
		GDK_WATCH,
		GDK_CROSSHAIR,
		GDK_HAND1,
		GDK_QUESTION_ARROW,
		GDK_XTERM,
		GDK_X_CURSOR,
		GDK_CROSS,
		GDK_TOP_LEFT_CORNER,
		GDK_SB_V_DOUBLE_ARROW,
		GDK_TOP_RIGHT_CORNER,
		GDK_SB_H_DOUBLE_ARROW,
		GDK_BLANK_CURSOR
	};

	cursor_set &cursor_set::get() {
		static cursor_set obj;
		return obj;
	}


	const guint keysym_mapping[ui::total_num_keys][max_keysym_mapping] = {
		{GDK_KEY_Cancel},
		{}, // xbutton_1
		{}, // xbutton_2
		{GDK_KEY_BackSpace},
		{GDK_KEY_Tab, GDK_KEY_KP_Tab, GDK_KEY_ISO_Left_Tab},
		{GDK_KEY_Clear},
		{GDK_KEY_Return, GDK_KEY_KP_Enter, GDK_KEY_ISO_Enter},
		{GDK_KEY_Shift_L, GDK_KEY_Shift_R},
		{GDK_KEY_Control_L, GDK_KEY_Control_R},
		{GDK_KEY_Alt_L, GDK_KEY_Alt_R},
		{GDK_KEY_Pause},
		{GDK_KEY_Caps_Lock},
		{GDK_KEY_Escape},
		{}, // convert
		{}, // nonconvert
		{GDK_KEY_space, GDK_KEY_KP_Space},
		{GDK_KEY_Page_Up, GDK_KEY_KP_Page_Up},
		{GDK_KEY_Page_Down, GDK_KEY_KP_Page_Down},
		{GDK_KEY_End, GDK_KEY_KP_End},
		{GDK_KEY_Home, GDK_KEY_KP_Home},
		{GDK_KEY_Left, GDK_KEY_KP_Left},
		{GDK_KEY_Up, GDK_KEY_KP_Up},
		{GDK_KEY_Right, GDK_KEY_KP_Right},
		{GDK_KEY_Down, GDK_KEY_KP_Down},
		{GDK_KEY_Select},
		{GDK_KEY_Print},
		{GDK_KEY_Execute},
		{}, // snapshot
		{GDK_KEY_Insert, GDK_KEY_KP_Insert},
		{GDK_KEY_Delete, GDK_KEY_KP_Delete},
		{GDK_KEY_Help},
		{GDK_KEY_a, GDK_KEY_A}, {GDK_KEY_b, GDK_KEY_B}, {GDK_KEY_c, GDK_KEY_C}, {GDK_KEY_d, GDK_KEY_D},
		{GDK_KEY_e, GDK_KEY_E}, {GDK_KEY_f, GDK_KEY_F}, {GDK_KEY_g, GDK_KEY_G}, {GDK_KEY_h, GDK_KEY_H},
		{GDK_KEY_i, GDK_KEY_I}, {GDK_KEY_j, GDK_KEY_J}, {GDK_KEY_k, GDK_KEY_K}, {GDK_KEY_l, GDK_KEY_L},
		{GDK_KEY_m, GDK_KEY_M}, {GDK_KEY_n, GDK_KEY_N}, {GDK_KEY_o, GDK_KEY_O}, {GDK_KEY_p, GDK_KEY_P},
		{GDK_KEY_q, GDK_KEY_Q}, {GDK_KEY_r, GDK_KEY_R}, {GDK_KEY_s, GDK_KEY_S}, {GDK_KEY_t, GDK_KEY_T},
		{GDK_KEY_u, GDK_KEY_U}, {GDK_KEY_v, GDK_KEY_V}, {GDK_KEY_w, GDK_KEY_W}, {GDK_KEY_x, GDK_KEY_X},
		{GDK_KEY_y, GDK_KEY_Y}, {GDK_KEY_z, GDK_KEY_Z},
		{}, // left_super
		{}, // right_super
		{}, // apps
		{GDK_KEY_Sleep},
		{GDK_KEY_KP_Multiply},
		{GDK_KEY_KP_Add},
		{GDK_KEY_KP_Separator},
		{GDK_KEY_KP_Subtract},
		{GDK_KEY_KP_Decimal},
		{GDK_KEY_KP_Divide},
		{GDK_KEY_F1, GDK_KEY_KP_F1}, {GDK_KEY_F2, GDK_KEY_KP_F2},
		{GDK_KEY_F3, GDK_KEY_KP_F3}, {GDK_KEY_F4, GDK_KEY_KP_F4},
		{GDK_KEY_F5}, {GDK_KEY_F6}, {GDK_KEY_F7}, {GDK_KEY_F8},
		{GDK_KEY_F9}, {GDK_KEY_F10}, {GDK_KEY_F11}, {GDK_KEY_F12},
		{GDK_KEY_Num_Lock},
		{GDK_KEY_Scroll_Lock},
		{GDK_KEY_Shift_L},
		{GDK_KEY_Shift_R},
		{GDK_KEY_Control_L},
		{GDK_KEY_Control_R},
		{GDK_KEY_Alt_L},
		{GDK_KEY_Alt_R}
	};

	ui::key get_mapped_key(guint ks) {
		static std::unordered_map<guint, ui::key> _mapping;
		static bool _mapped = false;

		if (!_mapped) {
			for (std::size_t i = 0; i < ui::total_num_keys; ++i) {
				if (
					i == static_cast<std::size_t>(ui::key::shift) ||
						i == static_cast<std::size_t>(ui::key::control) ||
						i == static_cast<std::size_t>(ui::key::alt)
					) {
					// shift, control, and alt are not mapped because they are only used when
					// testing if either key (left and right) is pressed
					continue;
				}
				for (std::size_t j = 0; j < max_keysym_mapping && keysym_mapping[i][j] != 0; ++j) {
					_mapping[keysym_mapping[i][j]] = static_cast<ui::key>(i);
				}
			}
			_mapped = true;
		}

		return _mapping[ks];
	}


	ui::mouse_button get_button_from_code(guint code) {
		switch (code) {
		case GDK_BUTTON_PRIMARY:
			return ui::mouse_button::primary;
		case GDK_BUTTON_SECONDARY:
			return ui::mouse_button::secondary;
		case GDK_BUTTON_MIDDLE:
			return ui::mouse_button::tertiary;
		default: // unrecognized button
			return ui::mouse_button::primary;
		}
	}
}
