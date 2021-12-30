// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Abstraction of modes of interaction that applies to a variety of kinds of editors.

#include <codepad/ui/element.h>
#include <codepad/ui/manager.h>

#include "caret_set.h"
#include "editor.h"
#include "buffer.h"

namespace codepad::editors {
	template <typename> class interaction_manager;
	template <typename> class interaction_mode;

	/// A \ref contents_region_base that exposes caret information for \ref interaction_manager.
	template <typename CaretSet> class interactive_contents_region_base : public contents_region_base {
		friend interaction_mode<CaretSet>;
	public:
		/// Returns the \ref caret_position that the given position points to. The position is relative to the top
		/// left corner of the \ref ui::element's layout.
		virtual typename CaretSet::position hit_test_for_caret(vec2d) const = 0;

		/// Returns the set of carets.
		virtual const CaretSet &get_carets() const = 0;
		/// Adds the given caret to the contents region.
		virtual void add_caret(typename CaretSet::selection) = 0;
		/// Removes the given caret.
		virtual void remove_caret(typename CaretSet::iterator) = 0;
		/// Clears all carets from the contents region.
		virtual void clear_carets() = 0;
	protected:
		/// Called when temporary carets have been changed.
		virtual void _on_temporary_carets_changed() = 0;
	};

	/// Virtual base class of different interaction modes. This class receive certain events, to which it must return
	/// a \p bool indicating whether this mode is still in effect. An object of this type is owned by a specific
	/// \ref manager_t.
	template <typename CaretSet> class interaction_mode {
	public:
		using manager_t = interaction_manager<CaretSet>; ///< The corresponding \ref interaction_manager type.

		/// Initializes \ref _manager.
		interaction_mode(manager_t &man) : _manager(man) {
		}
		/// Default virtual constructor.
		virtual ~interaction_mode() = default;

		/// Called when a mouse button has been pressed.
		virtual bool on_mouse_down(ui::mouse_button_info&) {
			return true;
		}
		/// Called when a mouse button has been released.
		virtual bool on_mouse_up(ui::mouse_button_info&) {
			return true;
		}
		/// Called when the mouse has been moved.
		virtual bool on_mouse_move(ui::mouse_move_info&) {
			return true;
		}
		/// Called when the mouse capture has been lost.
		virtual bool on_capture_lost() {
			return true;
		}

		/// Called when an edit operation is about to take place, where the carets will likely be used.
		virtual bool on_edit_operation() {
			return true;
		}
		/// Called when the viewport of the contents region has changed.
		virtual bool on_viewport_changed() {
			return true;
		}

		/// Returns the override cursor in this mode. If the cursor is not overriden, return
		/// \ref ui::cursor::not_specified.
		virtual ui::cursor get_override_cursor() const {
			return ui::cursor::not_specified;
		}

		/// Returns a list of temporary carets that should be rendered but are not actually in effect.
		virtual std::vector<typename CaretSet::selection> get_temporary_carets() const = 0;
	protected:
		/// Notify the contents region by calling
		/// \ref interactive_contents_region_base::_on_temporary_carets_changed().
		void _on_temporary_carets_changed();

		manager_t &_manager; ///< The manager of this \ref interaction_mode object.
	};
	/// Virtual base class that controls the activation of \ref interaction_mode "interaction_modes". This class
	/// receives certain events, to which it can either return \p nullptr, meaning that this mode is not activated,
	/// or a pointer to an newly-created object of the corresponding \ref interaction_mode. If an
	/// \ref interaction_mode is active, these will not receive any events. Derived classes should overload at least
	/// one operator.
	template <typename CaretSet> class interaction_mode_activator {
	public:
		using manager_t = interaction_manager<CaretSet>; ///< The corresponding \ref interaction_manager type.
		using mode_t = interaction_mode<CaretSet>; ///< The corresponding \ref interaction_mode type.

		/// Default virtual constructor.
		virtual ~interaction_mode_activator() = default;

		/// Called when a mouse button has been pressed. Returns \p nullptr by default.
		virtual std::unique_ptr<mode_t> on_mouse_down(manager_t&, ui::mouse_button_info&) {
			return nullptr;
		}
		/// Called when a mouse button has been released. Returns \p nullptr by default.
		virtual std::unique_ptr<mode_t> on_mouse_up(manager_t&, ui::mouse_button_info&) {
			return nullptr;
		}
		/// Called when the mouse has been moved. Returns \p nullptr by default.
		virtual std::unique_ptr<mode_t> on_mouse_move(manager_t&, ui::mouse_move_info&) {
			return nullptr;
		}
		/// Called when the mouse capture has been lost. Returns \p nullptr by default.
		virtual std::unique_ptr<mode_t> on_capture_lost(manager_t&) {
			return nullptr;
		}

		/// Returns the override cursor in this mode. If the cursor is not overriden, return
		/// \ref ui::cursor::not_specified. If a mode does override the cursor, all following modes will be ignored.
		virtual ui::cursor get_override_cursor(const manager_t&) const {
			return ui::cursor::not_specified;
		}
	};

