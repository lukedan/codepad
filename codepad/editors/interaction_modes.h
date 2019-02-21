// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Abstraction of modes of interaction that applies to a variety of kinds of editors.

#include "../ui/element.h"
#include "../ui/manager.h"
#include "caret_set.h"

namespace codepad::editors {
	class interaction_manager;

	/// Pure virtual class used to grant access to the associated \ref code::contents_region or
	/// \ref binary::contents_region.
	class contents_region_proxy {
	public:
		/// Initializes \ref manager.
		contents_region_proxy(interaction_manager &m) : manager(m) {
		}
		/// Default virtual destructor.
		virtual ~contents_region_proxy() = default;

		/// Returns the \ref caret_position that the given position points to. The position is relative to the top
		/// left corner of the \ref ui::element's layout.
		virtual caret_position hit_test_for_caret(vec2d) const = 0;

		/// Returns the current position of the editor.
		virtual vec2d get_editor_position() const = 0;
		/// Sets the position of the editor.
		virtual void set_editor_position(vec2d) = 0;

		/// Adds the given caret to the contents region.
		virtual void add_caret(caret_selection_position) = 0;
		/// Clears all carets from the contents region.
		virtual void clear_carets() = 0;
		/// Selects a caret to edit based on the given \ref caret_position, removes that caret, and returns its
		/// value.
		///
		/// \todo Ugly.
		virtual caret_selection_position select_remove_edited_caret(caret_position) = 0;

		/// Returns the associated \ref ui::element.
		virtual ui::element &get_element() const = 0;

		interaction_manager &manager; ///< The \ref interaction_manager.
	};

	/// Virtual base class of different interaction modes. This class receive certain events, to which it must return
	/// a \p bool indicating whether this mode is still in effect.
	class interaction_mode {
	public:
		/// Default virtual constructor.
		virtual ~interaction_mode() = default;

		/// Called when a mouse button has been pressed.
		virtual bool on_mouse_down(interaction_manager&, ui::mouse_button_info&) {
			return true;
		}
		/// Called when a mouse button has been released.
		virtual bool on_mouse_up(interaction_manager&, ui::mouse_button_info&) {
			return true;
		}
		/// Called when the mouse has been moved.
		virtual bool on_mouse_move(interaction_manager&, ui::mouse_move_info&) {
			return true;
		}
		/// Called when the mouse capture has been lost.
		virtual bool on_capture_lost(interaction_manager&) {
			return true;
		}
		/// Called when a key has been pressed.
		virtual bool on_key_down(interaction_manager&, ui::key_info&) {
			return true;
		}
		/// Called when a key has been released.
		virtual bool on_key_up(interaction_manager&, ui::key_info&) {
			return true;
		}
		/// Called when the element is being updated.
		virtual bool on_update(interaction_manager&) {
			return true;
		}

		/// Called when an edit operation is about to take place, where the carets will likely be used.
		virtual bool on_edit_operation(interaction_manager&) {
			return true;
		}

		/// Returns a list of temporary carets that should be rendered but are not actually in effect.
		virtual std::vector<caret_selection_position> get_temporary_carets(interaction_manager&) const = 0;
	};
	/// Virtual base class that controls the activation of \ref interaction_mode "interaction_modes". This class
	/// receives certain events, to which it can either return \p nullptr, meaning that this mode is not activated,
	/// or a pointer to an newly-created object of the corresponding \ref interaction_mode. If an
	/// \ref interaction_mode is active, these will not receive any events. Derived classes should overload at least
	/// one operator.
	class interaction_mode_activator {
	public:
		/// Default virtual constructor.
		virtual ~interaction_mode_activator() = default;

		/// Called when a mouse button has been pressed. Returns \p nullptr by default.
		virtual std::unique_ptr<interaction_mode> on_mouse_down(interaction_manager&, ui::mouse_button_info&) {
			return nullptr;
		}
		/// Called when a mouse button has been released. Returns \p nullptr by default.
		virtual std::unique_ptr<interaction_mode> on_mouse_up(interaction_manager&, ui::mouse_button_info&) {
			return nullptr;
		}
		/// Called when the mouse has been moved. Returns \p nullptr by default.
		virtual std::unique_ptr<interaction_mode> on_mouse_move(interaction_manager&, ui::mouse_move_info&) {
			return nullptr;
		}
		/// Called when the mouse capture has been lost. Returns \p nullptr by default.
		virtual std::unique_ptr<interaction_mode> on_capture_lost(interaction_manager&) {
			return nullptr;
		}
		/// Called when a key has been pressed. Returns \p nullptr by default.
		virtual std::unique_ptr<interaction_mode> on_key_down(interaction_manager&, ui::key_info&) {
			return nullptr;
		}
		/// Called when a key has been released. Returns \p nullptr by default.
		virtual std::unique_ptr<interaction_mode> on_key_up(interaction_manager&, ui::key_info&) {
			return nullptr;
		}
	};

