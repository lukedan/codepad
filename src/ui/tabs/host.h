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
	/// Indicates how the tab hosts should be split when the user drops the tab button in a \ref host.
	enum class drag_split_type : std::uint8_t {
		new_window, ///< The tab should be put in a new window.
		combine, ///< The tab should be added to this host.
		/// The current \ref host will be split in two, with the original tabs on the right and the
		/// tab being dragged on the left.
		split_left,
		/// The current \ref host will be split in two, with the original tabs on the bottom and the
		/// tab being dragged on the top.
		split_top,
		/// The current \ref host will be split in two, with the original tabs on the left and the
		/// tab being dragged on the right.
		split_right,
		/// The current \ref host will be split in two, with the original tabs on the top and the
		/// tab being dragged on the bottom.
		split_bottom
	};

	/// Used to select the destimation of a \ref tab that's being dragged.
	class drag_destination_selector : public panel {
	public:
		/// Returns the current \ref drag_split_type.
		drag_split_type get_drag_destination() const {
			return _dest;
		}

		/// Called to update the mouse position.
		void update(mouse_move_info &p) {
			_on_mouse_move(p);
		}

		/// Returns the default class of elements of this type.
		inline static std::u8string_view get_default_class() {
			return u8"drag_destination_selector";
		}

		/// Returns the name identifier of the `split left' indicator.
		inline static std::u8string_view get_split_left_indicator_name() {
			return u8"split_left_indicator";
		}
		/// Returns the name identifier of the `split right' indicator.
		inline static std::u8string_view get_split_right_indicator_name() {
			return u8"split_right_indicator";
		}
		/// Returns the name identifier of the `split up' indicator.
		inline static std::u8string_view get_split_up_indicator_name() {
			return u8"split_up_indicator";
		}
		/// Returns the name identifier of the `split down' indicator.
		inline static std::u8string_view get_split_down_indicator_name() {
			return u8"split_down_indicator";
		}
		/// Returns the name identifier of the `combine' indicator.
		inline static std::u8string_view get_combine_indicator_name() {
			return u8"combine_indicator";
		}
	protected:
		element
			/// Element indicating that the result should be \ref drag_split_type::split_left.
			*_split_left = nullptr,
			/// Element indicating that the result should be \ref drag_split_type::split_right.
			*_split_right = nullptr,
			/// Element indicating that the result should be \ref drag_split_type::split_top.
			*_split_up = nullptr,
			/// Element indicating that the result should be \ref drag_split_type::split_bottom.
			*_split_down = nullptr,
			/// Element indicating that the result should be \ref drag_split_type::combine.
			*_combine = nullptr;
		/// The current drag destination.
		drag_split_type _dest = drag_split_type::new_window;

		/// Initializes all destination indicators.
		void _initialize(std::u8string_view cls, const element_configuration &config) override {
			panel::_initialize(cls, config);

			get_manager().get_class_arrangements().get_or_default(cls).construct_children(*this, {
				{get_split_left_indicator_name(), _name_cast(_split_left)},
				{get_split_right_indicator_name(), _name_cast(_split_right)},
				{get_split_up_indicator_name(), _name_cast(_split_up)},
				{get_split_down_indicator_name(), _name_cast(_split_down)},
				{get_combine_indicator_name(), _name_cast(_combine)},
				});

			set_zindex(zindex::overlay);

			_setup_indicator(*_split_left, drag_split_type::split_left);
			_setup_indicator(*_split_right, drag_split_type::split_right);
			_setup_indicator(*_split_up, drag_split_type::split_top);
			_setup_indicator(*_split_down, drag_split_type::split_bottom);
			_setup_indicator(*_combine, drag_split_type::combine);
		}

		/// Initializes the given destination indicator.
		void _setup_indicator(element &elem, drag_split_type type) {
			elem.mouse_enter += [this, type]() {
				_dest = type;
			};
			elem.mouse_leave += [this]() {
				_dest = drag_split_type::new_window;
			};
		}
	};

	/// An element for displaying multiple tabs. It contains a ``tabs'' region for displaying the
	/// \ref tab_button "tab_buttons" of all available \ref tab "tabs" and a region that displays the currently
	/// selected tab.
	class host final : public panel {
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

			t.set_visibility(visibility::none);
			if (get_tab_count() == 1) {
				switch_tab(&t);
			}
		}
		/// Removes a \ref tab from this host by simply removing it from \ref _children. The rest are handled by
		/// \ref _on_child_removing.
		void remove_tab(tab &t) {
			_tab_contents_region->children().remove(t);
		}

		/// Switches the currently visible tab without changing the focus.
		void switch_tab(tab *t) {
			assert_true_logical(t == nullptr || t->logical_parent() == this, "the tab doesn't belong to this host");
			if (_active_tab) {
				_active_tab->set_visibility(visibility::none);
				_active_tab->_btn->set_zindex(0); // TODO a bit hacky
				_active_tab->_on_unselected();
			}
			_active_tab = t;
			if (_active_tab) {
				_active_tab->set_visibility(visibility::full);
				_active_tab->_btn->set_zindex(1);
				_active_tab->_on_selected();
			}
		}
		/// Switches the currently visible tab and sets the focus to that tab.
		void activate_tab(tab &t) {
			switch_tab(&t);
			get_manager().get_scheduler().set_focused_element(&t);
		}

		/// Moves the given tab before another specified tab. If the specified tab is \p nullptr, the tab is moved
		/// to the end of the tab list. If the moved tab was previously visible, it will remain visible after being
		/// moved.
		void move_tab_before(tab &target, tab *before) {
			_tab_contents_region->children().move_before(target, before);
			_tab_buttons_region->children().move_before(*target._btn, before == nullptr ? nullptr : before->_btn);
		}

		/// Returns the \ref panel that contains all tab buttons.
		panel &get_tab_buttons_region() const {
			return *_tab_buttons_region;
		}

		/// Returns the total number of tabs in the \ref host.
		std::size_t get_tab_count() const {
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
		inline static std::u8string_view get_default_class() {
			return u8"tab_host";
		}

		/// Returns the name identifier of the region that contains all tab buttons.
		inline static std::u8string_view get_tab_buttons_region_name() {
			return u8"tab_buttons_region";
		}
		/// Returns the name identifier of the region that contains tab contents.
		inline static std::u8string_view get_tab_contents_region_name() {
			return u8"tab_contents_region";
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
					switch_tab(nullptr);
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
					switch_tab(dynamic_cast<tab*>(*it));
				}
			}
		}
		/// Called when a \ref tab has been removed from \ref _tab_contents_region, to remove the associated
		/// \ref tab_button from \ref _tab_buttons_region.
		void _on_tab_removed(tab&);

		/// Initializes \ref _tab_buttons_region and \ref _tab_contents_region.
		void _initialize(std::u8string_view cls, const element_configuration &config) override {
			panel::_initialize(cls, config);

			get_manager().get_class_arrangements().get_or_default(cls).construct_children(*this, {
				{get_tab_buttons_region_name(), _name_cast(_tab_buttons_region)},
				{get_tab_contents_region_name(), _name_cast(_tab_contents_region)}
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
