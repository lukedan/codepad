#pragma once

#include <chrono>
#include <functional>

#include "renderer.h"
#include "../ui/panel.h"
#include "../ui/window_hotkey_manager.h"
#include "../utilities/event.h"
#include "../utilities/textconfig.h"

namespace codepad {
	namespace os {
		struct size_changed_info {
			explicit size_changed_info(vec2i v) : new_size(v) {
			}
			const vec2i new_size;
		};
		class window_base : public ui::panel {
			friend class ui::manager;
			friend class ui::element_collection;
		public:
			virtual void set_caption(const str_t&) = 0;
			virtual vec2i get_position() const = 0;
			virtual void set_position(vec2i) = 0;
			virtual vec2i get_client_size() const = 0;
			virtual void set_client_size(vec2i) = 0;

			virtual void activate() = 0;
			virtual void prompt_ready() = 0;

			virtual void set_display_maximize_button(bool) = 0;
			virtual void set_display_minimize_button(bool) = 0;
			virtual void set_display_caption_bar(bool) = 0;
			virtual void set_display_border(bool) = 0;
			virtual void set_sizable(bool) = 0;

			virtual bool hit_test_full_client(vec2i) const = 0;

			virtual vec2i screen_to_client(vec2i) const = 0;
			virtual vec2i client_to_screen(vec2i) const = 0;

			virtual void set_mouse_capture(ui::element &elem) {
				logger::get().log_verbose(
					CP_HERE, "set mouse capture 0x", &elem,
					" <", demangle(typeid(elem).name()), ">"
				);
				assert_true_usage(_capture == nullptr, "mouse already captured");
				_capture = &elem;
			}
			virtual ui::element *get_mouse_capture() const {
				return _capture;
			}
			virtual void release_mouse_capture() {
				logger::get().log_verbose(CP_HERE, "release mouse capture");
				assert_true_usage(_capture != nullptr, "mouse not captured");
				_capture = nullptr;
			}

			virtual void start_drag(std::function<bool()> dst = []() {
				return input::is_mouse_button_down(input::mouse_button::left);
				}) {
				assert_true_usage(!_drag, "the window is already being dragged");
				_dragcontinue = dst;
				_drag = true;
				_doffset = get_position() - input::get_mouse_position();
			}

			ui::decoration *create_decoration() {
				ui::decoration *d = new ui::decoration();
				d->_wnd = this;
				_decos.push_back(d);
				d->_tok = --_decos.end();
				return d;
			}
			void delete_decoration(ui::decoration *d) {
				_decos.erase(d->_tok);
				delete d;
				invalidate_visual();
			}

			event<void> close_request, got_window_focus, lost_window_focus;
			event<size_changed_info> size_changed;

			ui::window_hotkey_manager hotkey_manager;
		protected:
			ui::element *_focus{this}, *_capture = nullptr;
			bool _drag = false;
			vec2i _doffset;
			std::function<bool()> _dragcontinue;
			std::list<ui::decoration*> _decos;

			virtual bool _update_drag() { // TODO still buggy
				if (_drag) {
					if (_dragcontinue()) {
						set_position(_doffset + input::get_mouse_position());
						return true;
					}
					_drag = false;
				}
				return false;
			}

			void _on_prerender() override {
				os::renderer_base::get().begin(*this);
				panel::_on_prerender();
			}
			void _on_postrender() override {
				panel::_on_postrender();
				os::renderer_base::get().end();
			}

			virtual void _on_close_request() {
				close_request();
			}
			virtual void _on_size_changed(size_changed_info &p) {
				invalidate_layout();
				size_changed(p);
			}

			void _on_key_down(ui::key_info &p) override {
				if (_focus && _focus != this) {
					_focus->_on_key_down(p);
				} else {
					element::_on_key_down(p);
				}
			}
			void _on_key_up(ui::key_info &p) override {
				if (_focus && _focus != this) {
					_focus->_on_key_up(p);
				} else {
					element::_on_key_up(p);
				}
			}
			void _on_keyboard_text(ui::text_info &p) override {
				if (_focus && _focus != this) {
					_focus->_on_keyboard_text(p);
				} else {
					element::_on_keyboard_text(p);
				}
			}

			void _recalc_layout(rectd) override {
				_clientrgn = get_padding().shrink(get_layout());
			}

			virtual void _on_removing_window_element(ui::element *e) {
				ui::element *ef = _focus;
				for (; ef != nullptr && e != ef; ef = ef->parent()) {
				}
				if (ef != nullptr) {
					if (ui::manager::get().get_focused() == _focus) {
						ui::manager::get().set_focus(this);
					} else {
						_set_window_focus_element(this);
					}
				}
			}
			virtual void _set_window_focus_element(ui::element *e) {
				assert_true_logical(e && e->get_window() == this, "corrupted element tree");
				if (e != _focus) {
					_focus = e;
					std::vector<ui::element_hotkey_group_data> gps;
					for (ui::element *cur = _focus; cur != nullptr; cur = cur->parent()) {
						std::vector<const ui::element_hotkey_group*> cgps = cur->get_hotkey_groups();
						for (auto i = cgps.begin(); i != cgps.end(); ++i) {
							gps.push_back(ui::element_hotkey_group_data(*i, cur));
						}
					}
					hotkey_manager.reset_groups_prefiltered(gps);
				}
			}

			virtual void _on_got_window_focus() {
				ui::manager::get().set_focus(_focus);
				got_window_focus.invoke();
			}
			virtual void _on_lost_window_focus() {
				if (ui::manager::get().get_focused() == _focus) { // in case the focus has already shifted
					ui::manager::get().set_focus(nullptr);
				}
				if (_capture != nullptr) {
					_capture->_on_capture_lost();
				}
				lost_window_focus.invoke();
			}

