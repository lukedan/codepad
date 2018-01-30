#include "codebox.h"
#include "editor.h"

using namespace codepad::ui;

namespace codepad {
	namespace editor {
		namespace code {
			editor *component::_get_editor() const {
				return _get_box()->get_editor();
			}


			void codebox::_initialize() {
				panel_base::_initialize();

				_vscroll = element::create<scroll_bar>();
				_vscroll->set_anchor(anchor::dock_right);
				_vscroll->value_changed += [this](value_update_info<double> &info) {
					vertical_viewport_changed.invoke(info);
					invalidate_visual();
				};
				_children.add(*_vscroll);

				_editor = element::create<editor>();
				_editor->content_modified += [this]() {
					_reset_scrollbars();
				};
				_editor->folding_changed += [this]() {
					_reset_scrollbars();
				};
				_children.add(*_editor);
			}

			void codebox::_reset_scrollbars() {
				_vscroll->set_params(_editor->get_vertical_scroll_range(), get_layout().height());
			}

			void codebox::_on_mouse_scroll(mouse_scroll_info &p) {
				_vscroll->set_value(_vscroll->get_value() - _editor->get_scroll_delta() * p.delta);
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

			void codebox::_finish_layout() {
				rectd lo = get_client_region();
				_child_recalc_layout(_vscroll, lo);

				rectd r = get_components_region();
				double lpos = r.xmin;
				for (auto i = _lcs.begin(); i != _lcs.end(); ++i) {
					double cw = (*i)->get_desired_size().x;
					thickness mg = (*i)->get_margin();
					lpos += mg.left;
					_child_set_layout(*i, rectd(lpos, lpos + cw, lo.ymin, lo.ymax));
					lpos += cw + mg.right;
				}

				double rpos = r.xmax;
				for (auto i = _rcs.rbegin(); i != _rcs.rend(); ++i) {
					double cw = (*i)->get_desired_size().x;
					thickness mg = (*i)->get_margin();
					rpos -= mg.right;
					_child_set_layout(*i, rectd(rpos - cw, rpos, lo.ymin, lo.ymax));
					rpos -= cw - mg.left;
				}

				thickness emg = _editor->get_margin();
				_child_set_layout(_editor, rectd(lpos + emg.left, rpos - emg.right, lo.ymin, lo.ymax));

				_reset_scrollbars();
				panel_base::_finish_layout();
			}

			void codebox::_on_got_focus() {
				panel_base::_on_got_focus();
				_editor->_on_codebox_got_focus();
			}

			void codebox::_on_lost_focus() {
				_editor->_on_codebox_lost_focus();
				panel_base::_on_lost_focus();
			}
		}
	}
}
