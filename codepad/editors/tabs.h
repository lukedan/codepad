// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Structs for implementing the tab-based interface.

#include <deque>

#include "../os/misc.h"
#include "../os/current/all.h"
#include "../ui/element.h"
#include "../ui/element_classes.h"
#include "../ui/panel.h"
#include "../ui/common_elements.h"
#include "../ui/manager.h"
#include "../ui/renderer.h"
#include "../ui/window.h"

namespace codepad::editor {
	class tab;
	class tab_manager;

	/// Specifies the type of a tab's destination when being dragged.
	enum class drag_destination_type {
		new_window, ///< The tab will be moved to a new window.
		/// The tab has been added to a \ref tab_host, and the user is currently dragging and repositioning it in
		/// the tab list. After the user finishes dragging, it will remain at its place in the \ref tab_host.
		combine_in_tab,
		combine, ///< The tab will be added to a \ref tab_host.
		/// The current \ref tab_host will be split into two, with the original tabs on the right and the
		/// tab being dragged on the left.
		new_panel_left,
		/// The current \ref tab_host will be split into two, with the original tabs on the bottom and the
		/// tab being dragged on the top.
		new_panel_top,
		/// The current \ref tab_host will be split into two, with the original tabs on the left and the
		/// tab being dragged on the right.
		new_panel_right,
		/// The current \ref tab_host will be split into two, with the original tabs on the top and the
		/// tab being dragged on the bottom.
		new_panel_bottom
	};

	/// A panel with two regions separated by a draggable separator.
	class split_panel : public ui::panel_base {
	public:
		constexpr static double minimum_panel_size = 30.0; ///< The minimum size that a region can have.

		/// Sets the child that will be placed above or to the left of the separator.
		void set_child1(ui::element *elem) {
			_change_child(_c1, elem);
		}
		/// Returns the child that's currently above or to the left of the separator.
		ui::element *get_child1() const {
			return _c1;
		}
		/// Sets the child that will be placed below or to the right of the separator.
		void set_child2(ui::element *elem) {
			_change_child(_c2, elem);
		}
		/// Returns the child that's currently below or to the right of the separator.
		ui::element *get_child2() const {
			return _c2;
		}

		/// Returns the boundaries of the left/top region.
		rectd get_region1() const {
			rectd cln = get_client_region();
			return
				is_vertical() ?
				rectd(cln.xmin, cln.xmax, cln.ymin, _sep->get_layout().ymin) :
				rectd(cln.xmin, _sep->get_layout().xmin, cln.ymin, cln.ymax);
		}
		/// Returns the boundaries of the bottom/right region.
		rectd get_region2() const {
			rectd cln = get_client_region();
			return
				is_vertical() ?
				rectd(cln.xmin, cln.xmax, _sep->get_layout().ymax, cln.ymax) :
				rectd(_sep->get_layout().xmax, cln.xmax, cln.ymin, cln.ymax);
		}

		/// Returns the position of the separator, a number in the range of [0, 1].
		double get_separator_position() const {
			return _sep_position;
		}
		/// Sets the position of the separator.
		void set_separator_position(double pos) {
			double oldpos = _sep_position;
			_sep_position = std::clamp(pos, 0.0, 1.0);
			if (!_maintainpos) {
				double totw =
					is_vertical() ?
					get_client_region().height() - _sep->get_layout().height() :
					get_client_region().width() - _sep->get_layout().width();
				auto *sp = dynamic_cast<split_panel*>(_c1);
				if (sp && sp->is_vertical() == is_vertical()) {
					sp->_maintain_separator_position<false>(totw, oldpos, get_separator_position());
				}
				sp = dynamic_cast<split_panel*>(_c2);
				if (sp && sp->is_vertical() == is_vertical()) {
					sp->_maintain_separator_position<true>(totw, oldpos, get_separator_position());
				}
			}
			_invalidate_children_layout();
		}

		/// Returns the default class of all elements of type \ref split_panel.
		inline static str_t get_default_class() {
			return CP_STRLIT("split_panel");
		}

		/// Returns the role identifier of the separator.
		inline static str_t get_separator_role() {
			return CP_STRLIT("separator");
		}
	protected:
		ui::element
			/// The first child, displayed above or to the left of the separator.
			*_c1 = nullptr,
			/// The second child, displayed below or to the right of the separator.
			*_c2 = nullptr,
			/// The draggable separator.
			*_sep = nullptr;
		double
			/// The position of \ref _sep in this panel. This value should be between 0 and 1.
			_sep_position = 0.5,
			/// The offset to the mouse when the user drags the separator.
			_sep_offset = 0.0;
		bool
			/// Indicates when the position of \ref _sep is being set in \ref _maintain_separator_position to avoid
			/// infinite recursion.
			_maintainpos = false,
			/// Whether the user's dragging the separator.
			_sep_dragging = false;