			void _custom_render() override {
				panel::_custom_render();
				// render decorations
				bool has_active = false;
				for (auto i = _decos.begin(); i != _decos.end(); ) {
					if ((*i)->_update_and_render()) {
						has_active = true;
					} else {
						if (test_bit_all((*i)->get_state(), ui::visual_manager::default_states().corpse)) {
							delete *i;
							i = _decos.erase(i);
							continue;
						}
					}
					++i;
				}
				if (has_active) {
					invalidate_visual();
				}
			}

			void _initialize() override {
				panel::_initialize();
				renderer_base::get()._new_window(*this);
			}
			void _dispose() override {
				for (ui::decoration *dec : _decos) {
					delete dec;
				}
				if (ui::manager::get().get_focused() == _focus) {
					ui::manager::get().set_focus(nullptr);
				}
				renderer_base::get()._delete_window(*this);
				panel::_dispose();
			}

			void _on_mouse_enter() override {
				if (_capture != nullptr) {
					_capture->_on_mouse_enter();
					ui::element::_on_mouse_enter();
				} else {
					panel::_on_mouse_enter();
				}
			}
			void _on_mouse_leave() override {
				if (_capture != nullptr) {
					_capture->_on_mouse_leave();
					ui::element::_on_mouse_leave();
				} else {
					panel::_on_mouse_leave();
				}
			}
			void _on_mouse_move(ui::mouse_move_info &p) override {
				if (_capture != nullptr) {
					if (!_capture->is_mouse_over()) {
						_capture->_on_mouse_enter();
					}
					_capture->_on_mouse_move(p);
					ui::element::_on_mouse_move(p);
				} else {
					panel::_on_mouse_move(p);
				}
			}
			void _on_mouse_down(ui::mouse_button_info &p) override {
				if (_capture != nullptr) {
					_capture->_on_mouse_down(p);
					mouse_down(p);
				} else {
					panel::_on_mouse_down(p);
				}
			}
			void _on_mouse_up(ui::mouse_button_info &p) override {
				if (_capture != nullptr) {
					_capture->_on_mouse_up(p);
					ui::element::_on_mouse_up(p);
				} else {
					panel::_on_mouse_up(p);
				}
			}
			void _on_mouse_scroll(ui::mouse_scroll_info &p) override {
				if (_capture != nullptr) {
					for (element *e = _capture; !p.handled() && e != this; e = e->parent()) {
						assert_true_logical(e, "corrupted element tree");
						e->_on_mouse_scroll(p);
					}
					ui::element::_on_mouse_scroll(p);
				} else {
					panel::_on_mouse_scroll(p);
				}
			}
		};

		inline void opengl_renderer_base::begin(const window_base &wnd) {
			vec2i sz = wnd.get_actual_size().convert<int>();
			_begin_render_target(_render_target_stackframe(
				true, sz.x, sz.y,
				_get_begin_window_func(wnd),
				_get_end_window_func(wnd)
			));
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			_gl_verify();
		}
	}
	namespace ui {
		inline void manager::update_invalid_visuals() {
			if (_dirty.empty()) {
				return;
			}
			auto start = std::chrono::high_resolution_clock::now();
			double diff = std::chrono::duration<double>(start - _lastrender).count();
			if (diff < _min_render_interval) {
				return;
			}
			_lastrender = start;
			std::set<element*> ss;
			for (auto i = _dirty.begin(); i != _dirty.end(); ++i) {
				os::window_base *wnd = (*i)->get_window();
				if (wnd) {
					ss.insert(wnd);
				}
			}
			_dirty.clear();
			for (auto i = ss.begin(); i != ss.end(); ++i) {
				(*i)->_on_render();
			}
			double dur = std::chrono::duration<double, std::milli>(
				std::chrono::high_resolution_clock::now() - start
				).count();
			if (dur > render_time_redline) {
				logger::get().log_warning(CP_HERE, "render cost ", dur, "ms");
			}
		}
		inline void manager::set_focus(element *elem) {
			if (elem == _focus) {
				return;
			}
			os::window_base *neww = elem == nullptr ? nullptr : elem->get_window();
			assert_true_logical((neww != nullptr) == (elem != nullptr), "corrupted element tree");
			element *oldf = _focus;
			_focus = elem;
			if (neww != nullptr) {
				neww->_set_window_focus_element(elem);
				neww->activate();
			}
			if (oldf != nullptr) {
				oldf->_on_lost_focus();
			}
			if (_focus != nullptr) {
				_focus->_on_got_focus();
			}
			logger::get().log_verbose(
				CP_HERE, "focus changed to 0x", _focus,
				" <", _focus ? demangle(typeid(*_focus).name()) : "nullptr", ">"
			);
		}

		inline os::window_base *element::get_window() {
			element *cur = this;
			while (cur->_parent != nullptr) {
				cur = cur->_parent;
			}
			return dynamic_cast<os::window_base*>(cur);
		}

		inline void decoration::_on_visual_changed() {
			_wnd->invalidate_visual();
		}

		inline void element_collection::remove(element &elem) {
			assert_true_logical(elem._parent == &_f, "corrupted element tree");
			os::window_base *wnd = _f.get_window();
			if (wnd != nullptr) {
				wnd->_on_removing_window_element(&elem);
			}
			elem._parent = nullptr;
			_cs.erase(elem._col_token);
			element_collection_change_info ci(element_collection_change_info::type::remove, &elem);
			_f._on_children_changed(ci);
			changed(ci);
		}
	}
}