	/// Manages a list of \ref interaction_mode "interaction_modes". At any time at most one mode can be active, to
	/// which all interaction events will be forwarded.
	class interaction_manager {
	public:
		/// Returns a reference to \ref _activators.
		std::vector<interaction_mode_activator*> &activators() {
			return _activators;
		}
		/// \overload
		const std::vector<interaction_mode_activator*> &activators() const {
			return _activators;
		}

		/// Returns \ref _cached_position.
		caret_position get_mouse_position() const {
			return _cached_position;
		}

		/// Sets the \ref contents_region_proxy for this \ref interaction_manager.
		void set_contents_region_proxy(contents_region_proxy &proxy) {
			_contents_region = &proxy;
		}
		/// Returns \ref _contents_region.
		contents_region_proxy &get_contents_region() const {
			return *_contents_region;
		}

		/// Returns a list of temporary carets.
		///
		/// \sa interaction_mode::get_temporary_carets()
		std::vector<caret_selection_position> get_temporary_carets() {
			if (_active) {
				return _active->get_temporary_carets(*this);
			}
			return {};
		}

		/// Called when a mouse button has been pressed.
		void on_mouse_down(ui::mouse_button_info &info) {
			_cached_position = _contents_region->hit_test_for_caret(
				info.position - _contents_region->get_element().get_layout().xmin_ymin()
			);
			_dispatch_event<&interaction_mode_activator::on_mouse_down, &interaction_mode::on_mouse_down>(info);
		}
		/// Called when a mouse button has been released.
		void on_mouse_up(ui::mouse_button_info &info) {
			_cached_position = _contents_region->hit_test_for_caret(
				info.position - _contents_region->get_element().get_layout().xmin_ymin()
			);
			_dispatch_event<&interaction_mode_activator::on_mouse_up, &interaction_mode::on_mouse_up>(info);
		}
		/// Called when the mouse has been moved.
		void on_mouse_move(ui::mouse_move_info &info) {
			_cached_position = _contents_region->hit_test_for_caret(
				info.new_position - _contents_region->get_element().get_layout().xmin_ymin()
			);
			_dispatch_event<&interaction_mode_activator::on_mouse_move, &interaction_mode::on_mouse_move>(info);
		}
		/// Called when the mouse capture has been lost.
		void on_capture_lost() {
			_dispatch_event<&interaction_mode_activator::on_capture_lost, &interaction_mode::on_capture_lost>();
		}
		/// Called when a key has been pressed.
		void on_key_down(ui::key_info &info) {
			_dispatch_event<&interaction_mode_activator::on_key_down, &interaction_mode::on_key_down>(info);
		}
		/// Called when a key has been released.
		void on_key_up(ui::key_info &info) {
			_dispatch_event<&interaction_mode_activator::on_key_up, &interaction_mode::on_key_up>(info);
		}
		/// Called when the element is being updated.
		void on_update() {
			_dispatch_event<nullptr, &interaction_mode::on_update>();
		}

		/// Called when an edit operation is about to take place, where the carets will likely be used.
		void on_edit_operation() {
			_dispatch_event<nullptr, &interaction_mode::on_edit_operation>();
		}
	protected:
		std::vector<interaction_mode_activator*> _activators; ///< The list of activators.
		caret_position _cached_position; ///< The cached mouse position.
		std::unique_ptr<interaction_mode> _active; ///< The currently active \ref interaction_mode.
		contents_region_proxy *_contents_region = nullptr; ///< The proxy of the \p contents_region.

		/// Dispatches an event. If \ref _active is \p nullptr, then the \ref ActivatorPtr of each entry in
		/// \ref _activators will be called. Otherwise, \ref ModePtr of \ref _active will be called, and \ref _active
		/// will be disposed if necessary.
		template <auto ActivatorPtr, auto ModePtr, typename ...Args> void _dispatch_event(Args &&...args) {
			if (_active) {
				if (!(_active.get()->*ModePtr)(*this, std::forward<Args>(args)...)) { // deactivated
					_active.reset();
				}
			} else {
				if constexpr (ActivatorPtr) {
					for (interaction_mode_activator *act : _activators) {
						if (auto ptr = (act->*ActivatorPtr)(*this, std::forward<Args>(args)...)) { // activated
							_active = std::move(ptr);
							break;
						}
					}
				}
			}
		}
	};

	/// Contains several built-in interaction modes.
	namespace interaction_modes {
		/// A mode where the user can scroll the viewport by moving the mouse near or out of boundaries. This is
		/// intended to be used as a base class for other interaction modes.
		class mouse_nagivation_mode : public interaction_mode {
		public:
			constexpr static double default_padding_value = 50.0; ///< The default value of \ref _padding.

