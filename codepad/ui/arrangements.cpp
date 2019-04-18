// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "arrangements.h"

/// \file
/// Implementation of certain methods related to arrangement configurations.

#include "element.h"
#include "panel.h"
#include "manager.h"

using namespace std;

namespace codepad::ui {
	void class_arrangements::child::construct(
		element_collection &col, panel_base &logparent, notify_mapping &roles, vector<element*> &construction_notify
	) const {
		element *e = logparent.get_manager().create_element_custom(type, element_class, configuration);
		e->_logical_parent = &logparent;
		construction_notify.emplace_back(e);
		if (!children.empty()) {
			auto *pnl = dynamic_cast<panel*>(e);
			if (pnl == nullptr) {
				logger::get().log_warning(CP_HERE, "invalid children for non-panel type: ", type);
			} else {
				for (const child &c : children) {
					c.construct(pnl->children(), logparent, roles, construction_notify);
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
	}


	void class_arrangements::construct_children(panel_base &logparent, notify_mapping &roles) const {
		vector<element*> newelems;
		for (const child &c : children) {
			c.construct(logparent._children, logparent, roles, newelems);
		}
		for (element *e : newelems) {
			e->_on_logical_parent_constructed();
		}
		if (!roles.empty()) {
			logger::get().log_warning(CP_HERE, "there are unmatched roles");
		}
	}
}
