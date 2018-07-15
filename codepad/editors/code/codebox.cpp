#include "codebox.h"

/// \file
/// Implementation of certain functions of \ref codepad::editor::code::codebox.

#include <vector>

#include "editor.h"


using namespace std;

using namespace codepad::ui;

namespace codepad::editor::code {
	codebox *component::_get_box() const {
#ifdef CP_CHECK_LOGICAL_ERRORS
		codebox *cb = dynamic_cast<codebox*>(_parent);
		assert_true_logical(cb != nullptr, "the component is not a child of any codebox");
		return cb;
#else
		return static_cast<codebox*>(_parent);
#endif
	}

	editor *component::_get_editor() const {
		return _get_box()->get_editor();
	}


	/// \todo Have \ref editor listen to \ref vertical_viewport_changed and call
	/// \ref editor::_update_window_caret_position.
	void codebox::_initialize() {
		panel_base::_initialize();

		_vscroll = element::create<scroll_bar>();
		_vscroll->set_orientation(orientation::vertical);
		_vscroll->set_anchor(anchor::dock_right);
		_vscroll->value_changed += [this](value_update_info<double> &info) {
			_editor->_update_window_caret_position(); // update caret position
			vertical_viewport_changed.invoke(info);
			invalidate_visual();
		};
		_children.add(*_vscroll);

		_editor = element::create<editor>();
		_mod_tok = (_editor->editing_visual_changed += [this]() {
			_reset_scrollbars();
			});
		_children.add(*_editor);
	}
	void codebox::_dispose() {
		_editor->editing_visual_changed -= _mod_tok;
		panel_base::_dispose();
	}

	void codebox::_reset_scrollbars() {
		_vscroll->set_params(_editor->get_vertical_scroll_range(), get_layout().height());
	}

	void codebox::_on_mouse_scroll(mouse_scroll_info &p) {
		_vscroll->set_value(_vscroll->get_value() - _editor->get_scroll_delta() * p.offset);
		p.mark_handled();
	}

	void codebox::_on_key_down(key_info &p) {
		_editor->_on_key_down(p);
	}

	void codebox::_on_key_up(key_info &p) {
		_editor->_on_key_up(p);
	}

	void codebox::_on_keyboard_text(text_info &p) {
		_editor->_on_keyboard_text(p);
	}

	void codebox::_on_child_removing(ui::element *elem) {
		component *comp = dynamic_cast<component*>(elem);
		if (comp == nullptr) { // not a component
			assert_true_logical(elem == _editor || elem == _vscroll, "corrupted element tree");
			return;
		}
		comp->_on_removing();
	}

	void codebox::_finish_layout() {
		rectd lo = get_client_region();
		_child_recalc_layout(_vscroll, lo);

		vector<element*> elems(_lcs.begin(), _lcs.end());
		elems.emplace_back(_editor);
		for (element *e : _rcs) {
			elems.emplace_back(e);
		}
		stack_panel_base::layout_elements_in<orientation::horizontal>(get_components_region(), elems);

		panel_base::_finish_layout();
	}

	void codebox::_on_got_focus() {
		_editor->_on_codebox_got_focus();
		panel_base::_on_got_focus();
	}

	void codebox::_on_lost_focus() {
		_editor->_on_codebox_lost_focus();
		panel_base::_on_lost_focus();
	}
}
