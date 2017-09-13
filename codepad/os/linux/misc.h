#pragma once

#include <unordered_map>

#include <rapidjson/document.h> // included to prevent duplicate names
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "../renderer.h"
#include "../misc.h"

namespace codepad {
	namespace os {
		namespace _details {
			struct xlib_link {
				struct intercepted_atoms {
					constexpr static size_t size = 1;

					Atom delete_window = None;

					Atom *get_list() {
						return &delete_window;
					}
				};

				xlib_link() {
					display = XOpenDisplay(nullptr);
					assert_true_sys(display != nullptr, "unable to create display");

					attributes.bit_gravity = StaticGravity;
					// TODO maybe insufficient
					attributes.event_mask =
						ExposureMask |
						KeyPressMask | KeyReleaseMask |
						ButtonPressMask | ButtonReleaseMask |
						PointerMotionMask |
						ButtonMotionMask |
						LeaveWindowMask |
						FocusChangeMask |
						StructureNotifyMask;

					atoms.delete_window = XInternAtom(display, "WM_DELETE_WINDOW", false);

					input_method = XOpenIM(display, nullptr, nullptr, nullptr);
					assert_true_sys(input_method != None, "cannot open input method");
				}
				xlib_link(const xlib_link&) = delete;
				xlib_link(xlib_link&&) = delete;
				xlib_link &operator=(const xlib_link&) = delete;
				xlib_link &operator=(xlib_link&&) = delete;
				~xlib_link() {
					XCloseIM(input_method);
					XFree(visual_info);
					XFreeColormap(display, attributes.colormap);
					XCloseDisplay(display);
				}

				XSetWindowAttributes attributes{};
				intercepted_atoms atoms;
				Display *display = nullptr;
				XVisualInfo *visual_info = nullptr;
				XIM input_method;

				static xlib_link &get();
			};
		}
		namespace input {
			namespace _details {
				extern const KeySym key_id_mapping[total_num_keys];

				inline bool get_mapped_button(unsigned int btn, mouse_button &res) {
					switch (btn) {
					case Button1:
						res = mouse_button::left;
						return true;
					case Button2:
						res = mouse_button::middle;
						return true;
					case Button3:
						res = mouse_button::right;
						return true;
					default:
						return false;
					}
				}
				inline unsigned int get_mapped_button(mouse_button mb) {
					switch (mb) {
					case mouse_button::left:
						return Button1;
					case mouse_button::middle:
						return Button2;
					case mouse_button::right:
						return Button3;
					}
				}
				inline unsigned int get_mapped_button_mask(mouse_button mb) {
					switch (mb) {
					case mouse_button::left:
						return Button1Mask;
					case mouse_button::middle:
						return Button2Mask;
					case mouse_button::right:
						return Button3Mask;
					}
					return 0;
				}
				inline unsigned int get_mask_from_button(unsigned int btn) {
					switch (btn) {
					case Button1:
						return Button1Mask;
					case Button2:
						return Button2Mask;
					case Button3:
						return Button3Mask;
					case Button4:
						return Button4Mask;
					case Button5:
						return Button5Mask;
					default:
						return None;
					}
				}
				inline bool is_mouse_button_down_mask(unsigned int tmask) {
					auto &disp = os::_details::xlib_link::get();
					Window dw1, dw2;
					int di1, di2, di3, di4;
					unsigned int mask;
					XQueryPointer(
						disp.display, XDefaultRootWindow(disp.display),
						&dw1, &dw2, &di1, &di2, &di3, &di4, &mask
					);
					return (mask & tmask) != 0;
				}
				inline bool is_key_down_any(std::initializer_list<key> codes) {
					Display *disp = os::_details::xlib_link::get().display;
					unsigned char res[32];
					XQueryKeymap(disp, reinterpret_cast<char*>(res));
					for (auto i = codes.begin(); i != codes.end(); ++i) {
						KeyCode code = XKeysymToKeycode(disp, _details::key_id_mapping[static_cast<int>(*i)]);
						assert_true_sys(code != 0, "cannot get keycode");
						if ((res[code / 8] & (1 << (code % 8))) != 0) {
							return true;
						}
					}
					return false;
				}
				key get_mapped_key(KeySym);
			}
			inline bool is_key_down(key k) { // TODO maybe inaccurate
				switch (k) {
				case key::shift:
					return _details::is_key_down_any({key::left_shift, key::right_shift});
				case key::control:
					return _details::is_key_down_any({key::left_control, key::right_control});
				case key::alt:
					return _details::is_key_down_any({key::left_alt, key::right_alt});
				default:
					return _details::is_key_down_any({k});
				}
			}
			inline bool is_mouse_button_down(mouse_button mb) {
				return _details::is_mouse_button_down_mask(_details::get_mapped_button_mask(mb));
			}

			inline vec2i get_mouse_position() {
				auto &disp = os::_details::xlib_link::get();
				Window rootret, cret;
				int rxr, ryr, wxr, wyr;
				unsigned int msk;
				XQueryPointer(
					disp.display, XDefaultRootWindow(disp.display),
					&rootret, &cret, &rxr, &ryr, &wxr, &wyr, &msk
				);
				return vec2i(rxr, ryr);
			}
			inline void set_mouse_position(vec2i pos) {
				auto &disp = os::_details::xlib_link::get();
				XWarpPointer(disp.display, None, XDefaultRootWindow(disp.display), 0, 0, 0, 0, pos.x, pos.y);
			}
		}

		inline texture load_image(renderer_base&, const str_t&) {
			// TODO
			return texture();
		}
	}
}