		/// When this element is itself a child of a \ref split_panel with the same orientation, and the separator's
		/// position of the parent has changed, this function is called to make the element behave as if it is
		/// independent of its parent, i.e., to keep the global position of the draggable separator uncahnged.
		///
		/// \tparam MinChanged \p true if the position of the left/top boundary has changed, and \p false if that of
		///                    the right/bottom boundary has changed.
		/// \param ptotw The total width of the parent, not including that of its separator. This value is obtained
		///             before the layouts are updated.
		/// \param poldv The original position of the parent's separator.
		/// \param pnewv The updated position of the parent's separator.
		template <bool MinChanged> void _maintain_separator_position(double ptotw, double poldv, double pnewv) {
			vec2d sepsz = _sep->get_layout().size();
			double
				newpos,
				oldpos = get_separator_position(),
				padding = is_vertical() ? sepsz.y + get_padding().height() : sepsz.x + get_padding().width();
			double
				// the total width of the two regions, before and after the change
				omytotw, nmytotw,
				// the width of this region whose width won't change
				myfixw;
			// recalculate separator position
			if constexpr (MinChanged) {
				// (ptotw * (1 - poldv) - padding) * (1 - oldmv) = (ptotw * (1 - pnewv) - padding) * (1 - newmv)
				omytotw = ptotw * (1.0 - poldv) - padding;
				nmytotw = ptotw * (1.0 - pnewv) - padding;
				myfixw = omytotw * (1.0 - oldpos);
				newpos = 1.0 - myfixw / nmytotw;
			} else {
				// (ptotw * poldv - padding) * oldmv = (ptotw * pnewv - padding) * newmv
				omytotw = ptotw * poldv - padding;
				nmytotw = ptotw * pnewv - padding;
				myfixw = omytotw * oldpos;
				newpos = myfixw / nmytotw;
			}
			// the possibly affected child
			auto *sp = dynamic_cast<split_panel*>(MinChanged ? _c1 : _c2);
			if (sp && sp->is_vertical() == is_vertical()) {
				// must also be a split_panel with the same orientation
				// here we transform the positions so that it's as if this split_panel doesn't exist
				// for example, if MinChanged is true, we transform the positions from
				//   +------+--------------+
				//   |      |##############|
				//   |      |#+--+--+#    #|
				//   |      |#|  |  |#    #|
				//   |      |#+--+--+#    #|
				//   |      |##############|
				//   +------+--------------+,
				//   |----- ^ -------------|
				// where the frame marked by # is this element, to as if
				//   +------+--------+
				//   |      | +--+--+|
				//   |      | |  |  ||
				//   |      | +--+--+|
				//   +------+--------+,
				//   |----- ^ -------|
				// note how the space on the right is removed
				double neww = ptotw - padding - myfixw; // the width of the transformed region to be adjusted
				if constexpr (MinChanged) {
					sp->_maintain_separator_position<true>(neww, ptotw * poldv / neww, ptotw * pnewv / neww);
				} else {
					sp->_maintain_separator_position<false>(
						neww, omytotw * (1.0 - oldpos) / neww, nmytotw * (1.0 - newpos) / neww
						);
				}
			}
			// update position
			_maintainpos = true;
			set_separator_position(newpos);
			_maintainpos = false;
		}

		/// Changes the given child to the specified value.
		void _change_child(ui::element *&e, ui::element *newv) {
			if (e) {
				ui::element *old = e; // because e may be changed in _on_child_removed
				_children.remove(*old);
			}
			e = newv;
			if (e) {
				_child_set_logical_parent(*e, this);
				_children.add(*e);
			}
		}
		/// Sets the corresponding pointer to \p nullptr.
		void _on_child_removed(element &e) override {
			if (&e == _c1 || &e == _c2) {
				if (&e == _c1) {
					_c1 = nullptr;
				} else {
					_c2 = nullptr;
				}
			}
			panel_base::_on_child_removed(e);
		}

		/// Calls \ref invalidate_layout if the element's orientation has been changed, and updates the `vertical'
		/// state bit of \ref _sep accordingly.
		void _on_state_changed(value_update_info<ui::element_state_id> &p) override {
			panel_base::_on_state_changed(p);
			if (_has_any_state_bit_changed(get_manager().get_predefined_states().vertical, p)) {
				_sep->set_is_vertical(is_vertical());
				_invalidate_children_layout();
			}
		}

		/// Renders all children with additional clip regions.
		void _custom_render() override {
			_child_on_render(*_sep);
			ui::renderer_base &r = get_manager().get_renderer();
			if (_c1) {
				r.push_clip(get_region1().fit_grid_enlarge<int>());
				_child_on_render(*_c1);
				r.pop_clip();
			}
			if (_c2) {
				r.push_clip(get_region2().fit_grid_enlarge<int>());
				_child_on_render(*_c2);
				r.pop_clip();
			}
		}

		/// Updates the layout of all children.
		void _on_update_children_layout() override {
			rectd client = get_client_region();
			if (is_vertical()) {
				panel_base::layout_child_horizontal(*_sep, client.xmin, client.xmax);
				auto metrics = _sep->get_layout_height();
				double top = (client.height() - metrics.value) * _sep_position + client.ymin;
				_child_set_vertical_layout(*_sep, top, top + metrics.value);
			} else {
				panel_base::layout_child_vertical(*_sep, client.ymin, client.ymax);
				auto metrics = _sep->get_layout_width();
				double left = (client.width() - metrics.value) * _sep_position + client.xmin;
				_child_set_horizontal_layout(*_sep, left, left + metrics.value);
			}
			if (_c1) {
				panel_base::layout_child(*_c1, get_region1());
			}
			if (_c2) {
				panel_base::layout_child(*_c2, get_region2());
			}
		}

