#include "panel.h"

/// \file
/// Implementation of certain methods of codepad::ui::element_collection.

#include "manager.h"
#include "../os/window.h"

using namespace codepad::os;

namespace codepad::ui {
	element_collection::~element_collection() {
		assert_true_logical(_cs.empty(), "clear() not called in panel_base::_dispose()");
	}

	void element_collection::add(element &elem) {
		assert_true_usage(elem._parent == nullptr, "the element is already a child of another panel");
		_f._on_child_adding(&elem);
		changing.invoke_noret(element_collection_change_info::type::add, &elem);
		elem._parent = &_f;
		// use naive approach to find the item whose z-index is less or equal
		auto ins_before = _cs.begin();
		for (; ins_before != _cs.end(); ++ins_before) {
			if ((*ins_before)->get_zindex() <= elem.get_zindex()) {
				break;
			}
		}
		elem._col_token = _cs.insert(ins_before, &elem);
		_f._on_child_added(&elem);
		changed.invoke_noret(element_collection_change_info::type::add, &elem);
	}

	void element_collection::set_zindex(element &elem, int newz) {
		_f._on_child_zindex_changing(&elem);
		changing.invoke_noret(element_collection_change_info::type::set_zindex, &elem);
		if (elem._zindex != newz) {
			auto ins_before = _cs.erase(elem._col_token);
			if (elem._zindex < newz) { // move forward
				while (ins_before != _cs.begin()) {
					--ins_before;
					if ((*ins_before)->get_zindex() > newz) {
						++ins_before;
						break;
					}
				}
			} else { // move back
				for (; ins_before != _cs.end(); ++ins_before) {
					if ((*ins_before)->get_zindex() <= newz) {
						break;
					}
				}
			}
			elem._col_token = _cs.insert(ins_before, &elem);
			elem._zindex = newz;
		}
		_f._on_child_zindex_changed(&elem);
		changed.invoke_noret(element_collection_change_info::type::set_zindex, &elem);
	}

	void element_collection::remove(element &elem) {
		assert_true_logical(elem._parent == &_f, "corrupted element tree");
		_f._on_child_removing(&elem);
		changing.invoke_noret(element_collection_change_info::type::remove, &elem);
		window_base *wnd = _f.get_window();
		if (wnd != nullptr) {
			wnd->_on_removing_window_element(&elem);
		}
		elem._parent = nullptr;
		_cs.erase(elem._col_token);
		_f._on_child_removed(&elem);
		changed.invoke_noret(element_collection_change_info::type::remove, &elem);
	}

	void element_collection::clear() {
		while (!_cs.empty()) {
			remove(**_cs.begin());
		}
	}


	void panel_base::_on_mouse_down(mouse_button_info &p) {
		bool childhit = false;
		for (auto i : _children) {
			if (_hit_test_child(i, p.position)) {
				i->_on_mouse_down(p);
				childhit = true;
				break;
			}
		}
		mouse_down.invoke(p);
		if (p.button == os::input::mouse_button::primary) {
			if (_can_focus && !p.focus_set()) {
				p.mark_focus_set();
				get_window()->set_window_focused_element(*this);
			}
			if (!childhit) { // mouse is over a child
				_set_state_bit(manager::get().get_predefined_states().mouse_down, true);
			}
		}
	}

	bool panel_base::_hit_test_child(element *c, vec2d p) {
		if (!test_bit_any(c->get_state(), manager::get().get_predefined_states().ghost)) {
			return c->hit_test(p);
		}
		return false;
	}

	void panel_base::_dispose() {
		if (_dispose_children) {
			for (auto i : _children) {
				manager::get().mark_disposal(*i);
			}
		}
		_children.clear();
		element::_dispose();
	}
}
