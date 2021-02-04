// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Definition of certain global objects and functions,
/// and auxiliary structs to log their construction and destruction.

#include <stack>
#include <string>

#include "codepad/core/misc.h"
#include "codepad/core/plugins.h"
#include "codepad/core/settings.h"
#include "codepad/ui/commands.h"
#include "codepad/ui/renderer.h"
#include "codepad/ui/manager.h"
#include "codepad/ui/element_classes.h"

namespace codepad {
	// TODO probably put these into ui::manager as well
	ui::window *ui::mouse_position::_active_window = nullptr;
	// this is set to 1 so that no window thinks it has up-to-date mouse position initially
	std::size_t ui::mouse_position::_global_timestamp = 1;

	/*std::optional<document_formatting_cache::_in_effect_params> document_formatting_cache::_eff;*/

	void initialize(int argc, char **argv) {
		os::initialize(argc, argv);

		/*document_formatting_cache::enable();*/
	}


	/// Type names of initializing global objects are pushed onto this stack to make dependency clear.
	std::stack<std::u8string> _global_init_stk;
	std::u8string _cur_global_dispose; ///< Type name of the global object that's currently being destructed.

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
			if constexpr (!std::is_same_v<T, logger>) {
				logger::get().log_debug(CP_HERE) <<
					std::u8string((_global_init_stk.size() - 1) * 2, u8' ') <<
					"finish init: " << _global_init_stk.top();
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
				std::u8string tname = demangle(typeid(T).name());
				if constexpr (!std::is_same_v<T, logger>) { // logging is not performed for logger
					logger::get().log_debug(CP_HERE) <<
						std::u8string(_global_init_stk.size() * 2, u8' ') << "begin init: " << tname;
				}
				_global_init_stk.emplace(move(tname));
			}
			/// Destructor. Logs when the object has been destructed.
			~_init_marker() {
				if constexpr (!std::is_same_v<T, logger>) { // logging is not performed for logger
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
#ifdef CP_PLATFORM_UNIX
	namespace os {
#	ifndef CP_USE_GTK
		_details::xlib_link &_details::xlib_link::get() {
			static _global_wrapper<xlib_link> _v;
			return _v.object;
		}
#	endif
	}
#endif
}
