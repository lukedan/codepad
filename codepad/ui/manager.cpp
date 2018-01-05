#include "manager.h"
#include "element.h"
#include "panel.h"

namespace codepad {
	namespace ui {
		void manager::update_scheduled_elements() {
			monitor_performance mon(CP_HERE);
			auto nnow = std::chrono::high_resolution_clock::now();
			_upd_dt = std::chrono::duration<double>(nnow - _now).count();
			_now = nnow;
			if (!_upd.empty()) {
				std::set<element*> list;
				std::swap(list, _upd);
				for (auto i = list.begin(); i != list.end(); ++i) {
					(*i)->_on_update();
				}
			}
		}
		void manager::dispose_marked_elements() {
			monitor_performance mon(CP_HERE);
			while (!_del.empty()) {
				std::set<element*> batch;
				std::swap(batch, _del);
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
			monitor_performance mon(CP_HERE);
			if (!_targets.empty()) {
				auto start = std::chrono::high_resolution_clock::now();
				_layouting = true;
				std::unordered_map<element*, bool> ftg;
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
				double dur = std::chrono::duration<double, std::milli>(
					std::chrono::high_resolution_clock::now() - start
					).count();
				if (dur > relayout_time_redline) {
					logger::get().log_warning(CP_HERE, "relayout cost ", dur, "ms");
				}
			}
		}
	}
}
