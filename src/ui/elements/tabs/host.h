// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of tab hosts.

#include "../../element.h"
#include "../../panel.h"
#include "../../manager.h"
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

		/// Adds \ref _split_left, \ref _split_right, \ref _split_up, \ref _split_down, and \ref _combine to the
		/// mapping.
		class_arrangements::notify_mapping _get_child_notify_mapping() override;

		/// Initializes all destination indicators.
		void _initialize(std::u8string_view cls, const element_configuration&) override;

		/// Initializes the given destination indicator.
		void _setup_indicator(element&, drag_split_type);
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
		void add_tab(tab&);
		/// Removes a \ref tab from this host by simply removing it from \ref _children. The rest are handled by
		/// \ref _on_child_removing.
		void remove_tab(tab &t) {
			_tab_contents_region->children().remove(t);
		}

		/// Switches the currently visible tab without changing the focus.
		void switch_tab(tab*);
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
		void _set_drag_dest_selector(drag_destination_selector*);

		/// Called when a \ref tab's being removed from \ref _tab_contents_region to change the currently active tab
		/// if necessary.
		///
		/// \todo Select a better tab when the active tab is removed.
		void _on_tab_removing(tab&);
		/// Called when a \ref tab has been removed from \ref _tab_contents_region, to remove the associated
		/// \ref tab_button from \ref _tab_buttons_region.
		void _on_tab_removed(tab&);

		/// Adds \ref _tab_buttons_region and \ref _tab_contents_region to the mapping.
		class_arrangements::notify_mapping _get_child_notify_mapping() override;

		/// Initializes \ref _tab_buttons_region and \ref _tab_contents_region.
		void _initialize(std::u8string_view cls, const element_configuration&) override;
	private:
		tab_manager *_tab_manager = nullptr; ///< The manager of this tab.
	};
}
