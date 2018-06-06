#pragma once

/// \file
/// Structs for implementing the tab-based interface.

#include "../os/misc.h"
#include "../os/renderer.h"
#include "../os/window.h"
#include "../os/current.h"
#include "../ui/element.h"
#include "../ui/element_classes.h"
#include "../ui/panel.h"
#include "../ui/common_elements.h"
#include "../ui/manager.h"

namespace codepad::editor {
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

	/// A separator in a \ref panel_base. Its layout is determined by its orientation and its position, which is in
	/// the range of [0, 1] and can be further restricted by \ref set_range. The user can use the mouse to drag this
	/// element and change its position within the allowed range.
	///
	/// \todo Make the size of \ref draggable_separator customizable.
	class draggable_separator : public ui::element {
		friend class ui::element;
	public:
		/// The default size of a \ref draggable_separator.
		constexpr static double default_thickness = 5.0;

		/// Sets its position. The position is automatically clamped into the range of [\ref _minv, \ref _maxv], the
		/// layout is updated, and \ref value_changed is then invoked.
		void set_position(double v) {
			double ov = _posv;
			_posv = std::clamp(v, _minv, _maxv);
			if (_orient == ui::orientation::horizontal) {
				set_margin(ui::thickness(_posv, 0.0, 1.0 - _posv, 0.0));
			} else {
				set_margin(ui::thickness(0.0, _posv, 0.0, 1.0 - _posv));
			}
			value_changed.invoke_noret(ov);
		}
		/// Returns the current position of the separator.
		double get_position() const {
			return _posv;
		}

		/// Returns the minimum allowed position.
		double get_range_min() const {
			return _minv;
		}
		/// Returns the maximum allowed position.
		double get_range_max() const {
			return _maxv;
		}
		/// Sets the range that position is allowed to be in. If the current position is not in this range,
		/// \ref set_position is called to make sure.
		void set_range(double rmin, double rmax) {
			_minv = rmin;
			_maxv = rmax;
			if (_posv < _minv || _posv > _maxv) {
				set_position(_posv);
			}
		}

		/// Sets the orientation of this separator. If it's \ref ui::orientation::horizontal, its parent will be
		/// split into the left part and the right part; otherwise, if it's \ref ui::orientation::vertical, the
		/// the parent will be split into the top part and the bottom part.
		void set_orientation(ui::orientation ori) {
			_orient = ori;
			_on_orient_changed();
		}
		/// Returns the current orientation of this separator.
		ui::orientation get_orientation() const {
			return _orient;
		}

		/// Shows the appropriate cursor that indicates the user can change its position by dragging.
		os::cursor get_default_cursor() const override {
			return
				_orient == ui::orientation::horizontal ?
				os::cursor::arrow_east_west :
				os::cursor::arrow_north_south;
		}

		/// Sets the default width of the separator.
		vec2d get_desired_size() const override {
			return vec2d(default_thickness, default_thickness);
		}

		/// Returns the boundaries of the left/top region separated by this element.
		rectd get_region1() const {
			rectd plo = _parent->get_client_region();
			return
				_orient == ui::orientation::horizontal ?
				rectd(plo.xmin, _layout.xmin, plo.ymin, plo.ymax) :
				rectd(plo.xmin, plo.xmax, plo.ymin, _layout.ymin);
		}
		/// Returns the boundaries of the right/bottom region separated by this element.
		rectd get_region2() const {
			rectd plo = _parent->get_client_region();
			return
				_orient == ui::orientation::horizontal ?
				rectd(_layout.xmax, plo.xmax, plo.ymin, plo.ymax) :
				rectd(plo.xmin, plo.xmax, _layout.ymax, plo.ymax);
		}

		event<value_update_info<double>> value_changed; ///< Invoked when the position of the separator has changed.
		event<void>
			start_drag, ///< Invoked when the user starts dragging the separator.
			stop_drag; ///< Invoked when the user stops dragging the separator.

		/// Returns the default class of all elements of type \ref draggable_separator.
		inline static str_t get_default_class() {
			return CP_STRLIT("draggable_separator");
		}
	protected:
		ui::orientation _orient = ui::orientation::horizontal; ///< The orientation of the separator.
		double
			_posv = 0.5, ///< The position of the separator, in the range of [\ref _minv, \ref _maxv].
			_minv = 0.0, ///< The minimum allowed position of the separator.
			_maxv = 1.0; ///< The maxnimum allowed position of the separator.
		bool _drag = false; ///< Indicates whether the user is dragging the separator.

