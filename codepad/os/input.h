#pragma once

namespace codepad {
	namespace os {
		namespace input {
			enum class mouse_button { left, middle, right };
			enum class key {
				physical_left_mouse,
				physical_right_mouse,
				middle_mouse,
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
				a, b, c, d, e, f, g, h, i, j, k, l, m,
				n, o, p, q, r, s, t, u, v, w, x, y, z,

				max_value
			};
			constexpr size_t total_num_keys = static_cast<size_t>(key::max_value);
			bool is_key_down(key);
			bool is_mouse_button_swapped();
			inline bool is_mouse_button_down(mouse_button mb) {
				switch (mb) {
				case mouse_button::left:
					if (is_mouse_button_swapped()) {
						return is_key_down(key::physical_right_mouse);
					}
					return is_key_down(key::physical_left_mouse);
				case mouse_button::right:
					if (is_mouse_button_swapped()) {
						return is_key_down(key::physical_left_mouse);
					}
					return is_key_down(key::physical_right_mouse);
				case mouse_button::middle:
					return is_key_down(key::middle_mouse);
				}
				assert(false);
				return false;
			}

			vec2i get_mouse_position();
			void set_mouse_position(vec2i);
		}
	}
}