	/// Manages a list of \ref interaction_mode "interaction_modes" for a single editor. At any time at most one mode
	/// can be active, to which all interaction events will be forwarded.
	template <typename CaretSet> class interaction_manager {
	public:
		using mode_t = interaction_mode<CaretSet>; ///< The corresponding \ref interaction_mode type.
		/// The corresponding \ref interaction_mode_activator type.
		using mode_activator_t = interaction_mode_activator<CaretSet>;

		/// Returns a reference to \ref _activators.
		std::vector<std::unique_ptr<mode_activator_t>> &activators() {
			return _activators;
		}
		/// \overload
		const std::vector<std::unique_ptr<mode_activator_t>> &activators() const {
			return _activators;
		}

		/// Returns \ref _cached_position.
		typename CaretSet::position get_mouse_position() const {
			return _cached_position;
		}

		/// Sets the \ref interactive_contents_region_base for this \ref interaction_manager.
		void set_contents_region(interactive_contents_region_base<CaretSet> &rgn) {
			_contents_region = &rgn;
		}
		/// Returns \ref _contents_region.
		interactive_contents_region_base<CaretSet> &get_contents_region() const {
			return *_contents_region;
		}

		/// Returns a list of temporary carets.
		///
		/// \sa interaction_mode::get_temporary_carets()
		std::vector<typename CaretSet::selection> get_temporary_carets() const {
			if (_active) {
				return _active->get_temporary_carets();
			}
			return {};
		}

		/// Called when a mouse button has been pressed.
		void on_mouse_down(ui::mouse_button_info &info) {
			_update_cached_positions(info.position.get(*_contents_region));
			_dispatch_event<&mode_activator_t::on_mouse_down, &mode_t::on_mouse_down>(info);
		}
		/// Called when a mouse button has been released.
		void on_mouse_up(ui::mouse_button_info &info) {
			_update_cached_positions(info.position.get(*_contents_region));
			_dispatch_event<&mode_activator_t::on_mouse_up, &mode_t::on_mouse_up>(info);
		}
		/// Called when the mouse has been moved.
		void on_mouse_move(ui::mouse_move_info &info) {
			_update_cached_positions(info.new_position.get(*_contents_region));
			_dispatch_event<&mode_activator_t::on_mouse_move, &mode_t::on_mouse_move>(info);
		}
		/// Called when the mouse capture has been lost.
		void on_capture_lost() {
			_dispatch_event<&mode_activator_t::on_capture_lost, &mode_t::on_capture_lost>();
		}

		/// Called when an edit operation is about to take place, where the carets will likely be used.
		void on_edit_operation() {
			_dispatch_event<nullptr, &mode_t::on_edit_operation>();
		}
		/// Called when the viewport of the contents region has changed.
		void on_viewport_changed() {
			_cached_position = _contents_region->hit_test_for_caret(_cached_mouse_position);
			_dispatch_event<nullptr, &mode_t::on_viewport_changed>();
		}

		/// Returns the overriden cursor. Returns \p ui::cursor::not_specified when the cursor is not overriden. If
		/// there's an active \ref interaction_mode, then it decides the overriden cursor; otherwise the cursor is
		/// decided by the list of \ref interaction_mode_activator "interaction_mode_activators".
		ui::cursor get_override_cursor() const {
			if (_active) {
				return _active->get_override_cursor();
			}
			for (auto &&activator : _activators) {
				ui::cursor c = activator->get_override_cursor(*this);
				if (c != ui::cursor::not_specified) {
					return c;
				}
			}
			return ui::cursor::not_specified;
		}
	protected:
		std::vector<std::unique_ptr<mode_activator_t>> _activators; ///< The list of activators.
		/// The cached mouse position relative to the corresponding \ref ui::element, used when none is supplied.
		vec2d _cached_mouse_position;
		typename CaretSet::position _cached_position; ///< The cached mouse position.
		std::unique_ptr<mode_t> _active; ///< The currently active \ref interaction_mode.
		interactive_contents_region_base<CaretSet> *_contents_region = nullptr; ///< The \ref contents_region_base.

		/// Dispatches an event. If \ref _active is \p nullptr, then the \p ActivatorPtr of each entry in
		/// \ref _activators will be called. Otherwise, \p ModePtr of \ref _active will be called, and \ref _active
		/// will be disposed if necessary.
		template <auto ActivatorPtr, auto ModePtr, typename ...Args> void _dispatch_event(Args &&...args) {
			if (_active) {
				if (!(_active.get()->*ModePtr)(std::forward<Args>(args)...)) { // deactivated
					_active.reset();
				}
			} else {
				if constexpr (static_cast<bool>(ActivatorPtr)) {
					for (auto &&act : _activators) {
						if (auto ptr = ((*act).*ActivatorPtr)(*this, std::forward<Args>(args)...)) { // activated
							_active = std::move(ptr);
							break;
						}
					}
				}
			}
		}
		/// Updates \ref _cached_mouse_position and \ref _cached_position with the given mouse position.
		void _update_cached_positions(vec2d pos) {
			_cached_mouse_position = pos;
			_cached_position = _contents_region->hit_test_for_caret(_cached_mouse_position);
		}
	};