		/// Updates the element's anchor and margin according to \ref _orient.
		void _on_orient_changed() {
			if (_orient == ui::orientation::horizontal) {
				set_anchor(ui::anchor::stretch_vertically);
				set_margin(ui::thickness(_posv, 0.0, 1.0 - _posv, 0.0));
			} else {
				set_anchor(ui::anchor::stretch_horizontally);
				set_margin(ui::thickness(0.0, _posv, 0.0, 1.0 - _posv));
			}
		}

		/// Starts dragging the separator if the primary mouse button is pressed.
		void _on_mouse_down(ui::mouse_button_info &p) override {
			if (p.button == os::input::mouse_button::primary) {
				start_drag.invoke();
				_drag = true;
				get_window()->set_mouse_capture(*this);
			}
			element::_on_mouse_down(p);
		}
		/// Moves the separator if it's being dragged.
		void _on_mouse_move(ui::mouse_move_info &p) override {
			if (_drag) {
				rectd clnrgn = parent()->get_client_region();
				vec2d sz = get_actual_size();
				set_position(
					_orient == ui::orientation::horizontal ?
					(p.new_pos.x - clnrgn.xmin - 0.5 * sz.x) / (clnrgn.width() - sz.x) :
					(p.new_pos.y - clnrgn.ymin - 0.5 * sz.y) / (clnrgn.height() - sz.y)
				);
			}
		}
		/// Called when the user stops dragging the separator.
		void _on_end_drag() {
			_drag = false;
			get_window()->release_mouse_capture();
		}
		/// Calls \ref _on_end_drag to stop dragging.
		void _on_capture_lost() override {
			_on_end_drag();
		}
		/// Calls \ref _on_end_drag to stop dragging if the primary mouse is released when dragging.
		void _on_mouse_up(ui::mouse_button_info &p) override {
			if (_drag && p.button == os::input::mouse_button::primary) {
				_on_end_drag();
			}
		}

		/// Sets \ref _can_focus to \p false and calls \ref _on_orient_changed to initialize this element's
		/// margin and anchor.
		void _initialize() override {
			ui::element::_initialize();
			_can_focus = false;
			_on_orient_changed();
		}
	};
	/// A panel with two regions separated by a \ref draggable_separator.
	class split_panel : public ui::panel_base {
	public:
		constexpr static double minimum_panel_size = 30.0; ///< The minimum size that a region can have.

		/// Sets the child that will be placed in the region obtained from \ref draggable_separator::get_region1.
		void set_child1(ui::element *elem) {
			_change_child(_c1, elem);
		}
		/// Returns the child that's currently in the region obtained from \ref draggable_separator::get_region1.
		ui::element *get_child1() const {
			return _c1;
		}
		/// Sets the child that will be placed in the region obtained from \ref draggable_separator::get_region2.
		void set_child2(ui::element *elem) {
			_change_child(_c2, elem);
		}
		/// Returns the child that's currently in the region obtained from \ref draggable_separator::get_region2.
		ui::element *get_child2() const {
			return _c2;
		}

		/// Sets the orientation of the \ref draggable_separator.
		void set_orientation(ui::orientation ori) {
			_sep->set_orientation(ori);
		}
		/// Returns the orientation of the \ref draggable_separator.
		ui::orientation get_orientation() const {
			return _sep->get_orientation();
		}

		/// Overrides the layout of the two children.
		bool override_children_layout() const override {
			return true;
		}

		/// Returns the default class of all elements of type \ref split_panel.
		inline static str_t get_default_class() {
			return CP_STRLIT("split_panel");
		}
	protected:
		ui::element
			/// The first child, displayed in the region obtained from \ref draggable_separator::get_region1.
			*_c1 = nullptr,
			/// The second child, displayed in the region obtained from \ref draggable_separator::get_region2.
			*_c2 = nullptr;
		draggable_separator *_sep = nullptr; ///< The separator.
		/// Indicates when the position of \ref _sep is being set in \ref _maintain_separator_position to avoid
		/// infinite recursion.
		bool _maintainpos = false;

