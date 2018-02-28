#include "common_elements.h"

/// \file common_elements.cpp
/// Implementation of certain methods of commonly used elements.

namespace codepad::ui {
	scroll_bar *scroll_bar_drag_button::_get_bar() const {
#ifdef CP_DETECT_LOGICAL_ERRORS
		auto res = dynamic_cast<scroll_bar*>(_parent);
		assert_true_logical(res != nullptr, "the button is not a child of a scroll bar");
		return res;
#else
		return static_cast<scroll_bar*>(_parent);
#endif
	}
	void scroll_bar_drag_button::_on_mouse_down(mouse_button_info &p) {
		if (p.button == _trigbtn) {
			scroll_bar *b = _get_bar();
			if (b->get_orientation() == orientation::horizontal) {
				_doffset = p.position.x - get_layout().xmin;
			} else {
				_doffset = p.position.y - get_layout().ymin;
			}
		}
		button_base::_on_mouse_down(p);
	}
	void scroll_bar_drag_button::_on_mouse_move(mouse_move_info &p) {
		if (_trigbtn_down) {
			scroll_bar *b = _get_bar();
			double totsz, diff;
			rectd client = b->get_client_region();
			if (b->get_orientation() == orientation::horizontal) {
				diff = p.new_pos.x - client.xmin - _doffset;
				totsz = client.width();
			} else {
				diff = p.new_pos.y - client.ymin - _doffset;
				totsz = client.height();
			}
			b->set_value(diff * b->get_total_range() / totsz);
		}
		button_base::_on_mouse_move(p);
	}
}