			/// Updates \ref _speed. This function always returns \p true.
			bool on_mouse_move(interaction_manager &man, ui::mouse_move_info &info) override {
				ui::element &elem = man.get_contents_region().get_element();
				rectd r = ui::thickness(_padding).shrink(elem.get_layout());
				r.make_valid_average();
				// find anchor point
				_scrolling = false;
				vec2d anchor = info.new_position;
				if (anchor.x < r.xmin) {
					anchor.x = r.xmin;
					_scrolling = true;
				} else if (anchor.x > r.xmax) {
					anchor.x = r.xmax;
					_scrolling = true;
				}
				if (anchor.y < r.ymin) {
					anchor.y = r.ymin;
					_scrolling = true;
				} else if (anchor.y > r.ymax) {
					anchor.y = r.ymax;
					_scrolling = true;
				}
				// calculate speed
				_speed = info.new_position - anchor; // TODO further manipulate _speed
				if (_scrolling) { // schedule update
					elem.get_manager().get_scheduler().schedule_element_update(elem);
				}
				return true;
			}
			/// Updates the position of the contents region. This function always returns \p true.
			bool on_update(interaction_manager &man) override {
				if (_scrolling) {
					contents_region_proxy &contents = man.get_contents_region();
					ui::element &elem = contents.get_element();
					contents.set_editor_position(
						contents.get_editor_position() +
						_speed * elem.get_manager().get_scheduler().update_delta_time()
					);
					elem.get_manager().get_scheduler().schedule_element_update(elem);
				}
				return true;
			}
		protected:
			vec2d _speed; ///< The speed of scrolling.
			/// The inner padding. This allows the screen to start scrolling even if the mouse is still inside the
			/// \ref ui::element.
			double _padding = default_padding_value;
			bool _scrolling = false; ///< Whether the viewport is currently scrolling.
		};

		/// The mode that allows the user to edit a single selected region using the mouse.
		class mouse_single_selection_mode : public mouse_nagivation_mode {
		public:
			/// How existing carets will be handled.
			enum class mode {
				single, ///< Existing carets will be cleared.
				multiple, ///< Existing carets will be preserved.
				extend ///< One of the existing carets will be edited.
			};

			/// Acquires mouse capture and initializes the caret with the given value.
			mouse_single_selection_mode(
				interaction_manager &man, ui::mouse_button trig, caret_selection_position initial_value
			) : mouse_nagivation_mode(), _selection(initial_value), _trigger_button(trig) {
				ui::element &elem = man.get_contents_region().get_element();
				elem.get_window()->set_mouse_capture(elem);
			}
			/// Delegate constructor that initializes the caret with the mouse position.
			mouse_single_selection_mode(interaction_manager &man, ui::mouse_button trig) :
				mouse_single_selection_mode(man, trig, man.get_mouse_position()) {
			}

			/// Updates the caret.
			bool on_mouse_move(interaction_manager &man, ui::mouse_move_info &info) override {
				mouse_nagivation_mode::on_mouse_move(man, info);
				_selection.set_caret_position(man.get_mouse_position());
				return true;
			}
			/// Releases capture and exits this mode if \ref _trigger_button is released.
			bool on_mouse_up(interaction_manager &man, ui::mouse_button_info &info) override {
				if (info.button == _trigger_button) {
					_exit(man, true);
					return false;
				}
				return true;
			}
			/// Exits this mode.
			bool on_capture_lost(interaction_manager &man) override {
				_exit(man, false);
				return false;
			}
			/// Exits this mode.
			bool on_edit_operation(interaction_manager &man) override {
				_exit(man, true);
				return false;
			}

			/// Returns \ref _selection.
			std::vector<caret_selection_position> get_temporary_carets(interaction_manager&) const override {
				return {_selection};
			}
		protected:
			caret_selection_position _selection; ///< The selection being edited.
			/// The mouse button that triggered this mode.
			ui::mouse_button _trigger_button = ui::mouse_button::primary;

			/// Called when about to exit the mode. Adds the caret to the caret set. If \p release_capture is
			/// \p true, then \ref ui::window_base::release_mouse_capture() will also be called.
			void _exit(interaction_manager &man, bool release_capture) {
				man.get_contents_region().add_caret(_selection);
				if (release_capture) {
					man.get_contents_region().get_element().get_window()->release_mouse_capture();
				}
			}
		};
		/// Triggers \ref mouse_selection_mode.
		class mouse_single_selection_mode_activator : public interaction_mode_activator {
		public:
			std::unique_ptr<interaction_mode> on_mouse_down(
				interaction_manager &man, ui::mouse_button_info &info
			) override {
				if (info.button == edit_button && info.modifiers == edit_modifiers) {
					return std::make_unique<mouse_single_selection_mode>(
						man, edit_button,
						man.get_contents_region().select_remove_edited_caret(man.get_mouse_position())
						);
				} else if (info.button == multiple_select_button && info.modifiers == multiple_select_modifiers) {
					return std::make_unique<mouse_single_selection_mode>(man, multiple_select_button);
				} else if (info.button == ui::mouse_button::primary) {
					man.get_contents_region().clear_carets();
					return std::make_unique<mouse_single_selection_mode>(man, ui::mouse_button::primary);
				}
				return nullptr;
			}

			ui::mouse_button
				/// The mouse button used for multiple selection.
				multiple_select_button = ui::mouse_button::primary,
				/// The mouse button used for editing existing selected regions.
				edit_button = ui::mouse_button::primary;
			ui::modifier_keys
				/// The modifier keys for multiple selection.
				multiple_select_modifiers = ui::modifier_keys::control,
				/// The modifiers for editing existing selected regions.
				edit_modifiers = ui::modifier_keys::shift;
		};
	}
}
