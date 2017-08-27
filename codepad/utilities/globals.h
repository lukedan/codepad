#pragma once

#include "misc.h"
#include "tasks.h"
#include "../os/renderer.h"
#include "../os/current.h"
#include "../ui/manager.h"
#include "../editors/docking.h"
#include "../editors/code/editor.h"

namespace codepad {
	template <typename ...Args> struct singleton_factory; // TODO thread safety?
	template <> struct singleton_factory<> {
	public:
		virtual ~singleton_factory() {
			for (auto i = _vars.rbegin(); i != _vars.rend(); ++i) {
				delete *i;
			}
		}
	protected:
		struct _variable_wrapper {
			virtual ~_variable_wrapper() {
			}
		};
		std::list<_variable_wrapper*> _vars;

		template <typename T, typename ...Args> T &_register(Args &&...args) {
			if (!std::is_same<logger, typename T::object_type>::value) {
				logger::get().log_info(CP_HERE,
					"initializing variable: ",
					typeid(typename T::object_type).name()
				);
			}
			T *var = new T(std::forward<Args>(args)...);
			_vars.push_back(var);
			return *var;
		}
	};
	template <typename T, typename ...Others> struct singleton_factory<T, Others...> :
		public singleton_factory<Others...> {
	public:
		template <typename U> U &get() {
			return _get_impl(_helper<U>());
		}
	protected:
		typedef singleton_factory<Others...> _direct_base;
		typedef singleton_factory<> _root_base;
	private:
		template <typename U> struct _helper {
		};

		T &_get_impl(_helper<T>) {
			if (!_var) {
				_var = &_root_base::_register<_t_wrapper>();
			}
			return _var->object;
		}
		template <typename U> U &_get_impl(_helper<U>) {
			return _direct_base::template get<U>();
		}

		struct _t_wrapper : public _root_base::_variable_wrapper {
			typedef T object_type;
			T object;
		};
		_t_wrapper *_var = nullptr;
	};

	struct globals {
	public:
		template <typename T> T &get() {
			return _vars.get<T>();
		}

		inline static globals &current() {
			return *_cur;
		}
	protected:
		struct _cur_setter {
			_cur_setter(globals &g) {
				assert_true_logical(_cur == nullptr, "globals already initialized");
				_cur = &g;
			}
			~_cur_setter() {
				_cur = nullptr;
			}
		};

		_cur_setter _csetter{*this};
		singleton_factory <
			logger,
			callback_buffer,
			async_task_pool,
			os::renderer_base::_default_renderer,
#ifdef CP_PLATFORM_WINDOWS
			os::freetype_font::_library,
			os::directwrite_font::_factory,
#endif
			ui::manager,
#ifdef CP_DETECT_LOGICAL_ERRORS
			ui::control_dispose_rec,
#endif
			ui::content_host::_default_font,
			editor::dock_manager,
			editor::code::editor::_used_font_family
		> _vars;

		static globals *_cur;
	};

	inline logger &logger::get() {
		return globals::current().get<logger>();
	}
	inline callback_buffer &callback_buffer::get() {
		return globals::current().get<callback_buffer>();
	}
	inline async_task_pool &async_task_pool::get() {
		return globals::current().get<async_task_pool>();
	}
	inline os::renderer_base::_default_renderer &os::renderer_base::_get_rend() {
		return globals::current().get<_default_renderer>();
	}
#ifdef CP_PLATFORM_WINDOWS
	inline os::freetype_font::_library &os::freetype_font::_get_library() {
		return globals::current().get<_library>();
	}
	inline os::directwrite_font::_factory &os::directwrite_font::_get_factory() {
		return globals::current().get<_factory>();
	}
#endif
	inline ui::manager &ui::manager::get() {
		return globals::current().get<manager>();
	}
#ifdef CP_DETECT_LOGICAL_ERRORS
	inline ui::control_dispose_rec &ui::control_dispose_rec::get() {
		return globals::current().get<control_dispose_rec>();
	}
#endif
	inline std::shared_ptr<const os::font> &ui::content_host::_get_deffnt() {
		return globals::current().get<_default_font>().font;
	}
	inline editor::dock_manager &editor::dock_manager::get() {
		return globals::current().get<dock_manager>();
	}
	inline ui::font_family &editor::code::editor::_get_font() {
		return globals::current().get<_used_font_family>().family;
	}
}
