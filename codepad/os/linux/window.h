#pragma once

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "../window.h"

namespace codepad {
	namespace os {
		class window : public window_base {
			friend struct codepad::globals;
			friend class opengl_renderer;
			friend class ui::element;
		public:
			void set_caption(const str_t &cap) override {
				XStoreName(
					_details::xlib_link::get().display, _win,
					reinterpret_cast<const char*>(convert_to_utf8(cap).c_str())
				);
			}
			vec2i get_position() const override {
				Window dumw1;
				int x, y;
				XTranslateCoordinates(_details::xlib_link::get().display, _win, _get_root(), 0, 0, &x, &y, &dumw1);
				return vec2i(x, y);
			}
			void set_position(vec2i pos) override { // TODO not accurate
				Display *dp = _details::xlib_link::get().display;
				Window dumw1;
				int x, y;
				unsigned int dumu1, dumu2, dumu3, dumu4;
				XGetGeometry(dp, _win, &dumw1, &x, &y, &dumu1, &dumu2, &dumu3, &dumu4);
				XMoveWindow(dp, _win, pos.x - x, pos.y - y);
			}
			vec2i get_client_size() const override {
				Window dumw1;
				int dumi1, dumi2;
				unsigned int w, h, dumu1, dumu2;
				XGetGeometry(
					_details::xlib_link::get().display, _win,
					&dumw1, &dumi1, &dumi2, &w, &h, &dumu1, &dumu2
				);
				return vec2i(static_cast<int>(w), static_cast<int>(h));
			}
			void set_client_size(vec2i sz) override {
				XResizeWindow(
					_details::xlib_link::get().display, _win,
					static_cast<unsigned int>(sz.x), static_cast<unsigned int>(sz.y)
				);
			}

			void activate() override {
				// TODO
			}
			void prompt_ready() override {
				// TODO
			}

			void set_display_maximize_button(bool) override {
				// TODO
			}
			void set_display_minimize_button(bool) override {
				// TODO
			}
			void set_display_caption_bar(bool) override {
				// TODO
			}
			void set_display_border(bool) override {
				// TODO
			}
			void set_sizable(bool) override {
				// TODO
			}

			bool hit_test_full_client(vec2i) const override {
				// TODO
				return false;
			}

			vec2i screen_to_client(vec2i pos) const override {
				Window dumw1;
				int x, y;
				XTranslateCoordinates(
					_details::xlib_link::get().display, _get_root(), _win,
					pos.x, pos.y, &x, &y, &dumw1
				);
				return vec2i(x, y);
			}
			vec2i client_to_screen(vec2i pos) const override {
				Window dumw1;
				int x, y;
				XTranslateCoordinates(
					_details::xlib_link::get().display, _win, _get_root(),
					pos.x, pos.y, &x, &y, &dumw1
				);
				return vec2i(x, y);
			}

			void set_mouse_capture(ui::element &elem) override {
				window_base::set_mouse_capture(elem);
				// TODO
			}
			void release_mouse_capture() {
				window_base::release_mouse_capture();
				// TODO
			}

			inline static str_t get_default_class() {
				return U"window";
			}
		protected:
			window() {
				_details::xlib_link &disp = _details::xlib_link::get();
				_win = XCreateWindow(
					disp.display, RootWindow(disp.display, disp.visual_info->screen),
					0, 0, static_cast<unsigned int>(_width), static_cast<unsigned int>(_height), 0,
					disp.visual_info->depth, InputOutput, disp.visual_info->visual,
					CWEventMask | CWColormap | CWBorderPixel | CWBackPixel | CWBitGravity,
					&disp.attributes
				);
				_layout = rectd(0.0, _width, 0.0, _height);
				assert_true_sys(XSetWMProtocols(
					disp.display, _win, disp.atoms.get_list(),
					static_cast<int>(_details::xlib_link::intercepted_atoms::size)
				) != 0, "cannot set protocols");
				_xic = XCreateIC(
					disp.input_method,
					XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
					XNClientWindow, _win,
					XNFocusWindow, _win,
					nullptr
				);
				XMapWindow(_details::xlib_link::get().display, _win);
			}

			void _set_cursor(ui::cursor);
			Window _get_root() const {
				return XDefaultRootWindow(_details::xlib_link::get().display);
			}

