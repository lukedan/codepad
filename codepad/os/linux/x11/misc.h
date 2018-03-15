#pragma once

/// \file
/// Miscellaneous functions of the linux platform when using X11.

#include <unordered_map>

#include <rapidjson/document.h> // included to prevent duplicate names
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "../../renderer.h"
#include "../../misc.h"

namespace codepad::os {
	namespace _details {
		/// Loads and initializes XLib.
		struct xlib_link {
			/// Atoms for events that are intercepted by the program.
			struct intercepted_atoms {
				/// The total number of events.
				constexpr static size_t size = 1;

				Atom delete_window = None; ///< The user clicks on the `close' button.

				/// Returns the list of intercepted events.
				Atom *get_list() {
					return &delete_window;
				}
			};

			/// Constructor. Initializes XLib, registers for events, and initializes XIM for user input.
			///
			/// \todo The list of events may not be enough for all desired behaviors.
			xlib_link() {
				display = XOpenDisplay(nullptr);
				assert_true_sys(display != nullptr, "unable to create display");

				attributes.bit_gravity = StaticGravity;
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
			/// No move construction.
			xlib_link(xlib_link&&) = delete;
			/// No copy construction.
			xlib_link(const xlib_link&) = delete;
			/// No move assignment.
			xlib_link &operator=(xlib_link&&) = delete;
			/// No copy assignment.
			xlib_link &operator=(const xlib_link&) = delete;
			/// Disposes all XLib related structures.
			~xlib_link() {
				XCloseIM(input_method);
				XFree(visual_info);
				XFreeColormap(display, attributes.colormap);
				XCloseDisplay(display);
			}

			XSetWindowAttributes attributes{}; ///< Attributes of all windows created.
			intercepted_atoms atoms; ///< Additional intercepted events.
			Display *display = nullptr; ///< The XLib display.
			XVisualInfo *visual_info = nullptr; ///< The display format of the windows.
			XIM input_method; ///< The input method.

			/// Returns the global \ref xlib_link object.
			static xlib_link &get();
		};
	}
	namespace input {
		namespace _details {
			/// A mapping from \ref key to X11 key codes.
			///
			/// \todo The list of keys is not yet complete, and the behavior may not be as intended.
			extern const KeySym key_id_mapping[total_num_keys];

			/// Obtains the \ref mouse_button corresponding to the given button code.
			///
			/// \param btn The button code.
			/// \param res Stores the return value.
			/// \return \p true if \p btn is a valid button code.
			inline bool get_mapped_button(unsigned int btn, mouse_button &res) {
				switch (btn) {
				case Button1:
					res = mouse_button::primary;
					return true;
				case Button2:
					res = mouse_button::tertiary;
					return true;
				case Button3:
					res = mouse_button::secondary;
					return true;
				default:
					return false;
				}
			}
			/// Returns the button code corresponding to the given \ref mouse_button.
			inline unsigned int get_mapped_button(mouse_button mb) {
				switch (mb) {
				case mouse_button::primary:
					return Button1;
				case mouse_button::tertiary:
					return Button2;
				case mouse_button::secondary:
					return Button3;
				}
			}
			/// Returns the button mask corresponding to the given \ref mouse_button.
			inline unsigned int get_mapped_button_mask(mouse_button mb) {
				switch (mb) {
				case mouse_button::primary:
					return Button1Mask;
				case mouse_button::tertiary:
					return Button2Mask;
				case mouse_button::secondary:
					return Button3Mask;
				}
				return 0;
			}
			/// Returns the button mask corresponding to the given button code.
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
			/// Tests if the mouse button corresponding to the given button mask is pressed.
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
			/// Tests if any of the given keys is pressed.
			inline bool is_key_down_any(std::initializer_list<key> codes) {
				Display *disp = os::_details::xlib_link::get().display;
				unsigned char res[32]; // currently pressed keys
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
			/// Given a \p KeySym, returns the corresponding \ref key. This function uses a
			/// \p std::unordered_map to map the values. Certain keys have more than one key
			/// codes that are added manually.
			///
			/// \todo Find a less hacky way to handle this mapping.
			key get_mapped_key(KeySym);
		}
		/// \todo The result may not be accurate since multiple keys may be mapped to the same \ref key.
		inline bool is_key_down(key k) {
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
}
