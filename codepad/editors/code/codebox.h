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

	/// An element that contains an \ref editor and other components, and forwards all keyboard events to the
	/// \ref editor.
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

		/// Returns a pointer the \ref editor associated with this \ref codebox.
		editor &get_editor() const {
			return *_editor;
		}

		/// Adds a component before the given element. For example, if \p before is \ref get_editor(), then the
		/// component will be inserted before the editor.
		void insert_component_before(ui::element *before, ui::element &comp) {
			_child_set_logical_parent(comp, this);
			_components->children().insert_before(before, comp);
		}
		/// Removes the given component.
		void remove_component(ui::element &e) {
			_components->children().remove(e);
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

		/// Returns the role identifier of the vertical scrollbar.
		inline static str_t get_vertical_scrollbar_role() {
			return CP_STRLIT("vertical_scrollbar");
		}
		/// Returns the role identifier of the horizontal scrollbar.
		inline static str_t get_horizontal_scrollbar_role() {
			return CP_STRLIT("horizontal_scrollbar");
		}
		/// Returns the role identifier of the editor.
		inline static str_t get_editor_role() {
			return CP_STRLIT("editor");
		}
		/// Returns the role identifier of the `components' panel.
		inline static str_t get_components_panel_role() {
			return CP_STRLIT("components_panel");
		}
	protected:
		/// Token used to listen to \ref editor::editing_visual_changed, to adjust the parameters of \ref _vscroll
		/// when the height of the document has changed.
		event<void>::token _mod_tok;

		ui::scrollbar *_vscroll = nullptr; ///< The vertical \ref ui::scrollbar.
		ui::panel *_components = nullptr; ///< The panel that contains all components and the \ref editor.
		editor *_editor = nullptr; ///< The \ref editor.

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

		/// Initializes \ref _vscroll and \ref _editor.
		void _initialize(const str_t&, const ui::element_metrics&) override;
		/// Removes (and disposes) all components, and unregisters from \ref editor::editing_visual_changed.
		/// Components are removed in advance in order to avoid problems that may occur when
		/// \ref ui::element::_on_removing_from_parent is called.
		void _dispose() override;
	};

	/// Utility functions for elements designed to work as components of \ref codebox.
	namespace component_helper {
		/// Returns the \ref codebox that the given \ref ui::element is in.
		inline codebox &get_box(const ui::element &elem) {
			auto *cb = dynamic_cast<codebox*>(elem.logical_parent());
			assert_true_logical(cb != nullptr, "component_helper function called with non-component");
			return *cb;
		}
		/// Returns the \ref editor that's in the same \ref codebox as the given component.
		editor &get_editor(const ui::element&);
	}
}
