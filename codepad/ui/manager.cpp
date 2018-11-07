#include "manager.h"

/// \file
/// Implementation of certain methods of codepad::ui::manager.

#include <deque>

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

		_predef_states.render_invisible = register_state_id(CP_STRLIT("render_invisible"), element_state_type::configuration);
		_predef_states.hittest_invisible = register_state_id(CP_STRLIT("hittest_invisible"), element_state_type::configuration);
		_predef_states.layout_invisible = register_state_id(CP_STRLIT("layout_invisible"), element_state_type::configuration);
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

		// from schedule_visual_config_update()
		if (!_visualcfg_update.empty()) {
			set<element*> oldset;
			swap(oldset, _visualcfg_update);
			for (element *e : oldset) {
				e->invalidate_visual();
				if (!e->_on_update_visual_configurations(_upd_dt)) {
					schedule_visual_config_update(*e);
				}
			}
		}

		// from schedule_metrics_config_update()
		if (!_metricscfg_update.empty()) {
			set<element*> oldset;
			swap(oldset, _metricscfg_update);
			for (element *e : oldset) {
				e->invalidate_layout();
				if (!e->_config.metrics_config.update(_upd_dt)) {
					schedule_metrics_config_update(*e);
				}
			}
		}

		// from schedule_update()
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
			for (element *elem : batch) {
				elem->_dispose();
#ifdef CP_CHECK_USAGE_ERRORS
				assert_true_usage(!elem->_initialized, "element::_dispose() must be invoked by children classses");
#endif
				// also remove the current entry from all lists
				auto *pnl = dynamic_cast<panel_base*>(elem);
				if (pnl) {
					_children_layout_scheduled.erase(pnl);
				}
				_layout_notify.erase(elem);
				_visualcfg_update.erase(elem);
				_metricscfg_update.erase(elem);
				_dirty.erase(elem);
				_del.erase(elem);
				_upd.erase(elem);
				// delete it
				delete elem;
			}
		}
	}

	void manager::invalidate_layout(element &e) {
		// TODO maybe optimize for panels
		if (e.parent() != nullptr) {
			invalidate_children_layout(*e.parent());
		}
	}

	void manager::invalidate_children_layout(panel_base &e) {
		_children_layout_scheduled.emplace(&e);
	}

	void manager::update_invalid_layout() {
		if (_children_layout_scheduled.empty() && _layout_notify.empty()) {
			return;
		}
		performance_monitor mon(CP_HERE, relayout_time_redline);
		assert_true_logical(!_layouting, "update_invalid_layout() cannot be called recursively");
		_layouting = true;
		deque<element*> notify(_layout_notify.begin(), _layout_notify.end()); // list of elements to be notified
		_layout_notify.clear();
		// gather the list of elements with invalidated layout
		set<panel_base*> childrenupdate;
		swap(childrenupdate, _children_layout_scheduled);
		for (panel_base *pnl : childrenupdate) {
			pnl->_on_update_children_layout();
			for (element *elem : pnl->_children.items()) {
				notify.emplace_back(elem);
			}
		}
		while (!notify.empty()) {
			element *li = notify.front();
			notify.pop_front();
			li->_on_layout_changed();
			auto *pnl = dynamic_cast<panel_base*>(li);
			if (pnl != nullptr) {
				for (element *elem : pnl->_children.items()) {
					notify.emplace_back(elem);
				}
			}
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