	template <typename CaretSet> void interaction_mode<CaretSet>::_on_temporary_carets_changed() {
		_manager.get_contents_region()._on_temporary_carets_changed();
	}

	/// A registry of interaction modes.
	template <typename CaretSet> class interaction_mode_registry {
	public:
		/// Used to create instances of \ref interaction_mode_activator.
		using activator_creator = std::function<std::unique_ptr<interaction_mode_activator<CaretSet>>()>;

		/// Tries to create an \ref interaction_mode_activator that corresponds to the given name.
		std::unique_ptr<interaction_mode_activator<CaretSet>> try_create(std::u8string_view name) const {
			if (auto it = mapping.find(name); it != mapping.end()) {
				return it->second();
			}
			return nullptr;
		}

		std::map<std::u8string, activator_creator, std::less<>> mapping; ///< The mapping between mode names and creators.
	};


	/// Contains several built-in interaction modes.
	namespace interaction_modes {
		/// A mode where the user can scroll the viewport by moving the mouse near or out of boundaries. This is
		/// intended to be used as a base class for other interaction modes.
		template <typename CaretSet> class mouse_navigation_mode : public interaction_mode<CaretSet> {
		public:
			using typename interaction_mode<CaretSet>::manager_t;

			constexpr static double default_padding_value = 50.0; ///< The default value of \ref _padding.

			/// Initializes the base class with the given \ref manager_t.
			mouse_navigation_mode(manager_t &man) : interaction_mode<CaretSet>(man) {
			}
			/// Unregisters \ref _scroll_task.
			~mouse_navigation_mode() {
				if (!_scroll_task.empty()) {
					this->_manager.get_contents_region().get_manager().get_scheduler().cancel_synchronous_task(_scroll_task);
				}
			}

			/// Updates \ref _speed. This function always returns \p true.
			bool on_mouse_move(ui::mouse_move_info &info) override {
				contents_region_base &elem = this->_manager.get_contents_region();
				rectd r = ui::thickness(_padding).shrink(rectd::from_corners(vec2d(), elem.get_layout().size()));
				r = r.made_positive_average();
				// find anchor point
				bool scrolling = false;
				vec2d anchor = info.new_position.get(elem);
				if (anchor.x < r.xmin) {
					anchor.x = r.xmin;
					scrolling = true;
				} else if (anchor.x > r.xmax) {
					anchor.x = r.xmax;
					scrolling = true;
				}
				if (anchor.y < r.ymin) {
					anchor.y = r.ymin;
					scrolling = true;
				} else if (anchor.y > r.ymax) {
					anchor.y = r.ymax;
					scrolling = true;
				}
				if (scrolling) {
					// calculate speed
					_speed = info.new_position.get(elem) - anchor; // TODO further manipulate _speed
					if (_scroll_task.empty()) { // schedule update
						auto now = ui::scheduler::clock_t::now();
						_last_scroll_update = now;
						_scroll_task = elem.get_manager().get_scheduler().register_synchronous_task(
							now, &elem,
							[this](ui::element *e) -> std::optional<ui::scheduler::clock_t::time_point> {
								auto now = ui::scheduler::clock_t::now();
								double delta_time = std::chrono::duration<double>(now - _last_scroll_update).count();
								auto *rgn = dynamic_cast<contents_region_base*>(e);
								rgn->get_editor().set_position_immediate(
									rgn->get_editor().get_position() + _speed * delta_time
								);
								_last_scroll_update = now;
								return now;
							}
						);
					}
				} else {
					if (!_scroll_task.empty()) {
						elem.get_manager().get_scheduler().cancel_synchronous_task(_scroll_task);
					}
				}
				return true;
			}
		protected:
			vec2d _speed; ///< The speed of scrolling.
			ui::scheduler::sync_task_token _scroll_task; ///< The task used to update smooth mouse scrolling.
			ui::scheduler::clock_t::time_point _last_scroll_update; ///< The time of the last scroll update.
			/// The inner padding. This allows the screen to start scrolling even if the mouse is still inside the
			/// \ref ui::element.
			double _padding = default_padding_value;
		};

