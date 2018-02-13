#include "../os/window.h"
#include "manager.h"
#include "element.h"
#include "panel.h"

using namespace std;
using namespace std::chrono;
using namespace codepad::os;

namespace codepad {
	namespace ui {
		void manager::update_scheduled_elements() {
			performance_monitor mon(CP_HERE);
			auto nnow = high_resolution_clock::now();
			_upd_dt = duration<double>(nnow - _now).count();
			_now = nnow;
			if (!_upd.empty()) {
				set<element*> list;
				swap(list, _upd);
				for (auto i = list.begin(); i != list.end(); ++i) {
					(*i)->_on_update();
				}
			}
		}

		void manager::dispose_marked_elements() {
			performance_monitor mon(CP_HERE);
			while (!_del.empty()) {
				set<element*> batch;
				swap(batch, _del);
				for (auto i = batch.begin(); i != batch.end(); ++i) {
#ifdef CP_DETECT_LOGICAL_ERRORS
					++control_dispose_rec::get().reg_disposed;
#endif
					(*i)->_dispose();
					_targets.erase(*i);
					_dirty.erase(*i);
					_upd.erase(*i);
					_del.erase(*i);
#ifdef CP_DETECT_USAGE_ERRORS
					assert_true_usage(!(*i)->_initialized, "element::_dispose() must be invoked by children classses");
#endif
					delete *i;
				}
			}
		}

		void manager::update_invalid_layouts() {
			if (!_targets.empty()) {
				performance_monitor mon(CP_HERE, relayout_time_redline);
				_layouting = true;
				unordered_map<element*, bool> ftg;
				for (auto i = _targets.begin(); i != _targets.end(); ++i) {
					element *cur = i->first;
					if (i->second) {
						while (cur->_parent && cur->_parent->is_dependent_relayout()) {
							cur = cur->_parent;
						}
					}
					bool &v = ftg[cur];
					v = v || i->second;
				}
				_targets.clear();
				for (auto i = ftg.begin(); i != ftg.end(); ++i) {
					_q.push_back(_layout_info(i->first, i->second));
				}
				while (!_q.empty()) {
					_layout_info li = _q.front();
					_q.pop_front();
					if (li.need_recalc) {
						rectd prgn;
						if (li.elem->_parent) {
							prgn = li.elem->_parent->get_client_region();
						}
						li.elem->_recalc_layout(prgn);
					}
					li.elem->_finish_layout();
				}
				_layouting = false;
			}
		}

		void manager::update_invalid_visuals() {
			if (_dirty.empty()) {
				return;
			}
			performance_monitor mon(CP_HERE, render_time_redline);
			auto now = high_resolution_clock::now();
			double diff = duration<double>(now - _lastrender).count();
			if (diff < _min_render_interval) {
				return;
			}
			_lastrender = now;
			set<element*> ss;
			for (auto i = _dirty.begin(); i != _dirty.end(); ++i) {
				os::window_base *wnd = (*i)->get_window();
				if (wnd) {
					ss.insert(wnd);
				}
			}
			_dirty.clear();
			for (auto i : ss) {
				i->_on_render();
				update_visual_immediate(*i);
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
			os::window_base *neww = elem == nullptr ? nullptr : elem->get_window();
			assert_true_logical((neww != nullptr) == (elem != nullptr), "corrupted element tree");
			element *oldf = _focus;
			_focus = elem;
			if (neww != nullptr) {
				neww->_set_window_focus_element(elem);
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
		}
	}
}
