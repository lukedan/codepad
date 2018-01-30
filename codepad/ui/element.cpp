#include "../os/window.h"
#include "element.h"
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
			return manager::get().get_focused() == this;
		}

		void element::_on_mouse_down(mouse_button_info &p) {
			mouse_down(p);
			if (_can_focus) {
				p.mark_focus_set();
				manager::get().set_focus(this);
			}
			_set_visual_style_bit(visual::get_predefined_states().mouse_down, true);
		}

		void element::_on_render() {
			if (test_bit_all(_vis, visibility::render_only)) {
				_on_prerender();
				if (_rst.update_and_render(get_layout())) {
					invalidate_visual();
				}
				_rst.render(get_layout());
				_custom_render();
				_on_postrender();
			}
		}

		void element::_recalc_layout(rectd prgn) {
			_layout = prgn;
			vec2d sz = get_target_size();
			layout_on_direction(
				test_bit_all(_anchor, anchor::left), test_bit_all(_anchor, anchor::right),
				_layout.xmin, _layout.xmax, _margin.left, _margin.right, sz.x
			);
			layout_on_direction(
				test_bit_all(_anchor, anchor::top), test_bit_all(_anchor, anchor::bottom),
				_layout.ymin, _layout.ymax, _margin.top, _margin.bottom, sz.y
			);
			_clientrgn = get_padding().shrink(get_layout());
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
			_wnd->invalidate_visual();
		}
	}
}
