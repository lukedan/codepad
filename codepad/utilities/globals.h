#pragma once

#include <optional>

#include "misc.h"
#include "tasks.h"
#include "performance_monitor.h"
#include "../os/renderer.h"
#include "../os/current.h"
#include "../ui/manager.h"
#include "../ui/visual.h"
#include "../ui/commands.h"
#include "../ui/element_classes.h"
#include "../editors/docking.h"
#include "../editors/code/editor.h"
#include "../editors/code/context_manager.h"

namespace codepad {
	template <typename ...Args> struct singleton_factory { // TODO thread safety
	public:
		~singleton_factory() {
			for (; !_dispose_order.empty(); _dispose_order.pop_back()) {
				_dispose_order.back()();
			}
		}

		template <typename T> T &get() {
			std::optional<T> &ir = std::get<std::optional<T>>(_objs);
			if (!ir.has_value()) {
				ir.emplace();
				if (!std::is_same_v<logger, T>) {
					logger::get().log_info(CP_HERE, "initialized variable: ", demangle(typeid(T).name()));
				}
				_dispose_order.push_back([this]() {
					logger::get().log_info(CP_HERE, "disposing variable: ", demangle(typeid(T).name()));
					std::get<std::optional<T>>(_objs).reset();
					});
			}
			return *ir;
		}
	protected:
		std::vector<std::function<void()>> _dispose_order;
		std::tuple<std::optional<Args>...> _objs;
	};

	// ordering is important
	struct globals {
	public:
		globals() : _epoch(std::chrono::high_resolution_clock::now()) {
		}

		template <typename T> T &get() {
			return _vars.get<T>();
		}

		inline static globals &current() {
			return *_cur;
		}

		const std::chrono::time_point<std::chrono::high_resolution_clock> &get_app_epoch() const {
			return _epoch;
		}
		std::chrono::duration<double> get_uptime() const {
			return std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - _epoch);
		}
	protected:
		struct _cur_setter { // otherwise _cur will be set to null too early
			explicit _cur_setter(globals &g) {
				assert_true_logical(_cur == nullptr, "globals already initialized");
				_cur = &g;
			}
			~_cur_setter() {
				_cur = nullptr;
			}
		};

		std::chrono::time_point<std::chrono::high_resolution_clock> _epoch;
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
			performance_monitor,
			os::freetype_font_base::_library,
			os::renderer_base::_default_renderer,
			ui::manager,
			ui::content_host::_default_font, // TODO remove this
			ui::visual::_registry,
			ui::class_manager,
			ui::command_registry,
			editor::dock_manager,
			editor::code::editor::_appearance_config, // TODO remove this as well
			editor::code::context_manager
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
	inline performance_monitor &performance_monitor::get() {
		return globals::current().get<performance_monitor>();
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
	inline ui::visual::_registry &ui::visual::_registry::get() {
		return globals::current().get<_registry>();
	}
	inline ui::class_manager &ui::class_manager::get() {
		return globals::current().get<class_manager>();
	}
	inline ui::command_registry &ui::command_registry::get() {
		return globals::current().get<command_registry>();
	}
	inline editor::dock_manager &editor::dock_manager::get() {
		return globals::current().get<dock_manager>();
	}
	inline editor::code::editor::_appearance_config &editor::code::editor::_get_appearance() {
		return globals::current().get<_appearance_config>();
	}
	inline editor::code::context_manager &editor::code::context_manager::get() {
		return globals::current().get<context_manager>();
	}

	inline std::chrono::time_point<std::chrono::high_resolution_clock> get_app_epoch() {
		return globals::current().get_app_epoch();
	}
	inline std::chrono::duration<double> get_uptime() {
		return globals::current().get_uptime();
	}
}
