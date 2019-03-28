// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of tab hosts.

#include "../element.h"
#include "../panel.h"
#include "../manager.h"
#include "tab.h"

namespace codepad::ui::tabs {
	/// Specifies the type of a tab's destination when being dragged.
	enum class drag_destination_type {
		new_window, ///< The tab will be moved to a new window.
		/// The tab has been added to a \ref host, and the user is currently dragging and repositioning it in
		/// the tab list. After the user finishes dragging, it will remain at its place in the \ref host.
		combine_in_tab,
		combine, ///< The tab will be added to a \ref host.
		/// The current \ref host will be split into two, with the original tabs on the right and the
		/// tab being dragged on the left.
		new_panel_left,
		/// The current \ref host will be split into two, with the original tabs on the bottom and the
		/// tab being dragged on the top.
		new_panel_top,
		/// The current \ref host will be split into two, with the original tabs on the left and the
		/// tab being dragged on the right.
		new_panel_right,
		/// The current \ref host will be split into two, with the original tabs on the top and the
		/// tab being dragged on the bottom.
		new_panel_bottom
	};

	/// Used to select the destimation of a \ref tab that's being dragged.
	class drag_destination_selector : public panel_base {
	public:
		/// Returns the current \ref drag_destination_type.
		drag_destination_type get_drag_destination(vec2d) const {
			return _dest;
		}

		/// Returns the default class of elements of this type.
		inline static str_view_t get_default_class() {
			return CP_STRLIT("drag_destination_selector");
		}

		/// Returns the role identifier of the `split left' indicator.
		inline static str_view_t get_split_left_indicator_role() {
			return CP_STRLIT("split_left_indicator");
		}
		/// Returns the role identifier of the `split right' indicator.
		inline static str_view_t get_split_right_indicator_role() {
			return CP_STRLIT("split_right_indicator");
		}
		/// Returns the role identifier of the `split up' indicator.
		inline static str_view_t get_split_up_indicator_role() {
			return CP_STRLIT("split_up_indicator");
		}
		/// Returns the role identifier of the `split down' indicator.
		inline static str_view_t get_split_down_indicator_role() {
			return CP_STRLIT("split_down_indicator");
		}
		/// Returns the role identifier of the `combine' indicator.
		inline static str_view_t get_combine_indicator_role() {
			return CP_STRLIT("combine_indicator");
		}
	protected:
		element
			/// Element indicating that the result should be \ref drag_destination_type::new_panel_left.
			* _split_left = nullptr,
			/// Element indicating that the result should be \ref drag_destination_type::new_panel_right.
			*_split_right = nullptr,
			/// Element indicating that the result should be \ref drag_destination_type::new_panel_top.
			*_split_up = nullptr,
			/// Element indicating that the result should be \ref drag_destination_type::new_panel_bottom.
			*_split_down = nullptr,
			/// Element indicating that the result should be \ref drag_destination_type::combine.
			*_combine = nullptr;
		/// The current drag destination.
		drag_destination_type _dest = drag_destination_type::new_window;

		/// Initializes all destination indicators.
		void _initialize(str_view_t cls, const element_metrics &metrics) override {
			panel_base::_initialize(cls, metrics);

			get_manager().get_class_arrangements().get_or_default(cls).construct_children(*this, {
				{get_split_left_indicator_role(), _role_cast(_split_left)},
				{get_split_right_indicator_role(), _role_cast(_split_right)},
				{get_split_up_indicator_role(), _role_cast(_split_up)},
				{get_split_down_indicator_role(), _role_cast(_split_down)},
				{get_combine_indicator_role(), _role_cast(_combine)},
				});

			set_can_focus(false);
			set_zindex(zindex::overlay);

			_setup_indicator(*_split_left, drag_destination_type::new_panel_left);
			_setup_indicator(*_split_right, drag_destination_type::new_panel_right);
			_setup_indicator(*_split_up, drag_destination_type::new_panel_top);
			_setup_indicator(*_split_down, drag_destination_type::new_panel_bottom);
			_setup_indicator(*_combine, drag_destination_type::combine);
		}

		/// Initializes the given destination indicator.
		void _setup_indicator(element &elem, drag_destination_type type) {
			elem.set_can_focus(false);
			elem.mouse_enter += [this, type]() {
				_dest = type;
			};
			elem.mouse_leave += [this]() {
				_dest = drag_destination_type::new_window;
			};
		}
	};

	/// An element for displaying multiple tabs. It contains a ``tabs'' region for displaying the
	/// \ref tab_button "tab_buttons" of all available \ref tab "tabs" and a region that displays the currently
	/// selected tab.
	class host final : public panel_base {
		friend tab;
		friend tab_manager;
	public:
		/// Adds a \ref tab to the end of the tab list. If there were no tabs in the tab list prior to this
		/// operation, the newly added tab will be automatically activated.
		void add_tab(tab &t) {
			_child_set_logical_parent(t, this);
			_child_set_logical_parent(*t._btn, this);
			_tab_contents_region->children().add(t);
			_tab_buttons_region->children().add(*t._btn);

			t.set_render_visibility(false);
			t.set_hittest_visibility(false);
			if (get_tab_count() == 1) {
				switch_tab(t);
			}
		}
		/// Removes a \ref tab from this host by simply removing it from \ref _children. The rest are handled by
		/// \ref _on_child_removing.
		void remove_tab(tab &t) {
			_tab_contents_region->children().remove(t);
		}