		/// Initializes \ref _sep and adds handlers for certain events.
		void _initialize(const str_t &cls, const ui::element_metrics &metrics) override {
			ui::panel_base::_initialize(cls, metrics);

			get_manager().get_class_arrangements().get_or_default(cls).construct_children(*this, {
				{get_separator_role(), _role_cast(_sep)}
				});

			_sep->set_can_focus(false);
			_sep->mouse_down += [this](ui::mouse_button_info &p) {
				if (p.button == ui::mouse_button::primary) {
					_sep_dragging = true;
					_sep_offset =
						is_vertical() ?
						p.position.y - _sep->get_layout().ymin :
						p.position.x - _sep->get_layout().xmin;
					get_window()->set_mouse_capture(*_sep);
				}
			};
			_sep->lost_capture += [this]() {
				_sep_dragging = false;
			};
			_sep->mouse_up += [this](ui::mouse_button_info &p) {
				if (_sep_dragging && p.button == ui::mouse_button::primary) {
					_sep_dragging = false;
					get_window()->release_mouse_capture();
				}
			};
			_sep->mouse_move += [this](ui::mouse_move_info &p) {
				if (_sep_dragging) {
					rectd client = get_client_region();
					double position =
						is_vertical() ?
						(p.new_position.y - _sep_offset - client.ymin) /
						(client.height() - _sep->get_layout().height()) :
						(p.new_position.x - _sep_offset - client.xmin) /
						(client.width() - _sep->get_layout().width());
					set_separator_position(position);
				}
			};
		}
	};


	/// Contains information about the user dragging a \ref tab_button.
	struct tab_drag_info {
		/// Initializes all fields of the struct.
		explicit tab_drag_info(vec2d df) : drag_diff(df) {
		}
		const vec2d drag_diff; ///< The offset of the top left corner of the \ref tab_button from the mouse cursor.
	};
	/// Contains information about the user clicking a \ref tab_button.
	struct tab_button_click_info {
		/// Initializes all fields of the struct.
		explicit tab_button_click_info(ui::mouse_button_info &i) : click_info(i) {
		}
		/// The \ref ui::mouse_button_info of the \ref ui::element::mouse_down event.
		ui::mouse_button_info &click_info;
	};
	/// A button representing a \ref tab in a \ref tab_host.
	class tab_button : public ui::panel_base {
		friend class tab_manager;
		friend class tab_host;
	public:
		/// The minimum distance the mouse cursor have to move before dragging starts.
		///
		/// \todo Combine different declarations and use system default.
		constexpr static double drag_pivot = 5.0;
		/// The default padding.
		///
		/// \todo Make this customizable.
		constexpr static ui::thickness content_padding = ui::thickness(5.0);

		/// Sets the label displayed on the button.
		void set_label(str_t str) {
			_label->content().set_text(std::move(str));
		}
		/// Returns the label curretly displayed on the button.
		const str_t &get_label() const {
			return _label->content().get_text();
		}

		/// Invoked when the ``close'' button is clicked, or when the user presses the tertiary mouse button on the
		/// \ref tab_button.
		event<void> request_close;
		event<tab_drag_info> start_drag; ///< Invoked when the user starts dragging the \ref tab_button.
		event<tab_button_click_info> click; ///< Invoked when the user clicks the \ref tab_button.

		/// Returns the default class of elements of type \ref tab_button.
		inline static str_t get_default_class() {
			return CP_STRLIT("tab_button");
		}

		/// Returns the role identifier of the label.
		inline static str_t get_label_role() {
			return CP_STRLIT("label");
		}
		/// Returns the role identifier of the `close' button.
		inline static str_t get_close_button_role() {
			return CP_STRLIT("close_button");
		}
	protected:
		ui::label *_label; ///< Used to display the tab's label.
		ui::button *_close_btn; ///< The `close' button.
		vec2d _mdpos; ///< The positon where the user presses the primary mouse button.
		/// The offset of the \ref tab_button in the ``tabs'' area.
		///
		/// \todo Use a more universal method to apply the offset.
		double _xoffset = 0.0;
		/// Indicates whether the user has pressed the primary mouse button when hovering over this element and may
		/// or may not start dragging.
		bool _predrag = false;

		/// Handles mouse button interactions.
		///
		/// \todo Make actions customizable.
		void _on_mouse_down(ui::mouse_button_info &p) override {
			if (
				p.button == ui::mouse_button::primary &&
				!_close_btn->is_mouse_over()
				) {
				_mdpos = p.position;
				_predrag = true;
				get_manager().get_scheduler().schedule_element_update(*this);
				click.invoke_noret(p);
			} else if (p.button == ui::mouse_button::tertiary) {
				request_close.invoke();
			}
			panel_base::_on_mouse_down(p);
		}

