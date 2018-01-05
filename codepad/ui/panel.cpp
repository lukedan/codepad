#include "panel.h"

namespace codepad {
	namespace ui {
		element_collection::~element_collection() {
			for (auto i = _cs.begin(); i != _cs.end(); ++i) {
				(*i)->_parent = nullptr;
			}
		}
		void element_collection::add(element &elem) {
			assert_true_usage(elem._parent == nullptr, "the element is already a child of another panel");
			elem._parent = &_f;
			// use naive approach to find the item whose z-index is less or equal
			auto ins_before = _cs.begin();
			for (; ins_before != _cs.end(); ++ins_before) {
				if ((*ins_before)->get_zindex() <= elem.get_zindex()) {
					break;
				}
			}
			elem._col_token = _cs.insert(ins_before, &elem);
			element_collection_change_info ci(element_collection_change_info::type::add, &elem);
			_f._on_children_changed(ci);
			changed(ci);
		}
		void element_collection::set_zindex(element &elem, int newz) {
			if (elem._zindex != newz) {
				auto ins_before = _cs.erase(elem._col_token);
				if (elem._zindex < newz) {
					while (ins_before != _cs.begin()) {
						--ins_before;
						if ((*ins_before)->get_zindex() > newz) {
							++ins_before;
							break;
						}
					}
				} else {
					for (; ins_before != _cs.end(); ++ins_before) {
						if ((*ins_before)->get_zindex() <= newz) {
							break;
						}
					}
				}
				elem._col_token = _cs.insert(ins_before, &elem);
				elem._zindex = newz;
			}
			element_collection_change_info ci(element_collection_change_info::type::set_zindex, &elem);
			_f._on_children_changed(ci);
			changed(ci);
		}
	}
}
