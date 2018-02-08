#pragma once

#include "../utilities/misc.h"

namespace codepad::os {
	class window_base;

	enum class cursor {
		normal,
		busy,
		crosshair,
		hand,
		help,
		text_beam,
		denied,
		arrow_all,
		arrow_northeast_southwest,
		arrow_north_south,
		arrow_northwest_southeast,
		arrow_east_west,

		invisible,
		not_specified
	};

	namespace input {
		enum class mouse_button {
			left, middle, right
		};
		enum class key {
			cancel,
			xbutton_1, xbutton_2,
			backspace,
			tab,
			clear,
			enter,
			shift, control, alt,
			pause,
			capital,
			escape,
			convert,
			nonconvert,
			space,
			page_up, page_down,
			end, home,
			left, up, right, down,
			select,
			print,
			execute,
			snapshot,
			insert,
			del,
			help,
			a, b, c, d, e, f, g, h, i, j, k, l, m,
			n, o, p, q, r, s, t, u, v, w, x, y, z,
			left_win, right_win,
			apps,
			sleep,
			multiply, add, separator, subtract, decimal, divide,
			f1, f2, f3, f4,
			f5, f6, f7, f8,
			f9, f10, f11, f12,
			num_lock,
			scroll,
			left_shift, right_shift,
			left_control, right_control,
			left_alt, right_alt,

			max_value
		};
		constexpr size_t total_num_keys = static_cast<size_t>(key::max_value);
		bool is_key_down(key);
		bool is_mouse_button_down(mouse_button);

		vec2i get_mouse_position();
		void set_mouse_position(vec2i);
	}

	enum class file_dialog_type {
		single_selection,
		multiple_selection
	};
	std::vector<std::filesystem::path> open_file_dialog(const window_base*, file_dialog_type);
}
