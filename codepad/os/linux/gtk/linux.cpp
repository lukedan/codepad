#include "../../linux.h"

#include <vector>

#include "../../../core/misc.h"

using namespace std;

namespace codepad::os {
	void initialize(int argc, char **argv) {
		gtk_init(&argc, &argv);
	}

	namespace _details {
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
	}

	namespace input {
		namespace _details {
			inline GdkModifierType get_modifier_bit_of_button(mouse_button btn) {
				switch (btn) {
				case mouse_button::primary:
					return GDK_BUTTON1_MASK;
				case mouse_button::secondary:
					return GDK_BUTTON2_MASK;
				case mouse_button::tertiary:
					return GDK_BUTTON3_MASK;
				}
				assert_true_logical(false, "invalid mouse button");
				return GDK_BUTTON1_MASK;
			}

			constexpr static size_t max_keysym_mapping = 4;
			/// \todo Complete the list of KeySyms.
			const guint keysym_mapping[total_num_keys][max_keysym_mapping] = {
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
			key get_mapped_key(guint ks) {
				static std::unordered_map<guint, key> _mapping;
				static bool _mapped = false;

				if (!_mapped) {
					for (size_t i = 0; i < total_num_keys; ++i) {
						if (
							i == static_cast<size_t>(key::shift) ||
								i == static_cast<size_t>(key::control) ||
								i == static_cast<size_t>(key::alt)
							) {
							// shift, control, and alt are not mapped because they are only used when
							// testing if either key (left and right) is pressed
							continue;
						}
						for (size_t j = 0; j < max_keysym_mapping && keysym_mapping[i][j] != 0; ++j) {
							_mapping[keysym_mapping[i][j]] = static_cast<key>(i);
						}
					}
					_mapped = true;
				}

				return _mapping[ks];
			}
		}

		bool is_mouse_button_down(mouse_button mb) {
			GdkDevice *pointer = gdk_seat_get_pointer(
				gdk_display_get_default_seat(gdk_display_get_default())
			);
			GdkModifierType mod{};
			gdk_device_get_state(pointer, gdk_get_default_root_window(), nullptr, &mod);
			return (mod & _details::get_modifier_bit_of_button(mb)) != 0;
		}
		vec2i get_mouse_position() {
			gint x, y;
			gdk_device_get_position(gdk_seat_get_pointer(
				gdk_display_get_default_seat(gdk_display_get_default())
			), nullptr, &x, &y);
			return vec2i(x, y);
		}
	}

	template <typename W, typename T, typename ...Args> inline void _invoke_event(
		window *w, void (W::*func)(T&), Args &&...args
	) {
		T info(std::forward<Args>(args)...);
		(w->*func)(info);
	};

	inline input::mouse_button _get_button_from_code(guint code) {
		switch (code) {
		case GDK_BUTTON_PRIMARY:
			return input::mouse_button::primary;
		case GDK_BUTTON_SECONDARY:
			return input::mouse_button::secondary;
		case GDK_BUTTON_MIDDLE:
			return input::mouse_button::tertiary;
		default: // unrecognized button
			return input::mouse_button::primary;
		}
	}

	inline input::key _get_key_of_event(GdkEvent *event) {
		return input::_details::get_mapped_key(event->key.keyval);
	}
	template <typename Ev> inline modifier_keys _get_modifiers(const Ev &event) {
		modifier_keys result = modifier_keys::none;
		if (event.state & GDK_CONTROL_MASK) {
			set_bits(result, modifier_keys::control);
		}
		if (event.state & GDK_SHIFT_MASK) {
			set_bits(result, modifier_keys::shift);
		}
		if (event.state & GDK_MOD1_MASK) { // normally alt
			set_bits(result, modifier_keys::alt);
		}
		if (event.state & GDK_HYPER_MASK) { // whatever
			set_bits(result, modifier_keys::super);
		}
		return result;
	}

