#pragma once

/// \file
/// Definition of codepad::editor::code::codebox and codepad::editor::code::component.

#include <fstream>
#include <map>
#include <functional>
#include <chrono>
#include <mutex>
#include <thread>

#include "../../ui/element.h"
#include "../../ui/panel.h"
#include "../../ui/common_elements.h"
#include "../../ui/manager.h"
#include "../../ui/draw.h"

namespace codepad::editor::code {
	class codebox;
	class editor;

	/// A component in a \ref codebox. Components are placed horizontally next to one another and fills the vertical
	/// space. Their contents usually scroll alongside the document, but the element itself is not moved, so derived
	/// classes should handle the change of vertical position themselves.
	class component : public ui::element {
		friend class codebox;
	public:
	protected:
		/// Returns the \ref codebox that it's a child of.
		codebox * _get_box() const;
		/// Returns the \ref editor component in the same \ref codebox.
		editor *_get_editor() const;

		/// Called after the component has been added to a \ref codebox.
		virtual void _on_added() {
		}
		/// Called before the component is removed from the \ref codebox. This is required to work correctly even if
		/// \ref _dispose() has been invoked but have not yet returned.
		virtual void _on_removing() {
		}

		/// Sets the component to non-focusable.
		void _initialize() override {
			element::_initialize();
			_can_focus = false;
		}
	};
	/// An element that contains an \ref editor and other \ref component "components", and forwards all keyboard
	/// events to the \ref editor.
	///
	/// \todo Add horizontal view.
	class codebox : public ui::panel_base {
	public:
		/// Sets the vertical position of the view.
		void set_vertical_position(double p) {
			_vscroll->set_value(p);
		}
		/// Returns the current vertical position.
		double get_vertical_position() const {
			return _vscroll->get_value();
		}

		/// Scrolls the view region so that the given point is visible.
		void make_point_visible(vec2d v) {
			_vscroll->make_point_visible(v.y);
		}

		/// This element manages the layout of its children, or more specifically, the \ref component "components".
		bool override_children_layout() const override {
			return true;
		}

		/// Returns a pointer the \ref editor associated with this \ref codebox.
		editor *get_editor() const {
			return _editor;
		}

		/// Adds a \ref component to the left of the \ref editor. The added component will be place to the right of
		/// all existing components.
		void add_component_left(component &e) {
			_add_component_to(e, _lcs);
		}
		/// Adds a \ref component to the right of the \ref editor. The added component will be place to the left of
		/// all existing components.
		void add_component_right(component &e) {
			_add_component_to(e, _rcs);
		}
		/// Removes the given \ref component from the \ref editor.
		void remove_component(component &e) {
			_children.remove(e);
		}

		/// Returns a list of all \ref component "components" to the left of the \ref editor.
		const std::vector<component*> &get_components_left() const {
			return _lcs;
		}
		/// Returns a list of all \ref component "components" to the right of the \ref editor.
		const std::vector<component*> &get_components_right() const {
			return _rcs;
		}

		/// Returns the region in which layout calculation for all components is performed.
		rectd get_components_region() const {
			rectd client = get_client_region();
			client.xmax = _vscroll->get_layout().xmin;
			return client;
		}

		/// Invoked when the vertical position or visible range is changed.
		event<value_update_info<double>> vertical_viewport_changed;

		/// Returns the default class of all elements of type \ref codebox.
		inline static str_t get_default_class() {
			return CP_STRLIT("codebox");
		}
	protected:
		std::vector<component*>
			/// A list of all components on the left of \ref _editor. The components are placed next to one another
			/// from left to right.
			_lcs,
			/// A list of all components on the right of \ref _editor. The components are placed next to one another
			/// from right to left.
			_rcs;
		/// Token used to listen to \ref editor::editing_visual_changed, to adjust the parameters of \ref _vscroll
		/// when the height of the document has changed.
		event<void>::token _mod_tok;
		ui::scroll_bar *_vscroll = nullptr; ///< The vertical \ref ui::scroll_bar.
		editor *_editor = nullptr; ///< The \ref editor.

		/// Adds a \ref component to the given list and \ref _children, then calls \ref component::_on_added.
		void _add_component_to(component &e, std::vector<component*> &v) {
			_children.add(e);
			v.push_back(&e);
			e._on_added();
		}

		/// Calculates and sets the parameters of \ref _vscroll.
		void _reset_scrollbars();

		/// Scrolls the viewport of the \ref codebox.
		void _on_mouse_scroll(ui::mouse_scroll_info&) override;
		/// Forwards the key down event to the \ref editor.
		void _on_key_down(ui::key_info&) override;
		/// Forwards the key up event to the \ref editor.
		void _on_key_up(ui::key_info&) override;
		/// Forwards the text event to the \ref editor.
		void _on_keyboard_text(ui::text_info&) override;

		/// Calls \ref editor::_on_codebox_got_focus.
		void _on_got_focus() override;
		/// Calls \ref editor::_on_codebox_lost_focus.
		void _on_lost_focus() override;

		/// When a child is being removed, checks if it's a \ref component and if so, calls
		/// \ref component::_on_removing.
		void _on_child_removing(ui::element*) override;
		/// Removes the child from \ref _lcs or \ref _rcs if it can be found. Checking has already been handled in
		/// \ref _on_child_removing. These are separated primarily due to the mechanism that potentially will be
		/// introduced in the future for elements to be notified when they're removed from their parents.
		void _on_child_removed(ui::element *elem) {
			// seek & erase
			auto it = std::find(_lcs.begin(), _lcs.end(), elem);
			if (it != _lcs.end()) {
				_lcs.erase(it);
			} else {
				it = std::find(_rcs.begin(), _rcs.end(), elem);
				if (it != _rcs.end()) {
					_rcs.erase(it);
				}
			}
		}

		/// Recalculates the layout of all \ref component "components" and the \ref editor.
		void _finish_layout() override;

		/// Initializes \ref _vscroll and \ref _editor.
		void _initialize() override;
		/// Unregisters from \ref editor::editing_visual_changed.
		void _dispose() override;
	};
}