		/// Switches the currently visible tab, but without changing the focus.
		void switch_tab(tab &t) {
			assert_true_logical(t.logical_parent() == this, "the tab doesn't belong to this host");
			if (_active_tab) {
				_active_tab->set_render_visibility(false);
				_active_tab->set_hittest_visibility(false);
				_active_tab->set_state_bits(get_manager().get_predefined_states().selected, false);
				_active_tab->_btn->set_zindex(0); // TODO a bit hacky
			}
			_active_tab = &t;
			t.set_render_visibility(true);
			t.set_hittest_visibility(true);
			t.set_state_bits(get_manager().get_predefined_states().selected, true);
			t._btn->set_zindex(1);
		}
		/// Switches the currently visible tab and sets the focus to that tab.
		void activate_tab(tab &t) {
			switch_tab(t);
			get_manager().get_scheduler().set_focused_element(&t);
		}

		/// Moves the given tab before another specified tab. If the specified tab is \p nullptr, the tab is moved
		/// to the end of the tab list. If the moved tab was previously visible, it will remain visible after being
		/// moved.
		void move_tab_before(tab &target, tab *before) {
			_tab_contents_region->children().move_before(target, before);
			_tab_buttons_region->children().move_before(*target._btn, before == nullptr ? nullptr : before->_btn);
		}

		/// Returns the region that all tab buttons are in.
		rectd get_tab_buttons_region() const {
			return _tab_buttons_region->get_layout();
		}

		/// Returns the total number of tabs in the \ref host.
		size_t get_tab_count() const {
			return _tab_contents_region->children().size();
		}

		/// Returns the manager of this tab.
		tab_manager &get_tab_manager() const {
			return *_tab_manager;
		}

		/// Returns the list of tabs.
		const element_collection &get_tabs() const {
			return _tab_contents_region->children();
		}

		/// Returns the default class of elements of type \ref host.
		inline static str_view_t get_default_class() {
			return CP_STRLIT("tab_host");
		}

		/// Returns the role identifier of the region that contains all tab buttons.
		inline static str_view_t get_tab_buttons_region_role() {
			return CP_STRLIT("tab_buttons_region");
		}
		/// Returns the role identifier of the region that contains tab contents.
		inline static str_view_t get_tab_contents_region_role() {
			return CP_STRLIT("tab_contents_region");
		}
	protected:
		panel
			*_tab_buttons_region = nullptr, ///< The panel that contains all tab buttons.
			*_tab_contents_region = nullptr; ///< The panel that contains the contents of all tabs.

		/// Pointer to the active tab.
		tab *_active_tab = nullptr;
		/// The \ref drag_destination_selector currently attached to this \ref host.
		drag_destination_selector *_dsel = nullptr;

		/// Sets the associated \ref drag_destination_selector.
		void _set_drag_dest_selector(drag_destination_selector *sel) {
			if (_dsel == sel) {
				return;
			}
			if (_dsel) {
				_children.remove(*_dsel);
			}
			_dsel = sel;
			if (_dsel) {
				_children.add(*_dsel);
			}
		}

		/// Called when a \ref tab's being removed from \ref _tab_contents_region to change the currently active tab
		/// if necessary.
		///
		/// \todo Select a better tab when the active tab is disposed.
		void _on_tab_removing(tab &t) {
			if (&t == _active_tab) { // change active tab
				if (_tab_contents_region->children().size() == 1) {
					_active_tab = nullptr;
				} else {
					auto it = _tab_contents_region->children().items().begin();
					for (; it != _tab_contents_region->children().items().end() && *it != &t; ++it) {
					}
					assert_true_logical(
						it != _tab_contents_region->children().items().end(),
						"removed tab in incorrect host"
					);
					auto original = it;
					if (++it == _tab_contents_region->children().items().end()) {
						it = --original;
					}
					switch_tab(*dynamic_cast<tab*>(*it));
				}
			}
		}
		/// Called when a \ref tab has been removed from \ref _tab_contents_region, to remove the associated
		/// \ref tab_button and the corresponding entry in \ref _tabs.
		void _on_tab_removed(tab&);

		/// Initializes \ref _tab_buttons_region and \ref _tab_contents_region.
		void _initialize(str_view_t cls, const element_metrics &metrics) override {
			panel_base::_initialize(cls, metrics);

			get_manager().get_class_arrangements().get_or_default(cls).construct_children(*this, {
				{get_tab_buttons_region_role(), _role_cast(_tab_buttons_region)},
				{get_tab_contents_region_role(), _role_cast(_tab_contents_region)}
				});

			_tab_contents_region->children().changing += [this](element_collection::change_info &p) {
				if (p.change_type == element_collection::change_info::type::remove) {
					tab *t = dynamic_cast<tab*>(&p.subject);
					assert_true_logical(t != nullptr, "corrupted element tree");
					_on_tab_removing(*t);
				}
			};
			_tab_contents_region->children().changed += [this](element_collection::change_info &p) {
				if (p.change_type == element_collection::change_info::type::remove) {
					tab *t = dynamic_cast<tab*>(&p.subject);
					assert_true_logical(t != nullptr, "corrupted element tree");
					_on_tab_removed(*t);
				}
			};
		}
	private:
		tab_manager *_tab_manager = nullptr; ///< The manager of this tab.
	};
}