		/// The mode that allows the user to edit a single selected region using the mouse.
		template <typename CaretSet> class mouse_single_selection_mode : public mouse_navigation_mode<CaretSet> {
		public:
			using typename mouse_navigation_mode<CaretSet>::manager_t;

			/// How existing carets will be handled.
			enum class mode {
				single, ///< Existing carets will be cleared.
				multiple, ///< Existing carets will be preserved.
				extend ///< One of the existing carets will be edited.
			};

			/// Acquires mouse capture and initializes the caret with the given value.
			mouse_single_selection_mode(
				manager_t &man, ui::mouse_button trig, typename CaretSet::selection initial_value
			) : mouse_navigation_mode<CaretSet>(man), _selection(initial_value), _trigger_button(trig) {
				contents_region_base &elem = this->_manager.get_contents_region();
				elem.get_window()->set_mouse_capture(elem);
			}
			/// Delegate constructor that initializes the caret with the mouse position.
			mouse_single_selection_mode(manager_t &man, ui::mouse_button trig) :
				mouse_single_selection_mode(man, trig, typename CaretSet::selection(man.get_mouse_position())) {
			}

			/// Updates the caret.
			bool on_mouse_move(ui::mouse_move_info &info) override {
				mouse_navigation_mode<CaretSet>::on_mouse_move(info);
				if (this->_manager.get_mouse_position() != CaretSet::get_caret_position(_selection)) {
					CaretSet::set_caret_position(_selection, this->_manager.get_mouse_position());
					interaction_mode<CaretSet>::_on_temporary_carets_changed();
				}
				return true;
			}
			/// Releases capture and exits this mode if \ref _trigger_button is released.
			bool on_mouse_up(ui::mouse_button_info &info) override {
				if (info.button == _trigger_button) {
					_exit(true);
					return false;
				}
				return true;
			}
			/// Updates the caret.
			bool on_viewport_changed() override {
				CaretSet::set_caret_position(_selection, this->_manager.get_mouse_position());
				return true;
			}
			/// Exits this mode.
			bool on_capture_lost() override {
				_exit(false);
				return false;
			}
			/// Exits this mode.
			bool on_edit_operation() override {
				_exit(true);
				return false;
			}

			/// Returns \ref _selection.
			std::vector<typename CaretSet::selection> get_temporary_carets() const override {
				return { _selection };
			}
		protected:
			typename CaretSet::selection _selection; ///< The selection being edited.
			/// The mouse button that triggered this mode.
			ui::mouse_button _trigger_button = ui::mouse_button::primary;

			/// Called when about to exit the mode. Adds the caret to the caret set. If \p release_capture is
			/// \p true, then \ref ui::window::release_mouse_capture() will also be called.
			void _exit(bool release_capture) {
				this->_manager.get_contents_region().add_caret(_selection);
				if (release_capture) {
					this->_manager.get_contents_region().get_window()->release_mouse_capture();
				}
			}
		};
		/// Triggers \ref mouse_single_selection_mode.
		template <typename CaretSet> class mouse_single_selection_mode_activator :
			public interaction_mode_activator<CaretSet> {
		public:
			using typename interaction_mode_activator<CaretSet>::manager_t;
			using typename interaction_mode_activator<CaretSet>::mode_t;

			std::unique_ptr<mode_t> on_mouse_down(
				manager_t &man, ui::mouse_button_info &info
			) override {
				if (info.get_gesture() == edit_gesture) {
					// select a caret to be edited
					auto it = man.get_contents_region().get_carets().begin(); // TODO select a better caret
					if (it.get_iterator() == man.get_contents_region().get_carets().carets.end()) {
						// should not happen
						logger::get().log_error() << "empty caret set when starting mouse interaction";
						return nullptr;
					}
					typename CaretSet::selection caret_sel = CaretSet::get_caret_selection(it);
					CaretSet::set_caret_position(caret_sel, man.get_mouse_position());
					man.get_contents_region().remove_caret(it.get_iterator());
					return std::make_unique<mouse_single_selection_mode<CaretSet>>(
						man, edit_gesture.primary, caret_sel
						);
				} else if (info.get_gesture() == multiple_select_gesture) {
					return std::make_unique<mouse_single_selection_mode<CaretSet>>(
						man, multiple_select_gesture.primary
						);
				} else if (info.button == ui::mouse_button::primary) {
					man.get_contents_region().clear_carets();
					return std::make_unique<mouse_single_selection_mode<CaretSet>>(man, ui::mouse_button::primary);
				}
				return nullptr;
			}

			ui::mouse_gesture
				/// The mouse gesture used for multiple selection.
				multiple_select_gesture{ ui::mouse_button::primary, ui::modifier_keys::control },
				/// The mouse gesture used for editing existing selected regions.
				edit_gesture{ ui::mouse_button::primary, ui::modifier_keys::shift };
		};

