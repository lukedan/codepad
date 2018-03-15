#include "../../linux.h"

#include <vector>

#include "../../../core/misc.h"

using namespace std;

namespace codepad::os {
	namespace _details {
		void check_init_gtk() {
			static bool _init = false;

			if (!_init) {
				gtk_init(nullptr, nullptr);
				_init = true;
			}
		}
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
		}

		/// \todo Implement \ref is_key_down.
		bool is_key_down(key k) {
			// TODO
			return false;
		}
		bool is_mouse_button_down(mouse_button mb) {
			GdkDevice *keyboard = gdk_seat_get_pointer(
				gdk_display_get_default_seat(gdk_display_get_default())
			);
			GdkModifierType mod{};
			gdk_device_get_state(keyboard, gdk_get_default_root_window(), nullptr, &mod);
			return (mod & _details::get_modifier_bit_of_button(mb)) != 0;
		}
		vec2i get_mouse_position() {
			os::_details::check_init_gtk();
			gint x, y;
			gdk_device_get_position(gdk_seat_get_pointer(
				gdk_display_get_default_seat(gdk_display_get_default())
			), nullptr, &x, &y);
			return vec2i(x, y);
		}
	}

	const gchar *window::_window_pointer_key = "window_pointer";
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
	/// \todo Handle keyboard input.
	/// \todo Can sometimes receive events from already destroyed windows (and SIGSEGV).
	bool window::_idle() {
		if (gdk_events_pending()) {
			GdkEvent *ev = gdk_event_get();
			if (ev) {
				auto *wnd = static_cast<window*>(g_object_get_data(
					G_OBJECT(ev->any.window), _window_pointer_key
				));
				switch (ev->type) {
				case GDK_CONFIGURE:
					if (ev->configure.width != wnd->_width || ev->configure.height != wnd->_height) {
						wnd->_width = ev->configure.width;
						wnd->_height = ev->configure.height;
						wnd->_layout = rectd(0.0, wnd->_width, 0.0, wnd->_height);
						_invoke_event(wnd, &window::_on_size_changed, vec2i(wnd->_width, wnd->_height));
					}
					break;
				case GDK_DELETE:
					wnd->_on_close_request();
					break;
				case GDK_MOTION_NOTIFY:
					{
						if (!wnd->is_mouse_over()) {
							wnd->_on_mouse_enter();
						}
						_invoke_event(wnd, &window::_on_mouse_move, vec2d(ev->motion.x, ev->motion.y));
						// set cursor
						cursor c = wnd->get_current_display_cursor();
						if (c == cursor::not_specified) {
							c = cursor::normal;
						}
						gdk_window_set_cursor(
							ev->any.window, _details::cursor_set::get().cursors[static_cast<int>(c)]
						);
					}
					break;
				case GDK_LEAVE_NOTIFY:
					wnd->_on_mouse_leave();
					break;
				case GDK_BUTTON_PRESS:
					_invoke_event(
						wnd, &window::_on_mouse_down,
						_get_button_from_code(ev->button.button), vec2d(ev->button.x, ev->button.y)
					);
					break;
				case GDK_BUTTON_RELEASE:
					_invoke_event(
						wnd, &window::_on_mouse_up,
						_get_button_from_code(ev->button.button), vec2d(ev->button.x, ev->button.y)
					);
					break;
				case GDK_GRAB_BROKEN:
					wnd->_on_lost_window_capture();
					break;
				case GDK_SCROLL:
					// TODO scroll
					break;
				case GDK_FOCUS_CHANGE:
					if (ev->focus_change.in) {
						wnd->_on_got_window_focus();
					} else {
						wnd->_on_lost_window_focus();
					}
					break;
				default:
					logger::get().log_info(CP_HERE, "unhandled window message ", ev->type);
					break;
				}
				gdk_event_free(ev);
				return true;
			}
			return false;
		}
	}

	vector<filesystem::path> open_file_dialog(const window_base *parent, file_dialog_type type) {
		// TODO
		return {"/home/lukedan/.bashrc"};
	}

	texture load_image(renderer_base &renderer, const filesystem::path &path) {
		_details::check_init_gtk();
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