	gboolean window::_on_delete_event(GtkWidget*, GdkEvent*, window *wnd) {
		wnd->_on_close_request();
		return true;
	}
	gboolean window::_on_motion_notify_event(GtkWidget*, GdkEvent *ev, window *wnd) {
		if (!wnd->is_mouse_over()) {
			wnd->_on_mouse_enter();
		}
		_form_onevent<ui::mouse_move_info>(
			*wnd, &window::_on_mouse_move, vec2d(ev->motion.x, ev->motion.y)
		);
		// update cursor
		cursor c = wnd->get_current_display_cursor();
		if (c == cursor::not_specified) {
			c = cursor::normal;
		}
		gdk_window_set_cursor(
			ev->any.window, _details::cursor_set::get().cursors[static_cast<int>(c)]
		);
		return true;
	}
	gboolean window::_on_button_press_event(GtkWidget*, GdkEvent *ev, window *wnd) {
		logger::get().log_verbose(CP_HERE, "mouse down");
		if (ev->button.type == GDK_BUTTON_PRESS) {
			_form_onevent<ui::mouse_button_info>(
				*wnd, &window::_on_mouse_down,
				_get_button_from_code(ev->button.button), _get_modifiers(ev->button), vec2d(ev->button.x, ev->button.y)
			);
		}
		// returning false here will cause this handler to be executed twice
		return true;
	}
	gboolean window::_on_button_release_event(GtkWidget*, GdkEvent *ev, window *wnd) {
		logger::get().log_verbose(CP_HERE, "mouse up");
		_form_onevent<ui::mouse_button_info>(
			*wnd, &window::_on_mouse_up,
			_get_button_from_code(ev->button.button), _get_modifiers(ev->button), vec2d(ev->button.x, ev->button.y)
		);
		return true;
	}
	gboolean window::_on_key_press_event(GtkWidget*, GdkEvent *event, window *wnd) {
		input::key k = _get_key_of_event(event);
		if (!wnd->hotkey_manager.on_key_down(key_gesture(k, _get_modifiers(event->key)))) {
			if (!gtk_im_context_filter_keypress(wnd->_imctx, &event->key)) {
				_form_onevent<ui::key_info>(*wnd, &window::_on_key_down, k);
			}
		}
		return true;
	}
	gboolean window::_on_key_release_event(GtkWidget*, GdkEvent *event, window *wnd) {
		input::key k = _get_key_of_event(event);
		if (!gtk_im_context_filter_keypress(wnd->_imctx, &event->key)) {
			_form_onevent<ui::key_info>(*wnd, &window::_on_key_up, k);
		}
		return true;
	}

	vector<filesystem::path> open_file_dialog(const window_base *parent, file_dialog_type type) {
#ifdef CP_DETECT_LOGICAL_ERRORS
		auto wnd = dynamic_cast<const window*>(parent);
		assert_true_logical(wnd != nullptr, "invalid window type");
#else
		auto wnd = static_cast<const window*>(parent);
#endif
		GtkWidget *dialog = gtk_file_chooser_dialog_new(
			nullptr, parent ? GTK_WINDOW(wnd->get_native_handle()) : nullptr, GTK_FILE_CHOOSER_ACTION_OPEN,
			"_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL
		);
		gtk_file_chooser_set_select_multiple(
			GTK_FILE_CHOOSER(dialog), type == file_dialog_type::multiple_selection
		);
		gint res = gtk_dialog_run(GTK_DIALOG(dialog));
		vector<filesystem::path> paths;
		if (res == GTK_RESPONSE_ACCEPT) {
			GSList *list = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
			for (GSList *iter = list; iter; iter = iter->next) {
				paths.emplace_back(reinterpret_cast<gchar*>(iter->data));
				g_free(iter->data);
			}
			g_slist_free(list);
		}
		gtk_widget_destroy(dialog);
		return paths;
	}

	texture load_image(renderer_base &renderer, const filesystem::path &path) {
		GError *err = nullptr;
		GdkPixbuf *buf = gdk_pixbuf_new_from_file(path.c_str(), &err);
		if (err) {
			logger::get().log_error(CP_HERE, "gdk error ", err->message);
			assert_true_sys(false, "cannot load image");
		}
		assert_true_usage(gdk_pixbuf_get_bits_per_sample(buf) == 8, "invalid bits per sample");
		auto
			width = static_cast<size_t>(gdk_pixbuf_get_width(buf)),
			height = static_cast<size_t>(gdk_pixbuf_get_height(buf));
		void *buffer = std::malloc(4 * width * height);
		auto *target = static_cast<unsigned char*>(buffer);
		auto has_alpha = static_cast<bool>(gdk_pixbuf_get_has_alpha(buf));
		const guchar *src = gdk_pixbuf_get_pixels(buf);
		int stride = gdk_pixbuf_get_rowstride(buf);
		for (size_t y = 0; y < height; ++y, src += stride) {
			const guchar *source = src;
			for (size_t x = 0; x < width; ++x, target += 4) {
				target[0] = source[0];
				target[1] = source[1];
				target[2] = source[2];
				if (has_alpha) {
					target[3] = source[3];
					source += 4;
				} else {
					target[3] = 255;
					source += 3;
				}
			}
		}
		texture tex = renderer.new_texture(width, height, buffer);
		std::free(buffer);
		g_object_unref(buf);
		return tex;
	}
}
