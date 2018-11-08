// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "arrangements.h"

#include "element.h"
#include "panel.h"
#include "manager.h"

namespace codepad::ui {
	void class_arrangements::child::construct(
		element_collection &col, panel_base *logparent, notify_mapping &roles
	) const {
		element *e = manager::get().create_element_custom(type, element_class, metrics);
		e->_logical_parent = logparent;
		e->set_state_bits(set_states, true);
		if (!children.empty()) {
			auto *pnl = dynamic_cast<panel*>(e);
			if (pnl == nullptr) {
				logger::get().log_warning(CP_HERE, "invalid children for non-panel type: ", type);
			} else {
				for (const child &c : children) {
					c.construct(pnl->children(), logparent, roles);
				}
			}
		}
		col.add(*e);
		if (!role.empty()) {
			auto it = roles.find(role);
			if (it == roles.end()) {
				logger::get().log_warning(CP_HERE, "unspecified role detected: ", role);
			} else {
				it->second(e);
				roles.erase(it);
			}
		}
		manager::get().schedule_visual_config_update(*e);
		manager::get().schedule_metrics_config_update(*e);
	}


	void class_arrangements::construct_children(panel_base &logparent, notify_mapping &roles) const {
		for (const child &c : children) {
			c.construct(logparent._children, &logparent, roles);
		}
		manager::get().schedule_visual_config_update(logparent);
		manager::get().schedule_metrics_config_update(logparent);
		if (!roles.empty()) {
			logger::get().log_warning(CP_HERE, "there are unmatched roles");
		}
	}
}
