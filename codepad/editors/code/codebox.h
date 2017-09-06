#pragma once

#include <fstream>
#include <map>
#include <functional>
#include <chrono>
#include <mutex>
#include <thread>

#include "../../ui/element.h"
#include "../../ui/manager.h"
#include "../../ui/draw.h"
#include "../../os/window.h"

namespace codepad {
	namespace editor {
		namespace code {
			class codebox;
			class editor;

			class component : public ui::element {
				friend class codebox;
			public:
			protected:
				codebox *_get_box() const;
				editor *_get_editor() const;

				virtual void _on_added() {
				}
				virtual void _on_removing() {
				}

				void _initialize() override {
					element::_initialize();
					_can_focus = false;
				}
			};
			class codebox : public ui::panel_base { // TODO horizontal view
			public:
				void set_vertical_position(double p) {
					_vscroll->set_value(p);
				}
				double get_vertical_position() const {
					return _vscroll->get_value();
				}

				void make_point_visible(vec2d v) {
					_vscroll->make_point_visible(v.y);
				}

				bool override_children_layout() const override {
					return true;
				}

				editor *get_editor() const {
					return _editor;
				}

				void add_component_left(component &e) {
					_add_component_to(e, _lcs);
				}
				void remove_component_left(component &e) {
					_remove_component_from(e, _lcs);
				}
				const std::vector<component*> &get_components_left() const {
					return _lcs;
				}
				const std::vector<component*> &get_components_right() const {
					return _rcs;
				}

				void add_component_right(component &e) {
					_add_component_to(e, _rcs);
				}
				void remove_component_right(component &e) {
					_remove_component_from(e, _rcs);
				}

				rectd get_components_region() const {
					rectd client = get_client_region();
					client.xmax = _vscroll->get_layout().xmin;
					return client;
				}

				event<value_update_info<double>> vertical_viewport_changed;

				inline static str_t get_default_class() {
					return U"codebox";
				}
			protected:
				ui::scroll_bar *_vscroll;
				editor *_editor;
				std::vector<component*> _lcs, _rcs;

				void _add_component_to(component &e, std::vector<component*> &v) {
					_children.add(e);
					v.push_back(&e);
					e._on_added();
				}
				void _remove_component_from(component &e, std::vector<component*> &v) {
					assert_true_usage(e.parent() == this, "the component is not a child of this codebox");
					auto it = std::find(v.begin(), v.end(), &e);
					assert_true_logical(it != v.end(), "component not found in expected list");
					e._on_removing();
					v.erase(it);
					_children.remove(e);
				}

				void _reset_scrollbars();

				void _on_mouse_scroll(ui::mouse_scroll_info&) override;
				void _on_key_down(ui::key_info&) override;
				void _on_key_up(ui::key_info&) override;
				void _on_keyboard_text(ui::text_info&) override;

				void _finish_layout() override;

				void _initialize() override;
				void _dispose() override {
					while (_lcs.size() > 0) {
						component &cmp = *_lcs.back();
						_remove_component_from(cmp, _lcs);
						if (_dispose_children) {
							ui::manager::get().mark_disposal(cmp);
						}
					}
					while (_rcs.size() > 0) {
						component &cmp = *_rcs.back();
						_remove_component_from(cmp, _rcs);
						if (_dispose_children) {
							ui::manager::get().mark_disposal(cmp);
						}
					}

					panel_base::_dispose();
				}
			};

			inline codebox *component::_get_box() const {
#ifdef CP_DETECT_LOGICAL_ERRORS
				codebox *cb = dynamic_cast<codebox*>(_parent);
				assert_true_logical(
					cb != nullptr,
					"the component is not a child of any codebox but certain actions are triggered"
				);
				return cb;
#else
				return static_cast<codebox*>(_parent);
#endif
			}
		}
	}
}
