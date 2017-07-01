#pragma once

#include <fstream>
#include <map>
#include <functional>
#include <chrono>
#include <mutex>
#include <thread>

#include "../ui/element.h"
#include "../ui/manager.h"
#include "../ui/textrenderer.h"
#include "../os/window.h"

namespace codepad {
	namespace editor {
		class codebox;
		class codebox_editor;
		class codebox_editor_code;

		class codebox_component : public ui::element {
		public:
		protected:
			codebox *_get_box() const;
			template <typename T> T *_get_editor() const;
		};
		class codebox : public ui::panel_base {
			friend class codebox_editor;
			friend class codebox_editor_code;
		public:
			void set_vertical_position(double p) {
				_vscroll->set_value(p);
			}
			double get_vertical_position() const {
				return _vscroll->get_value();
			}

			void make_point_visible(vec2d v) {
				_vscroll->make_point_visible(v.y);
				// TODO horizontal view
			}

			bool override_children_layout() const override {
				return true;
			}

			codebox_editor *get_editor() const {
				return _editor;
			}

			void add_component_left(codebox_component &e) {
				_add_component_to(e, _lcs);
			}
			void remove_component_left(codebox_component &e) {
				_remove_component_from(e, _lcs);
			}

			void add_component_right(codebox_component &e) {
				_add_component_to(e, _rcs);
			}
			void remove_component_right(codebox_component &e) {
				_remove_component_from(e, _rcs);
			}

			template <typename T> T *create_editor() {
				assert(!_editor);
				_editor = element::create<T>();
				_children.add(*static_cast<element*>(_editor));
				return static_cast<T*>(_editor);
			}
		protected:
			ui::scroll_bar *_vscroll;
			codebox_editor *_editor = nullptr;
			std::vector<codebox_component*> _lcs, _rcs;

			void _add_component_to(codebox_component &e, std::vector<codebox_component*> &v) {
				_children.add(e);
				v.push_back(&e);
			}
			void _remove_component_from(codebox_component &e, std::vector<codebox_component*> &v) {
				assert(e.parent() == this);
				auto it = std::find(v.begin(), v.end(), &e);
				assert(it != v.end());
				v.erase(it);
				_children.remove(e);
			}

			void _reset_scrollbars();
			void _on_content_modified() {
				_reset_scrollbars();
			}

			void _on_mouse_scroll(ui::mouse_scroll_info&) override;

			void _finish_layout() override;

			void _initialize() override;
		};
		class codebox_editor : public codebox_component {
		public:
			virtual double get_scroll_delta() const = 0;
			virtual double get_vertical_scroll_range() const = 0;
		};

		inline codebox *codebox_component::_get_box() const {
#ifndef NDEBUG
			codebox *cb = dynamic_cast<codebox*>(_parent);
			assert(cb);
			return cb;
#else
			return static_cast<codebox*>(_parent);
#endif
		}
		template <typename T> inline T *codebox_component::_get_editor() const {
			static_assert(std::is_base_of<codebox_editor, T>::value, "target must be editor");
			return dynamic_cast<T*>(_get_box()->get_editor());
		}

		inline void codebox::_initialize() {
			panel_base::_initialize();

			_vscroll = element::create<ui::scroll_bar>();
			_vscroll->set_anchor(ui::anchor::dock_right);
			_vscroll->value_changed += [this](value_update_info<double>&) {
				invalidate_visual();
			};
			_children.add(*_vscroll);
		}
		inline void codebox::_reset_scrollbars() {
			_vscroll->set_params(_editor->get_vertical_scroll_range(), get_layout().height());
		}
		inline void codebox::_on_mouse_scroll(ui::mouse_scroll_info &p) {
			_vscroll->set_value(_vscroll->get_value() - _editor->get_scroll_delta() * p.delta);
			p.mark_handled();
		}
		inline void codebox::_finish_layout() {
			rectd lo = get_client_region();
			_child_recalc_layout(_vscroll, lo);

			double lpos = lo.xmin;
			for (auto i = _lcs.begin(); i != _lcs.end(); ++i) {
				double cw = (*i)->get_desired_size().x;
				ui::thickness mg = (*i)->get_margin();
				lpos += mg.left;
				_child_set_layout(*i, rectd(lpos, lpos + cw, lo.ymin, lo.ymax));
				lpos += cw + mg.right;
			}

			double rpos = _vscroll->get_layout().xmin;
			for (auto i = _rcs.rbegin(); i != _rcs.rend(); ++i) {
				double cw = (*i)->get_desired_size().x;
				ui::thickness mg = (*i)->get_margin();
				rpos -= mg.right;
				_child_set_layout(*i, rectd(rpos - cw, rpos, lo.ymin, lo.ymax));
				rpos -= cw - mg.left;
			}

			ui::thickness emg = _editor->get_margin();
			_child_set_layout(_editor, rectd(lpos + emg.left, rpos - emg.right, lo.ymin, lo.ymax));

			_reset_scrollbars();
			panel_base::_finish_layout();
		}
	}
}