		/// Mode where the user is about to start dragging text with the mouse.
		template <typename CaretSet> class mouse_prepare_drag_mode : public interaction_mode<CaretSet> {
		public:
			using typename interaction_mode<CaretSet>::manager_t;

			/// Initializes \ref _init_pos and acquires mouse capture.
			mouse_prepare_drag_mode(manager_t &man, vec2d pos) : interaction_mode<CaretSet>(man), _init_pos(pos) {
				this->_manager.get_contents_region().get_window()->set_mouse_capture(
					this->_manager.get_contents_region()
				);
			}

			/// Exit on mouse up.
			bool on_mouse_up(ui::mouse_button_info&) override {
				this->_manager.get_contents_region().get_window()->release_mouse_capture();
				this->_manager.get_contents_region().clear_carets();
				this->_manager.get_contents_region().add_caret(
					typename CaretSet::selection(this->_manager.get_mouse_position())
				);
				return false;
			}
			/// Checks the position of the mouse, and starts drag drop and exits if the distance is enough.
			bool on_mouse_move(ui::mouse_move_info &info) override {
				if (
					(info.new_position.get(this->_manager.get_contents_region()) - _init_pos).length_sqr() > 25.0
				) { // TODO magic value
					logger::get().log_debug() << "start drag drop";
					// TODO start
					this->_manager.get_contents_region().get_window()->release_mouse_capture();
					return false;
				}
				return true;
			}
			/// Exit on capture lost.
			bool on_capture_lost() override {
				return false;
			}

			/// Exit on edit operations.
			bool on_edit_operation() override {
				this->_manager.get_contents_region().get_window()->release_mouse_capture();
				return false;
			}

			/// Returns \ref ui::cursor::normal.
			ui::cursor get_override_cursor() const override {
				return ui::cursor::normal;
			}

			/// No temporary carets.
			std::vector<typename CaretSet::selection> get_temporary_carets() const override {
				return {};
			}
		protected:
			vec2d _init_pos; ///< The initial position of the mouse.
		};
		/// Triggers \ref mouse_prepare_drag_mode.
		template <typename CaretSet> class mouse_prepare_drag_mode_activator :
			public interaction_mode_activator<CaretSet> {
		public:
			using typename interaction_mode_activator<CaretSet>::manager_t;
			using typename interaction_mode_activator<CaretSet>::mode_t;

			/// Activates a \ref mouse_prepare_drag_mode if the mouse is in a selected region.
			std::unique_ptr<mode_t> on_mouse_down(manager_t &man, ui::mouse_button_info &info) override {
				if (
					info.button == button &&
					man.get_contents_region().get_carets().is_in_selection(man.get_mouse_position())
					) {
					return std::make_unique<mouse_prepare_drag_mode<CaretSet>>(
						man, info.position.get(man.get_contents_region())
						);
				}
				return nullptr;
			}

			/// Returns the override cursor in this mode. If the cursor is not overriden, return
			/// \ref ui::cursor::not_specified. If a mode does override the cursor, all following modes will be
			/// ignored.
			virtual ui::cursor get_override_cursor(const manager_t &man) const override {
				if (man.get_contents_region().get_carets().is_in_selection(man.get_mouse_position())) {
					return ui::cursor::normal;
				}
				return interaction_mode_activator<CaretSet>::get_override_cursor(man);
			}

			ui::mouse_button button = ui::mouse_button::primary; ///< The mouse button that activates this mode.
		};
	}
}
