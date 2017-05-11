#pragma once

namespace codepad {
	namespace platform {
		namespace input {
			enum class mouse_button { left, middle, right };
			enum class key {
				physical_left_mouse,
				physical_right_mouse,
				middle_mouse,
				cancel,
				xbutton_1,
				xbutton_2,
				back,
				tab,
				clear,
				enter,
				shift,
				control,
				menu,
				pause,
				capital,
				escape,
				convert,
				nonconvert,
				space,
				prior,
				next,
				end,
				home,
				left,
				up,
				right,
				down,
				select,
				print,
				execute,
				snapshot,
				insert,
				del,
				help,
				left_win,
				right_win,
				apps,
				sleep,
				multiply,
				add,
				separator,
				subtract,
				decimal,
				divide,
				f1,
				f2,
				f3,
				f4,
				f5,
				f6,
				f7,
				f8,
				f9,
				f10,
				f11,
				f12,
				num_lock,
				scroll,
				left_shift,
				right_shift,
				left_control,
				right_control,
				left_menu,
				right_menu
			};
			bool is_key_down(key);
			bool is_mouse_button_swapped();
			bool is_mouse_button_down(mouse_button);

			vec2i get_mouse_position();
			void set_mouse_position(vec2i);
		}
	}
}
