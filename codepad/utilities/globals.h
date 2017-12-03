#pragma once

#include "misc.h"
#include "tasks.h"
#include "../os/renderer.h"
#include "../os/current.h"
#include "../ui/manager.h"
#include "../ui/theme_providers.h"
#include "../editors/docking.h"
#include "../editors/code/editor.h"

namespace codepad {
	template <typename ...Args> struct singleton_factory; // TODO thread safety?
	template <> struct singleton_factory<> {
	public:
		virtual ~singleton_factory() {
			for (auto i = _vars.rbegin(); i != _vars.rend(); ++i) {
				logger::get().log_info(CP_HERE, "disposing variable: ", (*i)->get_type_name());
				delete *i;
			}
		}
	protected:
		struct _variable_wrapper {
			virtual ~_variable_wrapper() = default;

			virtual std::string get_type_name() const = 0;
		};
		std::list<_variable_wrapper*> _vars;

		template <typename T, typename ...Args> T &_register(Args &&...args) {
			T *var = new T(std::forward<Args>(args)...);
			if (!std::is_same<logger, typename T::object_type>::value) {
				logger::get().log_info(CP_HERE,
					"initialized variable: ",
					demangle(typeid(typename T::object_type).name())
				);
			}
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
		using _direct_base = singleton_factory<Others...>;
		using _root_base = singleton_factory<>;
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
			using object_type = T;
			T object;

			std::string get_type_name() const override {
				return demangle(typeid(T).name());
			}
		};
		_t_wrapper *_var = nullptr;
	};

	// avoid pointers (including shared_ptr's)
	// or initialize them in the constructor
	// or use the pointed-to type first (tricky)
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
			explicit _cur_setter(globals &g) {
				assert_true_logical(_cur == nullptr, "globals already initialized");
				_cur = &g;
			}
			~_cur_setter() {
				_cur = nullptr;
			}
		};

		_cur_setter _csetter{*this};
		singleton_factory <
#ifdef CP_PLATFORM_WINDOWS
			os::wic_image_loader,
#endif
#ifdef CP_PLATFORM_UNIX
			os::_details::xlib_link,
			os::freetype_font::_font_config,
#endif
#ifdef CP_DETECT_LOGICAL_ERRORS
			ui::control_dispose_rec,
#endif
			logger,
			callback_buffer,
			async_task_pool,
			os::freetype_font_base::_library,
			os::renderer_base::_default_renderer,
			ui::manager,
			ui::content_host::_default_font, // TODO remove this
			ui::visual_manager::_registration,
			editor::dock_manager,
			editor::code::editor::_appearance_config // TODO remove this as well
		> _vars;

		static globals *_cur;
	};

#ifdef CP_PLATFORM_WINDOWS
	inline os::wic_image_loader &os::wic_image_loader::get() {
		return globals::current().get<wic_image_loader>();
	}
#endif
#ifdef CP_PLATFORM_UNIX
	inline os::_details::xlib_link &os::_details::xlib_link::get() {
		return globals::current().get<xlib_link>();
	}
	inline os::freetype_font::_font_config &os::freetype_font::_get_config() {
		return globals::current().get<_font_config>();
	}
#endif
#ifdef CP_DETECT_LOGICAL_ERRORS
	inline ui::control_dispose_rec &ui::control_dispose_rec::get() {
		return globals::current().get<control_dispose_rec>();
	}
#endif
	inline logger &logger::get() {
		return globals::current().get<logger>();
	}
	inline callback_buffer &callback_buffer::get() {
		return globals::current().get<callback_buffer>();
	}
	inline async_task_pool &async_task_pool::get() {
		return globals::current().get<async_task_pool>();
	}
	inline os::freetype_font_base::_library &os::freetype_font_base::_get_library() {
		return globals::current().get<_library>();
	}
	inline os::renderer_base::_default_renderer &os::renderer_base::_get_rend() {
		return globals::current().get<_default_renderer>();
	}
	inline ui::manager &ui::manager::get() {
		return globals::current().get<manager>();
	}
	inline std::shared_ptr<const os::font> &ui::content_host::_get_deffnt() {
		return globals::current().get<_default_font>().font;
	}
	inline ui::visual_manager::_registration &ui::visual_manager::_get_table() {
		return globals::current().get<_registration>();
	}
	inline editor::dock_manager &editor::dock_manager::get() {
		return globals::current().get<dock_manager>();
	}
	inline editor::code::editor::_appearance_config &editor::code::editor::_get_appearance() {
		return globals::current().get<_appearance_config>();
	}
}
