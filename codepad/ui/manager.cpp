#include "manager.h"

/// \file
/// Implementation of certain methods of codepad::ui::manager.

#include "../os/window.h"
#include "element.h"
#include "panel.h"
#include "../editors/tabs.h"
#include "../editors/code/editor.h"
#include "../editors/code/components.h"

using namespace std;
using namespace std::chrono;
using namespace codepad::os;

namespace codepad::ui {
	manager::manager() {
		_predef_states.mouse_over = register_state_id(CP_STRLIT("mouse_over"), element_state_type::passive);
		_predef_states.mouse_down = register_state_id(CP_STRLIT("mouse_down"), element_state_type::passive);
		_predef_states.focused = register_state_id(CP_STRLIT("focused"), element_state_type::passive);
		_predef_states.corpse = register_state_id(CP_STRLIT("corpse"), element_state_type::passive);

		_predef_states.invisible = register_state_id(CP_STRLIT("invisible"), element_state_type::configuration);
		_predef_states.ghost = register_state_id(CP_STRLIT("ghost"), element_state_type::configuration);
		_predef_states.vertical = register_state_id(CP_STRLIT("vertical"), element_state_type::configuration);


		_transfunc_map.emplace(CP_STRLIT("linear"), transition_functions::linear);
		_transfunc_map.emplace(CP_STRLIT("smoothstep"), transition_functions::smoothstep);
		_transfunc_map.emplace(
			CP_STRLIT("concave_quadratic"), transition_functions::concave_quadratic
		);
		_transfunc_map.emplace(
			CP_STRLIT("convex_quadratic"), transition_functions::convex_quadratic
		);
		_transfunc_map.emplace(CP_STRLIT("concave_cubic"), transition_functions::concave_cubic);
		_transfunc_map.emplace(CP_STRLIT("convex_cubic"), transition_functions::convex_cubic);


		// TODO use reflection instead
		register_element_type<element>();
		register_element_type<panel>();
		register_element_type<stack_panel>();
		register_element_type<label>();
		register_element_type<button>();
		register_element_type<scrollbar>();
		register_element_type<scrollbar_drag_button>();
		register_element_type<window>();

		register_element_type<editor::split_panel>();
		register_element_type<editor::tab_button>();
		register_element_type<editor::tab>();
		register_element_type<editor::drag_destination_selector>();
		register_element_type<editor::tab_host>();
		register_element_type<editor::code::codebox>();
		register_element_type<editor::code::editor>();
		register_element_type<editor::code::line_number_display>();
		register_element_type<editor::code::minimap>();
	}

	void manager::update_scheduled_elements() {
		performance_monitor mon(CP_HERE);
		auto nnow = high_resolution_clock::now();
		_upd_dt = duration<double>(nnow - _lastupdate).count();
		_lastupdate = nnow;
		if (!_upd.empty()) {
			set<element*> list; // the new list
			swap(list, _upd);
			for (auto i : list) {
				i->_on_update();
			}
		}
	}

	void manager::dispose_marked_elements() {
		performance_monitor mon(CP_HERE);
		while (!_del.empty()) { // as long as there are new batches to dispose of
			set<element*> batch;
			swap(batch, _del);
			// dispose the current batch
			// new batches may be produced during this process
			for (auto i : batch) {
				i->_dispose();
#ifdef CP_CHECK_USAGE_ERRORS
				assert_true_usage(!i->_initialized, "element::_dispose() must be invoked by children classses");
#endif
				// also remove the current entry from all lists
				_targets.erase(i);
				_dirty.erase(i);
				_upd.erase(i);
				_del.erase(i);
				// delete it
				delete i;
			}
		}
	}

	void manager::invalidate_layout(element &e) {
		if (_layouting) { // add element directly to queue
			if (e._parent != nullptr && e._parent->override_children_layout()) {
				_q.emplace(e._parent, false);
			} else {
				_q.emplace(&e, true);
			}
		} else {
			_targets[&e] = true;
		}
	}

	void manager::update_invalid_layout() {
		if (_targets.empty()) {
			return;
		}
		performance_monitor mon(CP_HERE, relayout_time_redline);
		assert_true_logical(!_layouting, "update_invalid_layout() cannot be called recursively");
		_layouting = true;
		// gather the list of elements with invalidated layout
		unordered_map<element*, bool> ftg;
		for (auto i : _targets) {
			element *cur = i.first;
			bool invalid = i.second;
			if (invalid) { // the current layout is not valid
				if (cur->_parent && cur->_parent->override_children_layout()) {
					// if its parent overrides its layout then just re-validate the parent's layout
					cur = cur->_parent;
					invalid = false;
				}
			}
			bool &v = ftg[cur];
			v = v || invalid; // there may be duplicate elements
		}
		_targets.clear();
		for (auto &i : ftg) { // push all gathered elements into a queue
			_q.emplace(i);
		}
		while (!_q.empty()) {
			auto li = _q.front();
			_q.pop();
			if (li.first->get_window() == nullptr) {
				continue;
			}
			if (li.second) { // re-calculate layout
				rectd prgn;
				if (li.first->_parent) {
					prgn = li.first->_parent->get_client_region();
				}
				li.first->_recalc_layout(prgn);
			}
			li.first->_finish_layout();
		}
		_layouting = false;
	}

	void manager::update_invalid_visuals() {
		if (_dirty.empty()) {
			return;
		}
		performance_monitor mon(CP_HERE, render_time_redline);
		auto now = high_resolution_clock::now();
		double diff = duration<double>(now - _lastrender).count();
		if (diff < _min_render_interval) {
			// don't render too often
			return;
		}
		_lastrender = now;
		// gather the list of windows to render
		set<element*> ss;
		for (auto i : _dirty) {
			os::window_base *wnd = i->get_window();
			if (wnd) {
				ss.insert(wnd);
			}
		}
		_dirty.clear();
		for (auto i : ss) {
			i->_on_render();
		}
	}

	void manager::update_visual_immediate(element &e) {
		window_base *wnd = e.get_window();
		if (wnd) {
			wnd->_on_render();
		}
	}

	void manager::set_focused_element(element &elem) {
#ifdef CP_CHECK_LOGICAL_ERRORS
		static bool _in = false;

		assert_true_logical(!_in, "recursive calls to set_focus");
		_in = true;
#endif
		if (_focus_wnd != nullptr) {
			_focus_wnd->interrupt_input_method();
		}
		os::window_base *wnd = elem.get_window();
		if (wnd != _focus_wnd) {
			wnd->activate();
		}
		wnd->set_window_focused_element(elem);
#ifdef CP_CHECK_LOGICAL_ERRORS
		_in = false;
#endif
	}
}
