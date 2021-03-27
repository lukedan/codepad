// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Popup windows.

#include "../window.h"

namespace codepad::ui {
	/// A pop-up window that maintains its position relative to a window.
	class popup : public window {
	public:
		/// Returns \ref _target.
		[[nodiscard]] rectd get_target() const {
			return _target;
		}
		/// Sets the target region this popup should stick to.
		void set_target(rectd target) {
			_target = target;
			_update_position();
		}

		/// Returns the default class of elements of this type.
		inline static std::u8string_view get_default_class() {
			return u8"popup";
		}
	protected:
		rectd _target; ///< The target area within the window that this popup window attempts to stick to.
		info_event<>::token _window_layout_changed_token; ///< Used to listen to \ref _window::window_layout_changed.

		/// Updates the position of this window to stick to the desired region.
		void _update_position() {
			if (auto *wnd = dynamic_cast<window*>(parent())) {
				set_position(wnd->client_to_screen(_target.xmin_ymax()));
			}
		}

		/// Listens to \ref window::window_layout_changed.
		void _on_added_to_parent() override;
		/// Unregisters from \ref window::window_layout_changed.
		void _on_removing_from_parent() override;

		/// Sets the properties of this popup window.
		void _initialize() override;
	};
}