		/// When this element is itself a child of a \ref split_panel with the same orientation, and the separator's
		/// position of the parent has changed, this function is called to make the element behave as if it is
		/// independent of its parent, i.e., to keep the global position of the \ref draggable_separator uncahnged.
		///
		/// \tparam MinChanged \p true if the position of the left/top boundary has changed, and \p false if that of
		///                    the right/bottom boundary has changed.
		/// \param ptotw The total width of the parent, not including that of its separator. This value is obtained
		///             before the layouts are updated.
		/// \param poldv The original position of the parent's separator.
		/// \param pnewv The updated position of the parent's separator.
		template <bool MinChanged> void _maintain_separator_position(double ptotw, double poldv, double pnewv) {
			vec2d sepsz = _sep->get_actual_size();
			double
				newpos,
				oldpos = _sep->get_position(),
				padding =
				_sep->get_orientation() == ui::orientation::horizontal ?
				sepsz.x + get_padding().width() :
				sepsz.y + get_padding().height();
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
			split_panel *sp = dynamic_cast<split_panel*>(MinChanged ? _c1 : _c2);
			if (sp && sp->get_orientation() == _sep->get_orientation()) {
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
			_sep->set_position(newpos);
			_maintainpos = false;
		}
		/// Recalculates the range that the \ref draggable_separator can be in.
		///
		/// \todo Still inaccurate. Needs mechanics such as ``min_size''.
		void _reset_separator_range() {
			element *c1 = _c1, *c2 = _c2;
			for (
				split_panel *next = dynamic_cast<split_panel*>(c1);
				next;
				c1 = next->get_child2(), next = dynamic_cast<split_panel*>(c1)
				) {
			}
			for (
				split_panel *next = dynamic_cast<split_panel*>(c2);
				next;
				c2 = next->get_child1(), next = dynamic_cast<split_panel*>(c2)
				) {
			}
			double minv, maxv, lmin, lw;
			if (_sep->get_orientation() == ui::orientation::horizontal) {
				minv = c1->get_layout().xmin;
				maxv = c2->get_layout().xmax;
				lmin = get_layout().xmin;
				lw = get_layout().width();
			} else {
				minv = c1->get_layout().ymin;
				maxv = c2->get_layout().ymax;
				lmin = get_layout().ymin;
				lw = get_layout().height();
			}
			minv += minimum_panel_size;
			maxv -= minimum_panel_size;
			if (minv > maxv) {
				minv = maxv = 0.5 * (minv + maxv);
			}
			_sep->set_range((minv - lmin) / lw, (maxv - lmin) / lw);
		}

		/// Changes the given child to the specified value.
		void _change_child(ui::element *&e, ui::element *newv) {
			if (e) {
				_children.remove(*e);
			}
			e = newv;
			if (e) {
				_children.add(*e);
			}
		}
		/// Sets the corresponding pointer to \p nullptr.
		void _on_child_removed(element *e) override {
			if (e == _c1) {
				_c1 = nullptr;
			} else if (e == _c2) {
				_c2 = nullptr;
			}
		}

		/// Renders all children with additional clip regions.
		void _custom_render() override {
			_child_on_render(_sep);
			if (_c1) {
				os::renderer_base::get().push_clip(_sep->get_region1().fit_grid_enlarge<int>());
				_child_on_render(_c1);
				os::renderer_base::get().pop_clip();
			}
			if (_c2) {
				os::renderer_base::get().push_clip(_sep->get_region2().fit_grid_enlarge<int>());
				_child_on_render(_c2);
				os::renderer_base::get().pop_clip();
			}
		}

		/// Updates the layout of all children.
		void _finish_layout() override {
			_child_recalc_layout(_sep, get_client_region());
			if (_c1) {
				_child_recalc_layout(_c1, _sep->get_region1());
			}
			if (_c2) {
				_child_recalc_layout(_c2, _sep->get_region2());
			}
			ui::panel_base::_finish_layout();
		}

		/// Initializes \ref _sep and adds handlers for certain events.
		///
		/// \todo Figure out why is the handler for stop_drag written like that.
		void _initialize() override {
			ui::panel_base::_initialize();
			_sep = ui::element::create<draggable_separator>();
			_sep->value_changed += [this](value_update_info<double> &p) {
				if (!_maintainpos) {
					double totw =
						_sep->get_orientation() == ui::orientation::horizontal ?
						get_client_region().width() - _sep->get_actual_size().x :
						get_client_region().height() - _sep->get_actual_size().y;
					split_panel *sp = dynamic_cast<split_panel*>(_c1);
					if (sp && sp->get_orientation() == _sep->get_orientation()) {
						sp->_maintain_separator_position<false>(totw, p.old_value, _sep->get_position());
					}
					sp = dynamic_cast<split_panel*>(_c2);
					if (sp && sp->get_orientation() == _sep->get_orientation()) {
						sp->_maintain_separator_position<true>(totw, p.old_value, _sep->get_position());
					}
				}
				invalidate_layout();
			};
			_sep->start_drag += [this]() {
				_reset_separator_range();
			};
			_sep->stop_drag += [this]() {
				_sep->set_range(0.0, 1.0);
			};
			_children.add(*_sep);
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

		/// Sets the text displayed on the button.
		void set_text(str_t str) {
			_content.set_text(std::move(str));
		}
		/// Returns the text curretly displayed on the button.
		const str_t &get_text() const {
			return _content.get_text();
		}

		/// Returns the desired size.
		///
		/// \todo Current method is hacky.
		vec2d get_desired_size() const override {
			vec2d sz = _content.get_text_size() + _padding.size();
			sz.x += sz.y;
			return sz;
		}

		/// Returns the height of a \ref tab_button.
		///
		/// \todo Remove this.
		inline static double get_tab_button_area_height() {
			return ui::content_host::get_default_font()->height() + tab_button::content_padding.height();
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
		/// Returns the default class of the ``close'' button of a \ref tab_button.
		inline static str_t get_tab_close_button_class() {
			return CP_STRLIT("tab_close_button");
		}
	protected:
		ui::content_host _content{*this}; ///< Used to display text on the button.
		ui::button *_btn; ///< The ``close'' button.
		vec2d _mdpos; ///< The positon where the user presses the primary mouse button.
		/// The offset of the \ref tab_button in the ``tabs'' area.
		///
		/// \todo Use a more universal method to apply the offset.
		double _xoffset = 0.0;

		/// Handles mouse button interactions.
		///
		/// \todo Make actions customizable.
		void _on_mouse_down(ui::mouse_button_info &p) override {
			if (
				p.button == os::input::mouse_button::primary &&
				!_btn->is_mouse_over()
				) {
				_mdpos = p.position;
				ui::manager::get().schedule_update(*this);
				click.invoke_noret(p);
			} else if (p.button == os::input::mouse_button::tertiary) {
				request_close.invoke();
			}
			panel_base::_on_mouse_down(p);
		}

		/// Checks and starts dragging.
		void _on_update() override {
			if (os::input::is_mouse_button_down(os::input::mouse_button::primary)) {
				vec2d diff = get_window()->screen_to_client(os::input::get_mouse_position()).convert<double>() - _mdpos;
				if (diff.length_sqr() > drag_pivot * drag_pivot) {
					start_drag.invoke_noret(get_layout().xmin_ymin() - _mdpos);
				} else {
					ui::manager::get().schedule_update(*this);
				}
			}
		}
		/// Renders \ref _content.
		void _custom_render() override {
			_content.render();
			panel_base::_custom_render();
		}

		/// Sets the width of \ref _btn.
		///
		/// \todo Current method is hacky.
		void _finish_layout() override {
			_btn->set_width(get_layout().height() - get_padding().height());
			panel_base::_finish_layout();
		}

		/// Initializes \ref _btn.
		void _initialize() override {
			panel_base::_initialize();
			_btn = ui::element::create<ui::button>();
			_btn->set_class(get_tab_close_button_class());
			_btn->set_anchor(ui::anchor::dock_right);
			_btn->set_can_focus(false);
			_btn->click += [this]() {
				request_close.invoke();
			};
			_children.add(*_btn);
			_padding = content_padding;
			_can_focus = false;
		}
	};

	/// Base class of elements used to select the destimation of a \ref tab that's being dragged.
	class drag_destination_selector : public ui::element {
	public:
		/// Returns the corresponding \ref drag_destination_type given the mouse's position. This function must not
		/// return \ref drag_destination_type::combine_in_tab.
		virtual drag_destination_type get_drag_destination(vec2d) const = 0;
	};

	class tab;
	/// An element for displaying multiple tabs. It contains a ``tabs'' region for displaying the
	/// \ref tab_button "tab_buttons" of all available \ref tab "tabs" and a region that displays the currently
	/// selected tab.
	///
	/// \todo Override \ref _on_child_removed to update tab list when a tab is disposed.
	class tab_host : public ui::panel_base {
		friend class tab;
		friend class tab_manager;
	public:
		/// This element overrides the layout of its children.
		bool override_children_layout() const override {
			return true;
		}

		/// Adds a \ref tab to the end of the tab list. If there were no tabs in the tab list prior to this
		/// operation, the newly added tab will be automatically activated.
		void add_tab(tab&);
		/// Removes the given \ref tab from this \ref tab_host. If the tab to remove is currently visible, the
		/// visible tab is automatically changed.
		///
		/// \todo Select a better tab when the active tab is disposed.
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

		/// Returns the region in which \ref tab_button "tab_buttons" are displayed.
		rectd get_tab_button_region() const {
			return rectd(
				_layout.xmin, _layout.xmax, _layout.ymin,
				_layout.ymin + ui::content_host::get_default_font()->height() + tab_button::content_padding.height()
			);
		}

		/// Returns the total number of tabs in the \ref tab_host.
		size_t tab_count() const {
			return _tabs.size();
		}

		/// Returns the default class of elements of type \ref tab_host.
		inline static str_t get_default_class() {
			return CP_STRLIT("tab_host");
		}
	protected:
		std::list<tab*> _tabs; ///< The list of tabs.
		/// Iterator to the active tab. This can only be \p end() when there's no tab in \ref _tabs.
		std::list<tab*>::iterator _active_tab = _tabs.end();
		/// The \ref drag_destination_selector associated with this \ref tab_host.
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

		/// Calculates the layout of all \ref tab "tabs".
		void _finish_layout() override;
	};
	/// A tab that contains other elements.
	class tab : public ui::panel {
		friend class tab_host;
		friend class tab_manager;
	public:
		/// Sets the text displayed on the \ref tab_button.
		void set_caption(str_t s) {
			_btn->set_text(std::move(s));
		}
		/// Returns the currently displayed text on the \ref tab_button.
		const str_t &get_caption() const {
			return _btn->get_text();
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
#ifdef CP_DETECT_LOGICAL_ERRORS
			tab_host *hst = dynamic_cast<tab_host*>(parent());
			assert_true_logical(
				(hst == nullptr) == (parent() == nullptr), "parent is not a tab host when get_host() is called"
			);
			return hst;
#else
			return static_cast<tab_host*>(parent());
#endif
		}

		/// Returns the default class of elements of type \ref tab.
		inline static str_t get_default_class() {
			return CP_STRLIT("tab");
		}
	protected:
		tab_button * _btn; ///< The \ref tab_button associated with tab.
		std::list<tab*>::iterator _text_tok; ///< Iterator to this tab in the \ref tab_host's tab list.

		/// Detaches the tab from the \ref tab_host, and marks the tab for disposal.
		///
		/// \todo Remove this function and use \ref ui::manager::mark_disposal directly instead; let
		///       \ref tab_host handle the removal of this tab.
		void _detach_and_dispose() {
			get_host()->remove_tab(*this);
			ui::manager::get().mark_disposal(*this);
		}
		/// Called when \ref request_close is called to handle the user's request of closing this tab. Calls
		/// \ref _detach_and_dispose by default.
		virtual void _on_close_requested() {
			_detach_and_dispose();
		}

		/// Initializes \ref _btn.
		void _initialize() override;
		/// Marks \ref _btn for disposal.
		void _dispose() override {
			ui::manager::get().mark_disposal(*_btn);
			ui::panel::_dispose();
		}
	};

	/// A \ref drag_destination_selector similar to that of Visual Studio.
	///
	/// \todo Add customizable appearance.
	/// \todo Document this class when it's finished.
	class grid_drag_destination_selector : public drag_destination_selector {
	public:
		struct region_metrics {
			double
				width_left = 30.0, width_center = 30.0, width_right = 30.0,
				height_top = 30.0, height_center = 30.0, height_bottom = 30.0;
		};
		const region_metrics &get_region_metrics() const {
			return _met;
		}
		void set_region_metrics(const region_metrics &rm) {
			_met = rm;
			invalidate_visual();
		}

		drag_destination_type get_drag_destination(vec2d mouse) const override {
			bool
				xin = mouse.x > _inner.xmin && mouse.x < _inner.xmax,
				yin = mouse.y > _inner.ymin && mouse.y < _inner.ymax;
			if (xin && yin) {
				return drag_destination_type::combine;
			}
			if (yin) {
				if (mouse.x < _inner.centerx()) {
					if (mouse.x > _outer.xmin) {
						return drag_destination_type::new_panel_left;
					}
				} else {
					if (mouse.x < _outer.xmax) {
						return drag_destination_type::new_panel_right;
					}
				}
			} else if (xin) {
				if (mouse.y < _inner.centery()) {
					if (mouse.y > _outer.ymin) {
						return drag_destination_type::new_panel_top;
					}
				} else {
					if (mouse.y < _outer.ymax) {
						return drag_destination_type::new_panel_bottom;
					}
				}
			}
			return drag_destination_type::new_window;
		}

		inline static str_t get_default_class() {
			return CP_STRLIT("grid_drag_destination_selector");
		}
	protected:
		region_metrics _met;
		rectd _inner, _outer;

		void _finish_layout() override {
			drag_destination_selector::_finish_layout();
			rectd r = get_layout();
			_outer = _inner = rectd::from_xywh(
				r.centerx() - _met.width_center * 0.5, r.centery() - _met.height_center * 0.5,
				_met.width_center, _met.height_center
			);
			_outer.xmin -= _met.width_left;
			_outer.ymin -= _met.height_top;
			_outer.xmax += _met.width_right;
			_outer.ymax += _met.height_bottom;
		}

		void _custom_render() override {
			ui::render_batch batch;
			batch.add_quad(_inner, rectd(), colord(1.0, 1.0, 0.0, 0.5));
			batch.add_quad(rectd(_outer.xmin, _inner.xmin, _inner.ymin, _inner.ymax), rectd(), colord(0.0, 1.0, 0.0, 0.5));
			batch.add_quad(rectd(_inner.xmax, _outer.xmax, _inner.ymin, _inner.ymax), rectd(), colord(0.0, 1.0, 0.0, 0.5));
			batch.add_quad(rectd(_inner.xmin, _inner.xmax, _outer.ymin, _inner.ymin), rectd(), colord(0.0, 1.0, 0.0, 0.5));
			batch.add_quad(rectd(_inner.xmin, _inner.xmax, _inner.ymax, _outer.ymax), rectd(), colord(0.0, 1.0, 0.0, 0.5));
			batch.draw(os::texture());
		}
	};
	/// Manages all \ref tab "tabs" and \ref tab_host "tab_hosts".
	class tab_manager {
		friend class tab;
		friend class tab_host;
	public:
		/// Constructor. Initializes \ref _possel.
		tab_manager() {
			_possel = ui::element::create<grid_drag_destination_selector>();
		}
		/// Disposes \ref _possel.
		~tab_manager() {
			ui::manager::get().mark_disposal(*_possel);
		}

		/// Creates a new \ref tab in a \ref tab_host in the last focused \ref os::window_base. If there are no
		/// windows, a new one is created.
		tab *new_tab() {
			tab_host *host = nullptr;
			if (_wndlist.size() > 0) {
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
				host = ui::element::create<tab_host>();
				_new_window()->children().add(*host);
			}
			tab *t = ui::element::create<tab>();
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
				ui::manager::get().mark_disposal(*_possel);
			}
			_possel = sel;
			if (_possel) {
				_possel->set_zindex(ui::element::max_zindex);
			}
		}
		/// Returns the current \ref drag_destination_selector used among all \ref tab_host "tab_hosts".
		drag_destination_selector *get_drag_destination_selector() const {
			return _possel;
		}

		/// Splits the \ref tab_host the given \ref tab is in into two \ref tab_host "tab_hosts" in a
		/// \ref split_panel, and moves the given tab into the other \ref tab_host.
		///
		/// \param t The \ref tab.
		/// \param orient The orientation of the new \ref split_panel.
		/// \param newfirst If \p true, \p t will be placed in the top/left \ref tab_host while other remaining
		///                 tabs will be put in the bottom/right \ref tab_host.
		void split_tab(tab &t, ui::orientation orient, bool newfirst) {
			tab_host *host = t.get_host();
			assert_true_usage(host != nullptr, "cannot split tab without host");
			_split_tab(*host, t, orient, newfirst);
		}
		/// Creates a new \ref os::window and a \ref tab_host and moves the given tab into the newly created
		/// \ref tab_host. The size of the tab will be kept unchanged.
		void move_tab_to_new_window(tab &t) {
			rectd tglayout = t.get_layout();
			tab_host *hst = t.get_host();
			os::window_base *wnd = t.get_window();
			if (hst != nullptr && wnd != nullptr) {
				tglayout = hst->get_layout().translated(wnd->get_position().convert<double>());
			}
			_move_tab_to_new_window(t, tglayout);
		}

		/// Updates all \ref tab_host "tab_hosts" whose tabs have been changed. All empty tab hosts will be removed,
		/// and empty windows will be closed.
		void update_changed_hosts() {
			for (auto i = _changed.begin(); i != _changed.end(); ++i) {
				if ((*i)->tab_count() == 0) {
					_on_tab_host_disposed(**i);
					auto father = dynamic_cast<split_panel*>((*i)->parent());
					if (father) {
						ui::element *other = (*i) == father->get_child1() ? father->get_child2() : father->get_child1();
						father->set_child1(nullptr);
						father->set_child2(nullptr);
						auto ff = dynamic_cast<split_panel*>(father->parent());
						if (ff) {
							if (father == ff->get_child1()) {
								ff->set_child1(other);
							} else {
								assert_true_logical(father == ff->get_child2(), "corrupted element graph");
								ff->set_child2(other);
							}
						} else {
#ifdef CP_DETECT_LOGICAL_ERRORS
							auto f = dynamic_cast<os::window_base*>(father->parent());
							assert_true_logical(f != nullptr, "parent of parent must be a window or a split panel");
#else
							auto f = static_cast<os::window_base*>(father->parent());
#endif
							f->children().remove(*father);
							f->children().add(*other);
						}
						ui::manager::get().mark_disposal(*father);
					} else {
#ifdef CP_DETECT_LOGICAL_ERRORS
						auto f = dynamic_cast<os::window_base*>((*i)->parent());
						assert_true_logical(f != nullptr, "parent must be a window or a split panel");
#else
						auto f = static_cast<os::window_base*>((*i)->parent());
#endif
						for (auto it = _wndlist.begin(); it != _wndlist.end(); ++it) {
							if (*it == f) {
								_wndlist.erase(it);
								break;
							}
						}
						ui::manager::get().mark_disposal(*f);
					}
					ui::manager::get().mark_disposal(**i);
				}
			}
			_changed.clear();
		}
		/// Updates the tab that's currently being dragged.
		void update_drag();
		/// Calls \ref update_changed_hosts and \ref update_drag to perform necessary updating.
		void update() {
			update_changed_hosts();
			update_drag();
		}

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
			return !os::input::is_mouse_button_down(os::input::mouse_button::primary);
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
		}

		/// Returns the global \ref tab_manager.
		static tab_manager &get();
	protected:
		std::set<tab_host*> _changed; ///< The set of \ref tab_host "tab_hosts" whose children have changed.
		std::list<os::window_base*> _wndlist; ///< The list of windows, ordered according to their z-indices.

		tab *_drag = nullptr; ///< The \ref tab that's currently being dragged.
		tab_host *_dest = nullptr; ///< The destination \ref tab_host of the \ref tab that's currently being dragged.
		drag_destination_type _dtype = drag_destination_type::new_window; ///< The type of the drag destination.
		vec2d _dragdiff; ///< The offset from the top left corner of the \ref tab_button to the mouse cursor.
		rectd _dragrect; ///< The boundaries of the main panel of \ref _drag, relative to the mouse cursor.
		std::function<bool()> _stopdrag; ///< The function used to determine when to stop dragging.
		/// The decoration for indicating where the tab will be if the user releases the primary mouse button.
		ui::decoration *_dragdec = nullptr;
		drag_destination_selector *_possel = nullptr; ///< The \ref drag_destination_selector.

		/// Used to maintain the focused element when tabs are detached and re-attached.
		struct _keep_intab_focus {
		public:
			/// Constructor.
			///
			/// \param t The tab that's about to be moved. If the currently focused element is a child of the tab,
			///          \ref _focus is set accordingly; otherwise this struct does nothing.
			explicit _keep_intab_focus(tab &t) {
				for (ui::element *e = ui::manager::get().get_focused(); e != nullptr; e = e->parent()) {
					if (e == &t) {
						_focus = ui::manager::get().get_focused();
						break;
					}
				}
			}
			/// No copy construction.
			_keep_intab_focus(const _keep_intab_focus&) = delete;
			/// No copy assignment.
			_keep_intab_focus &operator=(const _keep_intab_focus&) = delete;
			/// Resets the focused element to \ref _focus (if it's not \p nullptr) when it's disposed.
			~_keep_intab_focus() {
				if (_focus) {
					ui::manager::get().set_focus(_focus);
				}
			}
		protected:
			ui::element *_focus = nullptr; ///< The focused element before the \ref tab is moved.
		};

		/// Creates a new window and registers necessary event handlers.
		os::window_base *_new_window() {
			os::window_base *wnd = ui::element::create<os::window>();
			_wndlist.emplace_back(wnd);
			wnd->got_window_focus += [this, wnd]() {
				auto it = _wndlist.begin();
				for (; it != _wndlist.end(); ++it) { // there can't be too many windows... right?
					if (*it == wnd) {
						break;
					}
				}
				assert_true_logical(it != _wndlist.end(), "window has been silently removed");
				_wndlist.erase(it);
				_wndlist.insert(_wndlist.begin(), wnd);
			};
			wnd->close_request += [wnd]() { // when requested to be closed, send request to all tabs
				_enumerate_hosts(wnd, [](tab_host &hst) {
					std::vector<tab*> ts;
					for (auto i = hst._tabs.begin(); i != hst._tabs.end(); ++i) {
						ts.push_back(*i);
					}
					for (auto i = ts.begin(); i != ts.end(); ++i) {
						(*i)->_on_close_requested();
					}
					});
			};
			return wnd;
		}
		/// Splits the given \ref tab_host into halves, and returns the resulting \ref split_panel. The original
		/// \ref tab_host will be removed from its parent.
		split_panel *_replace_with_split_panel(tab_host &hst) {
			split_panel *sp = ui::element::create<split_panel>(), *f = dynamic_cast<split_panel*>(hst.parent());
			if (f) {
				if (f->get_child1() == &hst) {
					f->set_child1(sp);
				} else {
					assert_true_logical(f->get_child2() == &hst, "corrupted element tree");
					f->set_child2(sp);
				}
			} else {
				auto *w = dynamic_cast<os::window_base*>(hst.parent());
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
		void _split_tab(tab_host &host, tab &t, ui::orientation orient, bool newfirst) {
			_keep_intab_focus keep_focus(t);
			if (t.get_host() == &host) {
				host.remove_tab(t);
			}
			split_panel *sp = _replace_with_split_panel(host);
			tab_host *th = ui::element::create<tab_host>();
			if (newfirst) {
				sp->set_child1(th);
				sp->set_child2(&host);
			} else {
				sp->set_child1(&host);
				sp->set_child2(th);
			}
			th->add_tab(t);
			sp->set_orientation(orient);
		}

		/// Moves the given \ref tab to a new window with the given layout, detaching it from its original parent.
		void _move_tab_to_new_window(tab &t, rectd layout) {
			_keep_intab_focus keep_focus(t);
			tab_host *host = t.get_host();
			if (host != nullptr) {
				host->remove_tab(t);
			}
			os::window_base *wnd = _new_window();
			tab_host *nhst = ui::element::create<tab_host>();
			wnd->children().add(*nhst);
			nhst->add_tab(t);
			wnd->set_client_size(layout.size().convert<int>());
			wnd->set_position(layout.xmin_ymin().convert<int>());
		}

		/// Disposes \ref _dragdec if it isn't \p nullptr.
		void _try_dispose_preview() {
			if (_dragdec) {
				_dragdec->set_state(ui::visual::get_predefined_states().corpse);
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
		template <typename T> inline static void _enumerate_hosts(os::window_base *base, T &&cb) {
			assert_true_logical(base->children().size() == 1, "window must have only one child");
			std::vector<ui::element*> hsts;
			hsts.push_back(*base->children().begin());
			while (!hsts.empty()) {
				ui::element *ce = hsts.back();
				hsts.pop_back();
				tab_host *hst = dynamic_cast<tab_host*>(ce);
				if (hst) {
					if constexpr (std::is_same_v<decltype(cb(*static_cast<tab_host*>(nullptr))), bool>) {
						if (!cb(*hst)) {
							break;
						}
					} else {
						cb(*hst);
					}
				} else {
					split_panel *sp = dynamic_cast<split_panel*>(ce);
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
		void _update_drag_tab_position(double pos, double maxw) const {
			double halfw = 0.5 * _drag->_btn->get_desired_size().x, posx = pos + _dragdiff.x + halfw, cx = halfw;
			tab *res = nullptr;
			for (auto i = _dest->_tabs.begin(); i != _dest->_tabs.end(); ++i) {
				if (*i != _drag) {
					double thisw = (*i)->_btn->get_desired_size().x;
					if (posx < cx + 0.5 * thisw) {
						res = *i;
						break;
					}
					cx += thisw;
				}
			}
			_drag->_btn->_xoffset = std::clamp(posx, halfw, maxw - halfw) - cx;
			_dest->move_tab_before(*_drag, res);
		}

		/// Called when a \ref tab is removed from a \ref tab_host. Inserts the \ref tab_host to \ref _changed to
		/// update it afterwards.
		void _on_tab_detached(tab_host &host, tab&) {
			_changed.insert(&host);
		}
		/// Called when a \ref tab_host is about to be disposed in \ref update_changed_hosts. Handles drag related
		/// property changes.
		void _on_tab_host_disposed(tab_host &hst) {
			logger::get().log_info(CP_HERE, "tab host 0x", &hst, " disposed");
			if (_drag && _dest == &hst) {
				logger::get().log_info(CP_HERE, "resetting drag destination");
				_try_dispose_preview();
				_try_detach_possel();
				_dest = nullptr;
				_dtype = drag_destination_type::new_window;
			}
		}
	};
}
