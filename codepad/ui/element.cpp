#include "element.h"

/// \file
/// Implementation of certain methods related to ui::element.

#include "../os/window.h"
#include "panel.h"
#include "manager.h"

using namespace codepad::os;

namespace codepad {
	namespace ui {
		void element::invalidate_layout() {
			manager::get().invalidate_layout(*this);
		}

		void element::revalidate_layout() {
			manager::get().revalidate_layout(*this);
		}

		void element::invalidate_visual() {
			manager::get().invalidate_visual(*this);
		}

		bool element::has_focus() const {
			return manager::get().get_focused_element() == this;
		}

		void element::_on_mouse_down(mouse_button_info &p) {
			if (p.button == input::mouse_button::primary) {
				if (_can_focus && !p.focus_set()) {
					p.mark_focus_set();
					get_window()->set_window_focused_element(*this);
				}
				_set_visual_style_bit(visual::get_predefined_states().mouse_down, true);
			}
			mouse_down.invoke(p);
		}

		void element::_on_render() {
			if (test_bit_all(_vis, visibility::render_only)) {
				_on_prerender();
				if (_rst.update_and_render(get_layout())) {
					invalidate_visual();
				}
				_custom_render();
				_on_postrender();
			}
		}

		void element::_recalc_horizontal_layout(double xmin, double xmax) {
			auto wprop = get_layout_width();
			_layout.xmin = xmin;
			_layout.xmax = xmax;
			layout_on_direction(
				test_bit_all(_anchor, anchor::left), wprop.second, test_bit_all(_anchor, anchor::right),
				_layout.xmin, _layout.xmax, _margin.left, wprop.first, _margin.right
			);
		}

		void element::_recalc_vertical_layout(double ymin, double ymax) {
			auto hprop = get_layout_height();
			_layout.ymin = ymin;
			_layout.ymax = ymax;
			layout_on_direction(
				test_bit_all(_anchor, anchor::top), hprop.second, test_bit_all(_anchor, anchor::bottom),
				_layout.ymin, _layout.ymax, _margin.top, hprop.first, _margin.bottom
			);
		}

		void element::_dispose() {
			if (_parent) {
				_parent->_children.remove(*this);
			}
#ifdef CP_DETECT_USAGE_ERRORS
			_initialized = false;
#endif
		}

		void element::set_zindex(int v) {
			if (_parent) {
				_parent->_children.set_zindex(*this, v);
			} else {
				_zindex = v;
			}
		}

		window_base *element::get_window() {
			element *cur = this;
			while (cur->_parent != nullptr) {
				cur = cur->_parent;
			}
			return dynamic_cast<window_base*>(cur);
		}


		void decoration::_on_visual_changed() {
			if (_wnd) {
				_wnd->invalidate_visual();
			}
		}

		decoration::~decoration() {
			if (_wnd) {
				_wnd->_on_decoration_destroyed(*this);
			}
		}
	}
}
