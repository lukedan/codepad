#include "../linux.h"

#include <X11/cursorfont.h>

namespace codepad {
	namespace os {
		namespace input {
			namespace _details {
				const KeySym key_id_mapping[total_num_keys] = {
					XK_Cancel,
					0, 0,//xbutton_1, xbutton_2,
					XK_BackSpace,
					XK_Tab,
					XK_Clear,
					XK_Return,
					0, 0, 0,//shift, control, alt,
					XK_Pause,
					XK_Caps_Lock,
					XK_Escape,
					0,//convert,
					0,//nonconvert,
					XK_space,
					XK_Page_Up, XK_Page_Down,
					XK_End, XK_Home,
					XK_Left, XK_Up, XK_Right, XK_Down,
					XK_Select,
					XK_Print,
					XK_Execute,
					0,//snapshot,
					XK_Insert,
					XK_Delete,
					XK_Help,
					XK_a, XK_b, XK_c, XK_d, XK_e, XK_f, XK_g, XK_h, XK_i, XK_j, XK_k, XK_l, XK_m,
					XK_n, XK_o, XK_p, XK_q, XK_r, XK_s, XK_t, XK_u, XK_v, XK_w, XK_x, XK_y, XK_z,
					0, 0,//left_win, right_win,
					0,//apps,
					0,//sleep,
					XK_asterisk, XK_plus, XK_KP_Separator, XK_hyphen, XK_period, XK_slash,
					XK_F1, XK_F2, XK_F3, XK_F4,
					XK_F5, XK_F6, XK_F7, XK_F8,
					XK_F9, XK_F10, XK_F11, XK_F12,
					XK_Num_Lock,
					0,//scroll,
					XK_Shift_L, XK_Shift_R,
					XK_Control_L, XK_Control_R,
					XK_Alt_L, XK_Alt_R
				};
				key get_mapped_key(KeySym ks) {
					static std::unordered_map<KeySym, key> _mapping;
					static bool _mapped = false;

					if (!_mapped) {
						for (size_t i = 0; i < total_num_keys; ++i) {
							_mapping[key_id_mapping[i]] = static_cast<key>(i);
						}
						// HACK linux keycodes & keysyms
						_mapping[XK_KP_Space] = key::space;
						_mapping[XK_KP_Tab] = key::tab;
						_mapping[XK_KP_Enter] = key::enter;
						_mapping[XK_KP_Home] = key::home;
						_mapping[XK_KP_Left] = key::left;
						_mapping[XK_KP_Up] = key::up;
						_mapping[XK_KP_Right] = key::right;
						_mapping[XK_KP_Down] = key::down;
						_mapping[XK_KP_Page_Up] = key::page_up;
						_mapping[XK_KP_Page_Down] = key::page_down;
						_mapping[XK_KP_End] = key::end;
						_mapping[XK_KP_Begin] = key::home;
						_mapping[XK_KP_Insert] = key::insert;
						_mapping[XK_KP_Delete] = key::del;
						_mapping[XK_KP_Multiply] = key::multiply;
						_mapping[XK_KP_Add] = key::add;
						_mapping[XK_KP_Subtract] = key::subtract;
						_mapping[XK_KP_Divide] = key::divide;
						_mapping[XK_KP_Decimal] = key::decimal;
						_mapped = true;
					}

					return _mapping[ks];
				}
			}
		}

		const int opengl_renderer::_fbattribs[15] = {
			GLX_X_RENDERABLE, True,
			GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
			GLX_DOUBLEBUFFER, True,
			GLX_RED_SIZE, 8,
			GLX_GREEN_SIZE, 8,
			GLX_BLUE_SIZE, 8,
			GLX_ALPHA_SIZE, 8,
			None
		};

		const unsigned int _cursor_id_mapping[] = {
			XC_arrow,
			XC_watch,
			XC_tcross,
			XC_hand1,
			XC_question_arrow,
			XC_xterm,
			XC_X_cursor,
			XC_sizing, // or XC_fleur?
			XC_ll_angle,
			XC_sb_v_double_arrow,
			XC_lr_angle,
			XC_sb_h_double_arrow
		};
		void window::_set_cursor(ui::cursor c) {
			_oldc = c;
			auto &disp = _details::xlib_link::get();
			Cursor cc = XCreateFontCursor(disp.display, _cursor_id_mapping[static_cast<unsigned>(c)]);
			XDefineCursor(disp.display, _win, cc);
			XFreeCursor(disp.display, cc);
		}
	}
}