		/// Checks and starts dragging.
		void _on_update() override {
			panel_base::_on_update();
			if (_predrag) {
				if (os::is_mouse_button_down(ui::mouse_button::primary)) {
					vec2d diff =
						get_window()->screen_to_client(os::get_mouse_position()).convert<double>() - _mdpos;
					if (diff.length_sqr() > drag_pivot * drag_pivot) {
						_predrag = false;
						start_drag.invoke_noret(get_layout().xmin_ymin() - _mdpos);
					} else {
						get_manager().get_scheduler().schedule_element_update(*this);
					}
				} else {
					_predrag = false;
				}
			}
		}

		/// Initializes \ref _close_btn.
		void _initialize(const str_t &cls, const ui::element_metrics &metrics) override {
			panel_base::_initialize(cls, metrics);

			_can_focus = false;

			get_manager().get_class_arrangements().get_or_default(cls).construct_children(*this, {
				{get_label_role(), _role_cast(_label)},
				{get_close_button_role(), _role_cast(_close_btn)}
				});

			_close_btn->set_can_focus(false);
			_close_btn->click += [this]() {
				request_close.invoke();
			};
		}
	};

	/// Used to select the destimation of a \ref tab that's being dragged.
	class drag_destination_selector : public ui::panel_base {
	public:
		/// Returns the current \ref drag_destination_type.
		drag_destination_type get_drag_destination(vec2d) const {
			return _dest;
		}

		/// Returns the default class of elements of this type.
		inline static str_t get_default_class() {
			return CP_STRLIT("drag_destination_selector");
		}

