// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Definition of certain global objects and functions,
/// and auxiliary structs to log their construction and destruction.

#include <stack>
#include <string>

#include "misc.h"
#include "plugins.h"
#include "settings.h"
#include "../os/current/all.h"
#include "../ui/commands.h"
#include "../ui/common_elements.h"
#include "../ui/renderer.h"
#include "../ui/manager.h"
#include "../ui/element_classes.h"
#include "../ui/native_commands.h"
#include "../editors/buffer_manager.h"
#include "../editors/code/components.h"
#include "../editors/code/contents_region.h"
#include "../editors/code/view_caching.h"
#include "../editors/binary/contents_region.h"

using namespace std;

using namespace codepad::os;
using namespace codepad::ui;
using namespace codepad::editors;
using namespace codepad::editors::code;

namespace codepad {
	double minimap::_target_height = 2.0;

	std::unique_ptr<logger> logger::_current;

	// TODO probably put these into ui::manager as well
	window_base *mouse_position::_active_window = nullptr;
	// this is set to 1 so that no window thinks it has up-to-date mouse position initially
	std::size_t mouse_position::_global_timestamp = 1;

	/*std::optional<document_formatting_cache::_in_effect_params> document_formatting_cache::_eff;*/

	void initialize(int argc, char **argv) {
		os::initialize(argc, argv);

		/*document_formatting_cache::enable();*/
	}


	/// Type names of initializing global objects are pushed onto this stack to make dependency clear.
	stack<string> _global_init_stk;
	string _cur_global_dispose; ///< Type name of the global object that's currently being destructed.

	/// Wrapper struct for a global variable.
	/// Logs the beginning and ending of the creation and destruction of the underlying object,
	///
	/// \tparam T The type of the global variable.
	template <typename T> struct _global_wrapper {
	public:
		/// Constructor. Logs the object's construction.
		/// The beginning of the object's construction is logged by \ref _marker.
		/// This function then logs the ending, and pops the type name from \ref _global_init_stk.
		///
		/// \param args Arguments that are forwarded to the construction of the underlying object.
		template <typename ...Args> explicit _global_wrapper(Args &&...args) : object(forward<Args>(args)...) {
			// logging is not performed for logger since it may lead to recursive initialization
			if constexpr (!is_same_v<T, logger>) {
				logger::get().log_debug(CP_HERE) <<
					string((_global_init_stk.size() - 1) * 2, ' ') << "finish init: " << _global_init_stk.top();
			}
			_global_init_stk.pop();
		}
		/// Desturctor. Logs the object's destruction.
		~_global_wrapper() {
			assert_true_logical(_cur_global_dispose.length() == 0, "nested disposal of global objects");
			_cur_global_dispose = demangle(typeid(T).name());
		}

	protected:
		/// Helper struct used to log the beginning of the object's construction and the object's destruction.
		struct _init_marker {
			/// Constructor. Logs the beginning of the object's construction and
			/// pushes the type name onto \ref _global_init_stk.
			_init_marker() {
				string tname = demangle(typeid(T).name());
				if constexpr (!is_same_v<T, logger>) { // logging is not performed for logger
					logger::get().log_debug(CP_HERE) <<
						string(_global_init_stk.size() * 2, ' ') << "begin init: " << tname;
				}
				_global_init_stk.emplace(move(tname));
			}
			/// Destructor. Logs when the object has been destructed.
			~_init_marker() {
				if constexpr (!is_same_v<T, logger>) { // logging is not performed for logger
					logger::get().log_debug(CP_HERE) << "disposed: " << _cur_global_dispose;
				}
				_cur_global_dispose.clear();
			}
		};

		/// Helper object. Put before \ref object so that this is initialized before,
		/// and disposed after, \ref object.
		_init_marker _marker;
	public:
		T object; ///< The actual global object.
	};


	// all singleton getters
#ifdef CP_ENABLE_PLUGINS
	plugin_manager &plugin_manager::get() {
		static _global_wrapper<plugin_manager> _v;
		return _v.object;
	}
#endif


	namespace os {
#ifdef CP_PLATFORM_WINDOWS
		namespace _details {
			wic_image_loader &wic_image_loader::get() {
				static _global_wrapper<wic_image_loader> _v;
				return _v.object;
			}
		}


		window::_wndclass &window::_wndclass::get() {
			static _global_wrapper<_wndclass> _v;
			return _v.object;
		}

		window::_ime &window::_ime::get() {
			static _global_wrapper<_ime> _v;
			return _v.object;
		}


#endif
#ifdef CP_PLATFORM_UNIX
#	ifdef CP_USE_GTK
		namespace _details {
			cursor_set &cursor_set::get() {
				static _global_wrapper<cursor_set> _v;
				return _v.object;
			}
		}
#	else
		_details::xlib_link &_details::xlib_link::get() {
			static _global_wrapper<xlib_link> _v;
			return _v.object;
		}
#	endif
#endif
	}
	namespace editors {
		buffer_manager &buffer_manager::get() {
			static _global_wrapper<buffer_manager> _v;
			return _v.object;
		}


		namespace code {
			encoding_manager &encoding_manager::get() {
				static _global_wrapper<encoding_manager> _v;
				return _v.object;
			}


			interaction_mode_registry<caret_set> &contents_region::get_interaction_mode_registry() {
				static _global_wrapper<interaction_mode_registry<caret_set>> _v;
				static bool _initialized = false;

				if (!_initialized) {
					_v.object.mapping.emplace(
						u8"prepare_drag", []() {
							return std::make_unique<
								interaction_modes::mouse_prepare_drag_mode_activator<caret_set>
							>();
						}
					);
					_v.object.mapping.emplace(
						u8"single_selection", []() {
							return std::make_unique<
								interaction_modes::mouse_single_selection_mode_activator<caret_set>
							>();
						}
					);
					_initialized = true;
				}

				return _v.object;
			}
		}


		namespace binary {
			interaction_mode_registry<caret_set> &contents_region::get_interaction_mode_registry() {
				static _global_wrapper<interaction_mode_registry<caret_set>> _v;
				static bool _initialized = false;

				if (!_initialized) {
					_v.object.mapping.emplace(
						u8"prepare_drag", []() {
							return std::make_unique<
								interaction_modes::mouse_prepare_drag_mode_activator<caret_set>
							>();
						}
					);
					_v.object.mapping.emplace(
						u8"single_selection", []() {
							return std::make_unique<
								interaction_modes::mouse_single_selection_mode_activator<caret_set>
							>();
						}
					);
					_initialized = true;
				}

				return _v.object;
			}
		}
	}
}
