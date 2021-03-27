// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/popup.h"

/// \file
/// Implementation of popup windows.

namespace codepad::ui {
	void popup::_on_added_to_parent() {
		window::_on_added_to_parent();
		auto *wnd = dynamic_cast<window*>(parent());
		_window_layout_changed_token = (wnd->window_layout_changed += [this]() {
			_update_position();
			});
		_update_position();
	}

	void popup::_on_removing_from_parent() {
		window::_on_removing_from_parent();
		auto *wnd = dynamic_cast<window*>(parent());
		wnd->window_layout_changed -= _window_layout_changed_token;
	}

	void popup::_initialize() {
		window::_initialize();

		set_display_border(false);
		set_display_caption_bar(false);
		set_sizable(false);
		set_show_icon(false);
		set_topmost(true);
		set_focusable(false);
	}
}