		/// Returns the role identifier of the `split left' indicator.
		inline static str_t get_split_left_indicator_role() {
			return CP_STRLIT("split_left_indicator");
		}
		/// Returns the role identifier of the `split right' indicator.
		inline static str_t get_split_right_indicator_role() {
			return CP_STRLIT("split_right_indicator");
		}
		/// Returns the role identifier of the `split up' indicator.
		inline static str_t get_split_up_indicator_role() {
			return CP_STRLIT("split_up_indicator");
		}
		/// Returns the role identifier of the `split down' indicator.
		inline static str_t get_split_down_indicator_role() {
			return CP_STRLIT("split_down_indicator");
		}
		/// Returns the role identifier of the `combine' indicator.
		inline static str_t get_combine_indicator_role() {
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
		void _initialize(const str_t &cls, const ui::element_metrics &metrics) override {
			panel_base::_initialize(cls, metrics);

			get_manager().get_class_arrangements().get_or_default(cls).construct_children(*this, {
				{get_split_left_indicator_role(), _role_cast(_split_left)},
				{get_split_right_indicator_role(), _role_cast(_split_right)},
				{get_split_up_indicator_role(), _role_cast(_split_up)},
				{get_split_down_indicator_role(), _role_cast(_split_down)},
				{get_combine_indicator_role(), _role_cast(_combine)},
				});

			set_can_focus(false);
			set_zindex(ui::zindex::overlay);

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
	class tab_host final : public ui::panel_base {
		friend tab;
		friend tab_manager;
	public:
		/// Adds a \ref tab to the end of the tab list. If there were no tabs in the tab list prior to this
		/// operation, the newly added tab will be automatically activated.
		void add_tab(tab&);
		/// Removes a \ref tab from this host by simply removing it from \ref _children. The rest are handled by
		/// \ref _on_child_removing.
		void remove_tab(tab&);

		/// Switches the currently visible tab, but without changing the focus.
		void switch_tab(tab&);
		/// Switches the currently visible tab and sets the focus to that tab.
		void activate_tab(tab&);

		/// Returns the position of a given \ref tab in the tab list.
		size_t get_tab_position(tab&) const;
		/// Returns the \ref tab at the given position in the tab list.
		tab &get_tab_at(size_t) const;
		/// Moves the given tab before another specified tab. If the specified tab is \p nullptr, the tab is moved
		/// to the end of the tab list. If the moved tab was previously visible, it will remain visible after being
		/// moved.
		///
		/// \todo Make the tab regain focus if necessary.
		void move_tab_before(tab&, tab*);

		/// Returns the region that all tab buttons are in.
		rectd get_tab_buttons_region() const {
			return _tab_buttons_region->get_layout();
		}

		/// Returns the total number of tabs in the \ref tab_host.
		size_t tab_count() const {
			return _tabs.size();
		}

		/// Returns the manager of this tab.
		tab_manager &get_tab_manager() const {
			return *_tab_manager;
		}

		/// Returns the default class of elements of type \ref tab_host.
		inline static str_t get_default_class() {
			return CP_STRLIT("tab_host");
		}

		/// Returns the role identifier of the region that contains all tab buttons.
		inline static str_t get_tab_buttons_region_role() {
			return CP_STRLIT("tab_buttons_region");
		}
		/// Returns the role identifier of the region that contains tab contents.
		inline static str_t get_tab_contents_region_role() {
			return CP_STRLIT("tab_contents_region");
		}
	protected:
		ui::panel
			*_tab_buttons_region = nullptr, ///< The panel that contains all tab buttons.
			*_tab_contents_region = nullptr; ///< The panel that contains the contents of all tabs.

		std::list<tab*> _tabs; ///< The list of tabs.
		/// Iterator to the active tab. This can only be \p end() when there's no tab in \ref _tabs.
		std::list<tab*>::iterator _active_tab = _tabs.end();
		/// The \ref drag_destination_selector currently attached to this \ref tab_host.
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
		void _on_tab_removing(tab&);
		/// Called when a \ref tab has been removed from \ref _tab_contents_region, to remove the associated
		/// \ref tab_button and the corresponding entry in \ref _tabs.
		void _on_tab_removed(tab&);

		/// Initializes \ref _tab_buttons_region and \ref _tab_contents_region.
		void _initialize(const str_t&, const ui::element_metrics&) override;
	private:
		tab_manager *_tab_manager = nullptr; ///< The manager of this tab.
	};
	/// A tab that contains other elements.
	class tab : public ui::panel {
		friend tab_host;
		friend tab_manager;
	public:
		/// Sets the text displayed on the \ref tab_button.
		void set_label(str_t s) {
			_btn->set_label(std::move(s));
		}
		/// Returns the currently displayed text on the \ref tab_button.
		const str_t &get_label() const {
			return _btn->get_label();
		}

		/// Calls \ref tab_host::switch_tab to switch to this tab.
		void switch_to() {
			get_host()->switch_tab(*this);
		}
		/// Calls \ref tab_host::activate_tab to switch to and focus on this tab.
		void activate() {
			get_host()->activate_tab(*this);
		}
		/// Requests that this tab be closed. Derived classes should override \ref _on_close_requested to add
		/// additional behavior.
		void request_close() {
			_on_close_requested();
		}

		/// Returns the \ref tab_host that this tab is currently in.
		tab_host *get_host() const {
			return dynamic_cast<tab_host*>(logical_parent());
		}
		/// Returns the manager of this tab.
		tab_manager &get_tab_manager() const {
			return *_tab_manager;
		}

		/// Returns the default class of elements of type \ref tab.
		inline static str_t get_default_class() {
			return CP_STRLIT("tab");
		}
	protected:
		tab_button *_btn; ///< The \ref tab_button associated with tab.
		std::list<tab*>::iterator _text_tok; ///< Iterator to this tab in the \ref tab_host's tab list.

		/// Called when \ref request_close is called to handle the user's request of closing this tab. By default,
		/// this function removes this tab from the host, then marks this for disposal.
		virtual void _on_close_requested() {
			// also works without removing first, but this allows the window to check immediately if all tabs are
			// willing to close, and thus should always be performed with the next action.
			get_host()->remove_tab(*this);
			get_manager().get_scheduler().mark_for_disposal(*this);
		}

		/// Updates the state of \ref _btn.
		void _on_state_changed(value_update_info<ui::element_state_id> &info) override {
			panel::_on_state_changed(info);
			const auto &states = get_manager().get_predefined_states();
			ui::element_state_id concerned_states = states.focused | states.child_focused | states.selected;
			_btn->set_state((_btn->get_state() & ~concerned_states) | (get_state() & concerned_states));
		}

		/// Initializes \ref _btn.
		void _initialize(const str_t&, const ui::element_metrics&) override;
		/// Marks \ref _btn for disposal.
		void _dispose() override {
			get_manager().get_scheduler().mark_for_disposal(*_btn);
			ui::panel::_dispose();
		}
	private:
		tab_manager *_tab_manager = nullptr; ///< The manager of this tab.
	};

	/// Manages all \ref tab "tabs" and \ref tab_host "tab_hosts".
	class tab_manager {
		friend tab;
		friend tab_host;
	public:
		/// Constructor. Initializes \ref _possel and update tasks.
		tab_manager(ui::manager &man) : _manager(man) {
			_update_hosts_token = _manager.get_scheduler().register_update_task([this]() {
				update_changed_hosts();
			});
			_update_drag_token = _manager.get_scheduler().register_update_task([this]() {
				update_drag();
			});

			_possel = _manager.create_element<drag_destination_selector>();
		}
		/// Disposes \ref _possel, and unregisters update tasks.
		~tab_manager() {
			_manager.get_scheduler().mark_for_disposal(*_possel);

			_manager.get_scheduler().unregister_update_task(_update_drag_token);
			_manager.get_scheduler().unregister_update_task(_update_hosts_token);
		}

		/// Creates a new \ref tab in a \ref tab_host in the last focused \ref os::window_base. If there are no
		/// windows, a new one is created.
		tab *new_tab() {
			tab_host *host = nullptr;
			if (!_wndlist.empty()) {
				_enumerate_hosts(*_wndlist.begin(), [&host](tab_host &h) {
					host = &h;
					return false;
				});
			}
			return new_tab_in(host);
		}
		/// Creates a new \ref tab in the given \ref tab_host and returns it. If the given \ref tab_host is
		/// \p nullptr, a new window containing a new \ref tab_host will be created, in which the \ref tab will be
		/// created.
		tab *new_tab_in(tab_host *host = nullptr) {
			if (!host) {
				host = _new_tab_host();
				_new_window()->children().add(*host);
			}
			tab *t = _new_detached_tab();
			host->add_tab(*t);
			return t;
		}

		/// Returns the total number of windows managed by \ref tab_manager.
		size_t window_count() const {
			return _wndlist.size();
		}

		/// Returns \p true if there are no more \ref tab instances.
		bool empty() const {
			return window_count() == 0 && _drag == nullptr;
		}

		/// Sets the current \ref drag_destination_selector used among all \ref tab_host "tab_hosts".
		void set_drag_destination_selector(drag_destination_selector *sel) {
			if (_possel) {
				_manager.get_scheduler().mark_for_disposal(*_possel);
			}
			_possel = sel;
		}
		/// Returns the current \ref drag_destination_selector used among all \ref tab_host "tab_hosts".
		drag_destination_selector *get_drag_destination_selector() const {
			return _possel;
		}

		/// Splits the \ref tab_host the given \ref tab is in into two \ref tab_host "tab_hosts" in a
		/// \ref split_panel, and moves the given tab into the other \ref tab_host.
		///
		/// \param t The \ref tab.
		/// \param vertical If \p true, the new tab will be placed above or below old ones. Otherwise they'll be
		///                 placed side by side.
		/// \param newfirst If \p true, \p t will be placed in the top/left \ref tab_host while other remaining
		///                 tabs will be put in the bottom/right \ref tab_host.
		void split_tab(tab &t, bool vertical, bool newfirst) {
			tab_host *host = t.get_host();
			assert_true_usage(host != nullptr, "cannot split tab without host");
			_split_tab(*host, t, vertical, newfirst);
		}
		/// Creates a new \ref os::window and a \ref tab_host and moves the given tab into the newly created
		/// \ref tab_host. The size of the tab will be kept unchanged.
		void move_tab_to_new_window(tab &t) {
			rectd tglayout = t.get_layout();
			tab_host *hst = t.get_host();
			ui::window_base *wnd = t.get_window();
			if (hst != nullptr && wnd != nullptr) {
				tglayout = hst->get_layout().translated(wnd->get_position().convert<double>());
			}
			_move_tab_to_new_window(t, tglayout);
		}

		/// Updates all \ref tab_host "tab_hosts" whose tabs have been closed or moved. This is mainly intended to
		/// automatically merge empty tab hosts when they are emptied.
		void update_changed_hosts() {
			std::set<tab_host*> tmp_changes;
			std::swap(_changed, tmp_changes);
			while (!tmp_changes.empty()) {
				for (tab_host *host : tmp_changes) {
					if (host->tab_count() == 0) {
						if (auto father = dynamic_cast<split_panel*>(host->parent()); father) {
							// only merge when two empty hosts are side by side
							auto *other = dynamic_cast<tab_host*>(
								host == father->get_child1() ?
								father->get_child2() :
								father->get_child1()
								);
							if (other && other->tab_count() == 0) { // merge
								father->set_child1(nullptr);
								father->set_child2(nullptr);
								// use the other child to replace the split_panel
								auto ff = dynamic_cast<split_panel*>(father->parent());
								if (ff) {
									if (father == ff->get_child1()) {
										ff->set_child1(other);
									} else {
										assert_true_logical(father == ff->get_child2(), "corrupted element graph");
										ff->set_child2(other);
									}
								} else {
#ifdef CP_CHECK_LOGICAL_ERRORS
									auto f = dynamic_cast<ui::window_base*>(father->parent());
									assert_true_logical(f != nullptr, "parent of parent must be a window or a split panel");
#else
									auto f = static_cast<os::window_base*>(father->parent());
#endif
									f->children().remove(*father);
									f->children().add(*other);
								}
								_manager.get_scheduler().mark_for_disposal(*father);
								_changed.erase(host);
								_changed.emplace(other);
								_delete_tab_host(*host);
							}
						}
					}
				}
				tmp_changes.clear();
				std::swap(_changed, tmp_changes);
			}
		}
		/// Updates the tab that's currently being dragged.
		void update_drag();

		/// Returns \p true if the user's currently dragging a \ref tab.
		bool is_dragging_tab() const {
			return _drag != nullptr;
		}

		/// Starts dragging a given \ref tab.
		///
		/// \param t The tab to be dragged.
		/// \param diff The offset from the top left corner of the \ref tab_button to the mouse cursor.
		/// \param layout The layout of the \ref tab's main region.
		/// \param stop A callable object that returns \p true when the tab should be released.
		void start_drag_tab(tab &t, vec2d diff, rectd layout, std::function<bool()> stop = []() {
			return !os::is_mouse_button_down(ui::mouse_button::primary);
		}) {
			assert_true_usage(_drag == nullptr, "a tab is currently being dragged");
			tab_host *hst = t.get_host();
			if (hst) {
				_dest = hst;
				_dtype = drag_destination_type::combine_in_tab;
			} else {
				_dest = nullptr;
				_dtype = drag_destination_type::new_window;
			}
			_drag = &t;
			_dragdiff = diff;
			_dragrect = layout;
			_stopdrag = std::move(stop);
			_manager.get_scheduler().schedule_update_task(_update_drag_token);
		}
	protected:
		std::set<tab_host*> _changed; ///< The set of \ref tab_host "tab_hosts" whose children have changed.
		std::list<ui::window_base*> _wndlist; ///< The list of windows, ordered according to their z-indices.

		tab *_drag = nullptr; ///< The \ref tab that's currently being dragged.
		tab_host *_dest = nullptr; ///< The destination \ref tab_host of the \ref tab that's currently being dragged.
		drag_destination_type _dtype = drag_destination_type::new_window; ///< The type of the drag destination.
		vec2d _dragdiff; ///< The offset from the top left corner of the \ref tab_button to the mouse cursor.
		rectd _dragrect; ///< The boundaries of the main panel of \ref _drag, relative to the mouse cursor.
		std::function<bool()> _stopdrag; ///< The function used to determine when to stop dragging.
		ui::scheduler::update_task::token
			_update_hosts_token, ///< Token of the task that updates changed tab hosts.
			_update_drag_token; ///< Token of the task that updates the tab that's being dragged.
		/// The decoration for indicating where the tab will be if the user releases the primary mouse button.
		ui::decoration *_dragdec = nullptr;
		drag_destination_selector *_possel = nullptr; ///< The \ref drag_destination_selector.
		ui::manager &_manager; ///< The \ref ui::manager that manages all tabs.

		/// Creates a new window and registers necessary event handlers.
		ui::window_base *_new_window() {
			ui::window_base *wnd = _manager.create_element<os::window>();
			_wndlist.emplace_back(wnd);
			wnd->got_window_focus += [this, wnd]() {
				// there can't be too many windows... right?
				auto it = std::find(_wndlist.begin(), _wndlist.end(), wnd);
				assert_true_logical(it != _wndlist.end(), "window has been silently removed");
				_wndlist.erase(it);
				_wndlist.insert(_wndlist.begin(), wnd);
			};
			wnd->close_request += [this, wnd]() { // when requested to be closed, send request to all tabs
				_enumerate_hosts(wnd, [](tab_host &hst) {
					std::vector<tab*> ts(hst._tabs.begin(), hst._tabs.end());
					for (tab *t : ts) {
						t->_on_close_requested();
					}
				});
				update_changed_hosts(); // to ensure that empty hosts are merged
				if (wnd->children().items().size() == 1) {
					auto *host = dynamic_cast<tab_host*>(*wnd->children().items().begin());
					if (host && host->tab_count() == 0) {
						_delete_tab_host(*host); // just in case
						_delete_window(*wnd);
					}
				}
			};
			return wnd;
		}
		/// Deletes the given window managed by this \ref tab_manager. Use this instead of directly calling
		/// \ref scheduler::mark_for_disposal().
		void _delete_window(ui::window_base &wnd) {
			for (auto it = _wndlist.begin(); it != _wndlist.end(); ++it) {
				if (*it == &wnd) {
					_wndlist.erase(it);
					break;
				}
			}
			_manager.get_scheduler().mark_for_disposal(wnd);
		}
		/// Creates a new \ref tab instance not attached to any \ref tab_host. Use this instead of
		/// \ref ui::manager::create_element<tab>() so that the \ref tab is correctly registered to this manager.
		tab *_new_detached_tab() {
			tab *t = _manager.create_element<tab>();
			t->_tab_manager = this;
			return t;
		}
		/// Creates a new \ref tab_host instance. Use this instead of \ref ui::manager::create_element<tab_host>()
		/// so that the \ref tab_host is correctly registered to this manager.
		tab_host *_new_tab_host() {
			tab_host *h = _manager.create_element<tab_host>();
			h->_tab_manager = this;
			return h;
		}
		/// Prepares and marks a \ref tab_host for disposal. Use this instead of directly calling
		/// \ref scheduler::mark_for_disposal().
		void _delete_tab_host(tab_host &hst) {
			logger::get().log_info(CP_HERE, "tab host 0x", &hst, " disposed");
			if (_drag && _dest == &hst) {
				logger::get().log_info(CP_HERE, "resetting drag destination");
				_try_dispose_preview();
				_try_detach_possel();
				_dest = nullptr;
				_dtype = drag_destination_type::new_window;
			}
			_manager.get_scheduler().mark_for_disposal(hst);
		}
		/// Splits the given \ref tab_host into halves, and returns the resulting \ref split_panel. The original
		/// \ref tab_host will be removed from its parent.
		split_panel *_replace_with_split_panel(tab_host &hst) {
			split_panel *sp = _manager.create_element<split_panel>(), *f = dynamic_cast<split_panel*>(hst.parent());
			sp->set_can_focus(false);
			if (f) {
				if (f->get_child1() == &hst) {
					f->set_child1(sp);
				} else {
					assert_true_logical(f->get_child2() == &hst, "corrupted element tree");
					f->set_child2(sp);
				}
			} else {
				auto *w = dynamic_cast<ui::window_base*>(hst.parent());
				assert_true_logical(w != nullptr, "root element must be a window");
				w->children().remove(hst);
				w->children().add(*sp);
			}
			return sp;
		}

		/// Splits the given \ref tab_host into halves, moving the given tab to one half and all others to the
		/// other half.
		///
		/// \sa split_tab
		void _split_tab(tab_host &host, tab &t, bool vertical, bool newfirst) {
			if (t.get_host() == &host) {
				host.remove_tab(t);
			}
			split_panel *sp = _replace_with_split_panel(host);
			tab_host *th = _new_tab_host();
			if (newfirst) {
				sp->set_child1(th);
				sp->set_child2(&host);
			} else {
				sp->set_child1(&host);
				sp->set_child2(th);
			}
			th->add_tab(t);
			sp->set_is_vertical(vertical);
		}

		/// Moves the given \ref tab to a new window with the given layout, detaching it from its original parent.
		void _move_tab_to_new_window(tab &t, rectd layout) {
			tab_host *host = t.get_host();
			if (host != nullptr) {
				host->remove_tab(t);
			}
			ui::window_base *wnd = _new_window();
			tab_host *nhst = _new_tab_host();
			wnd->children().add(*nhst);
			nhst->add_tab(t);
			wnd->set_client_size(layout.size().convert<int>());
			wnd->set_position(layout.xmin_ymin().convert<int>());
		}

		/// Disposes \ref _dragdec if it isn't \p nullptr.
		void _try_dispose_preview() {
			if (_dragdec) {
				_dragdec->set_state(_manager.get_predefined_states().corpse);
				_dragdec = nullptr;
			}
		}
		/// Detaches \ref _possel from its parent if it has one.
		void _try_detach_possel() {
			if (_possel->parent() != nullptr) {
				assert_true_logical(_possel->parent() == _dest, "wrong parent for position selector");
				_dest->_set_drag_dest_selector(nullptr);
			}
		}

		/// Returns the layout of \ref _dragdec for the given \ref tab_host and \ref drag_destination_type.
		inline static rectd _get_preview_layout(const tab_host &th, drag_destination_type dtype) {
			rectd r = th.get_layout();
			switch (dtype) {
			case drag_destination_type::new_panel_left:
				r.xmax = r.centerx();
				break;
			case drag_destination_type::new_panel_top:
				r.ymax = r.centery();
				break;
			case drag_destination_type::new_panel_right:
				r.xmin = r.centerx();
				break;
			case drag_destination_type::new_panel_bottom:
				r.ymin = r.centery();
				break;
			default:
				break;
			}
			return r;
		}

		/// Iterates through all \ref tab_host "tab_hosts" in a given window, in a dfs-like fashion.
		///
		/// \param base The window.
		/// \param cb A callable object. It can either return \p void, or a \p bool indicating whether to continue.
		template <typename T> inline static void _enumerate_hosts(ui::window_base *base, T &&cb) {
			assert_true_logical(base->children().size() == 1, "window must have only one child");
			std::vector<ui::element*> hsts;
			hsts.push_back(*base->children().items().begin());
			while (!hsts.empty()) {
				ui::element *ce = hsts.back();
				hsts.pop_back();
				auto *hst = dynamic_cast<tab_host*>(ce);
				if (hst) {
					if constexpr (std::is_same_v<decltype(cb(*static_cast<tab_host*>(nullptr))), bool>) {
						if (!cb(*hst)) {
							break;
						}
					} else {
						cb(*hst);
					}
				} else {
					auto *sp = dynamic_cast<split_panel*>(ce);
					assert_true_logical(sp, "corrupted element tree");
					hsts.push_back(sp->get_child1());
					hsts.push_back(sp->get_child2());
				}
			}
		}

		/// Updates the position of \ref _drag by putting it before the right tab and setting the correct offset.
		///
		/// \param pos The position of the mouse cursor, relative to the area that contains all
		///            \ref tab_button "tab_buttons"
		/// \param maxw The width of the area that contains all \ref tab_button "tab_buttons".
		/// \todo Add tab button transform.
		/// \todo Support both horizontal and vertical tab lists.
		void _update_drag_tab_position(double pos, double maxw) const {
			double
				halfw = 0.5 * _drag->_btn->get_layout().width(),
				posx = pos + _dragdiff.x + halfw, cx = halfw;
			tab *res = nullptr;
			for (tab *t : _dest->_tabs) {
				if (t != _drag) {
					double thisw = t->_btn->get_layout().width();
					if (posx < cx + 0.5 * thisw) {
						res = t;
						break;
					}
					cx += thisw;
				}
			}
			_drag->_btn->_xoffset = std::clamp(posx, halfw, maxw - halfw) - cx;
			_dest->move_tab_before(*_drag, res);
		}

		/// Called when a \ref tab is removed from a \ref tab_host. Inserts the \ref tab_host to \ref _changed to
		/// update it afterwards, and schedules \ref update_changed_hosts() to be called.
		void _on_tab_detached(tab_host &host, tab&) {
			_changed.insert(&host);
			_manager.get_scheduler().schedule_update_task(_update_hosts_token);
		}
	};
}
