#include "manager.h"

/// \file
/// Implementation of certain methods of codepad::ui::manager.

#include "../os/window.h"
#include "element.h"
#include "panel.h"

using namespace std;
using namespace std::chrono;
using namespace codepad::os;

namespace codepad::ui {
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
#ifdef CP_DETECT_LOGICAL_ERRORS
				++control_dispose_rec::get().reg_disposed;
#endif
#ifdef CP_DETECT_USAGE_ERRORS
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
			_q.emplace_back(i);
		}
		while (!_q.empty()) {
			auto li = _q.front();
			_q.pop_front();
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

	void manager::set_focus(element *elem) {
		if (elem == _focus) {
			return;
		}
#ifdef CP_DETECT_LOGICAL_ERRORS
		static bool _in = false;

		assert_true_logical(!_in, "recursive calls to set_focus");
		_in = true;
#endif
		os::window_base *neww = elem == nullptr ? nullptr : elem->get_window();
		assert_true_logical((neww != nullptr) == (elem != nullptr), "corrupted element tree");
		element *oldf = _focus;
		_focus = elem;
		if (neww != nullptr) {
			neww->_set_window_focus_element(elem);
			// set_focus may be called again here, but it's fine as long as it's called with the same element
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
#ifdef CP_DETECT_LOGICAL_ERRORS
		_in = false;
#endif
	}
}