			template <typename T, typename P, typename ...Args> void _onevent(void (P::*Func)(T&), Args &&...args) {
				T info(std::forward<Args>(args)...);
				(this->*Func)(info);
			}
			bool _idle() {
				_details::xlib_link &disp = _details::xlib_link::get();
				XEvent event;
				if (XCheckWindowEvent(disp.display, _win, disp.attributes.event_mask, &event)) {
					switch (event.type) {
					// TODO more messages
					case ConfigureNotify:
						if (event.xconfigure.width != _width || event.xconfigure.height != _height) {
							_width = event.xconfigure.width;
							_height = event.xconfigure.height;
							_layout = rectd(0.0, _width, 0.0, _height);
							_onevent(&window::_on_size_changed, vec2i(_width, _height));
						}
						break;
					case MotionNotify:
						{
							if (!is_mouse_over()) {
								_on_mouse_enter();
							}
							_onevent(&window::_on_mouse_move, vec2d(event.xmotion.x, event.xmotion.y));
							ui::cursor c = get_current_display_cursor();
							if (c == ui::cursor::not_specified) {
								c = ui::cursor::normal;
							}
							if (c != _oldc) {
								if (c == ui::cursor::invisible) {
									// TODO
								} else {
									_set_cursor(c);
								}
							}
						}
						break;
					case LeaveNotify:
						_on_mouse_leave();
						break;
					case ButtonPress:
						{
							input::mouse_button mb;
							if (input::_details::get_mapped_button(event.xbutton.button, mb)) {
								_onevent(&window::_on_mouse_down, mb, vec2d(event.xbutton.x, event.xbutton.y));
							} else {
								switch (event.xbutton.button) {
								case Button4:
									_onevent(&window::_on_mouse_scroll, 1.0, vec2d(event.xbutton.x, event.xbutton.y));
									break;
								case Button5:
									_onevent(&window::_on_mouse_scroll, -1.0, vec2d(event.xbutton.x, event.xbutton.y));
									break;
								default:
									logger::get().log_warning(CP_HERE,
										"unrecognized mouse button: ", event.xbutton.button
									);
									break;
								}
							}
						}
						break;
					case ButtonRelease:
						{
							input::mouse_button mb;
							if (input::_details::get_mapped_button(event.xbutton.button, mb)) {
								_onevent(&window::_on_mouse_up, mb, vec2d(event.xbutton.x, event.xbutton.y));
							}
						}
						break;
					case KeyPress:
						{
							constexpr size_t im_buffer_size = 32;
							unsigned char buffer[im_buffer_size]{};
							Status stat;
							KeySym ksym;
							int sz = Xutf8LookupString(_xic, &event.xkey, reinterpret_cast<char*>(buffer), im_buffer_size, &ksym, &stat);
							input::key k = input::_details::get_mapped_key(ksym);
							if (!hotkey_manager.on_key_down(k)) {
								// TODO key_down event maybe incorrect
								_onevent(&window::_on_key_down, k);
								if (stat == XBufferOverflow) {
									unsigned char *lbuf = static_cast<unsigned char*>(std::malloc(static_cast<size_t>(sz)));
									Xutf8LookupString(_xic, &event.xkey, reinterpret_cast<char*>(lbuf), sz, &ksym, &stat);
									_onevent(&window::_on_keyboard_text, convert_to_utf32(lbuf));
									std::free(lbuf);
								} else if (stat != XLookupKeySym) {
									if (ksym != XK_BackSpace && ksym != XK_Delete) {
										_onevent(&window::_on_keyboard_text, convert_to_utf32(buffer));
									}
								}
							}
						}
						break;
					case KeyRelease:
						_onevent(&window::_on_key_up, input::_details::get_mapped_key(XLookupKeysym(&event.xkey, 0)));
						// TODO key_up event incorrect
						break;
					case FocusIn:
						XSetICFocus(_xic);
						_on_got_window_focus();
						break;
					case FocusOut:
						XUnsetICFocus(_xic);
						_on_lost_window_focus();
						break;
					case Expose:
						invalidate_visual();
						break;
					default:
						logger::get().log_warning(CP_HERE, "unrecognized message: ", event.type);
						break;
					}
					return true;
				}
				if (XCheckTypedWindowEvent(disp.display, _win, ClientMessage, &event)) {
					Atom atm = *reinterpret_cast<Atom*>(&event.xclient.data.b);
					if (atm == disp.atoms.delete_window) {
						_on_close_request();
					}
					return true;
				}
				return false;
			}
			void _on_update() override {
				while (_idle()) {
				}
				_update_drag();
				ui::manager::get().schedule_update(*this);
			}
			void _on_prerender() override {
				window_base::_on_prerender();
			}

			void _initialize() override {
				window_base::_initialize();
				ui::manager::get().schedule_update(*this);
			}
			void _dispose() override {
				Display *disp = _details::xlib_link::get().display;
				XDestroyIC(_xic);
				XDestroyWindow(disp, _win);
				window_base::_dispose();
			}

			Window _win = None;
			XIC _xic = None;
			int _width = 800, _height = 600;
			ui::cursor _oldc = ui::cursor::normal;
		};
	}
}
