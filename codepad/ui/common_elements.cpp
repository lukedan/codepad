#include "common_elements.h"

/// \file
/// Implementation of certain methods of commonly used elements.

namespace codepad::ui {
	scrollbar &scrollbar_drag_button::_get_bar() const {
		return *dynamic_cast<scrollbar*>(_logical_parent);
	}
	void scrollbar_drag_button::_on_mouse_down(mouse_button_info &p) {
		if (p.button == _trigbtn) {
			scrollbar &b = _get_bar();
			if (b.is_vertical()) {
				_doffset = p.position.y - get_layout().ymin;
			} else {
				_doffset = p.position.x - get_layout().xmin;
			}
		}
		button_base::_on_mouse_down(p);
	}
	void scrollbar_drag_button::_on_mouse_move(mouse_move_info &p) {
		if (_trigbtn_down) {
			scrollbar &b = _get_bar();
			double totsz, diff;
			rectd client = b.get_client_region();
			if (b.is_vertical()) {
				diff = p.new_position.y - client.ymin - _doffset;
				totsz = client.height();
			} else {
				diff = p.new_position.x - client.xmin - _doffset;
				totsz = client.width();
			}
			b.set_value(diff * b.get_total_range() / totsz);
		}
		button_base::_on_mouse_move(p);
	}
}
