// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/arrangements.h"

/// \file
/// Implementation of certain methods related to arrangement configurations.

#include "codepad/ui/element.h"
#include "codepad/ui/panel.h"
#include "codepad/ui/manager.h"

namespace codepad::ui {
	/// Data relavent to the starting and playing of animations. Shared pointers are used because this is used with
	/// \p std::function which requires it to be copy-constructible.
	struct _animation_data {
		/// Default constructor.
		_animation_data() = default;
		/// Initializes all fields of this struct.
		_animation_data(property_info prop, std::shared_ptr<animation_definition_base> ani) :
			property(std::move(prop)), animation(std::move(ani)) {
		}

		property_info property; ///< Used to create animation subjects.
		std::shared_ptr<animation_definition_base> animation; ///< Definition of this animation.
	};

	element *class_arrangements::construction_context::find_by_name(std::u8string_view id, element &self) {
		if (id.empty()) {
			return &self;
		}
		if (auto it = name_mapping.find(id); it != name_mapping.end()) {
			return it->second;
		}
		return nullptr;
	}

	void class_arrangements::construction_context::register_all_triggers_for(
		element &elem, const element_configuration &config
	) {
		for (const auto &trig : config.event_triggers) {
			element *subj = find_by_name(trig.identifier.subject, elem);
			if (subj == nullptr) {
				logger::get().log_warning(CP_HERE) << "cannot find element with name: " << trig.identifier.subject;
				continue;
			}
			register_trigger_for(*subj, elem, trig);
		}
	}


	element *class_arrangements::child::construct(construction_context &ctx) const {
		element *e = ctx.logical_parent.get_manager().create_element_custom_no_event_or_property(
			type, element_class
		);
		if (e) {
			e->_logical_parent = &ctx.logical_parent;
			if (!children.empty()) { // construct children
				auto *pnl = dynamic_cast<panel*>(e);
				if (pnl == nullptr) {
					logger::get().log_warning(CP_HERE) << "invalid children for non-panel type: " << type;
				} else {
					for (const child &c : children) {
						if (element *celem = c.construct(ctx)) {
							pnl->children().add(*celem);
						}
					}
				}
			}
			if (!name.empty()) { // register name
				if (!ctx.register_name(name, *e)) {
					logger::get().log_warning(CP_HERE) << "duplicate element names: " << name;
				}
			}
			ctx.all_created.emplace_back(this, e); // register creation
		} else {
			logger::get().log_warning(CP_HERE) << "failed to construct element with type " << type;
		}
		return e;
	}


	void class_arrangements::register_trigger_for(
		element &trigger, element &affected, const element_configuration::event_trigger &ev
	) {
		std::vector<_animation_data> anis;
		for (auto &ani : ev.animations) {
			if (ani.subject.empty()) {
				logger::get().log_error(CP_HERE) << "empty property path";
				continue;
			}
			property_info prop = affected._find_property_path(ani.subject);
			if (prop.accessor == nullptr || prop.value_handler == nullptr) {
				logger::get().log_error(CP_HERE) <<
					"failed to find property corresponding to property path: " <<
					property_path::to_string(ani.subject.begin(), ani.subject.end());
				continue;
			}
			auto kfani = prop.value_handler->parse_keyframe_animation(ani.definition);
			anis.emplace_back(std::move(prop), std::move(kfani));
		}
		if (!trigger._register_event(ev.identifier.name, [target = &affected, animations = std::move(anis)]() {
			for (auto &ani : animations) {
				target->_start_animation(ani.animation->start(target, ani.property.accessor));
			}
		})) {
			logger::get().log_error(CP_HERE) << "unknown event name: " << ev.identifier.name;
		}
	}

	void class_arrangements::set_properties_of(
		element &elem, const std::vector<element_configuration::property_value> &attrs
	) {
		for (const auto &[path, attr] : attrs) {
			if (path.empty()) {
				logger::get().log_error(CP_HERE) << "empty property path";
				continue;
			}
			property_info prop = elem._find_property_path(path);
			if (prop.accessor == nullptr || prop.value_handler == nullptr) {
				logger::get().log_error(CP_HERE) <<
					"failed to find property corresponding to property path: " <<
					property_path::to_string(path.begin(), path.end());
				continue;
			}
			prop.value_handler->set_value(&elem, *prop.accessor, attr);
		}
	}

	std::size_t class_arrangements::construct_children(panel &logparent, notify_mapping names) const {
		construction_context ctx(logparent);
		ctx.register_name(name, logparent);
		for (const child &c : children) {
			if (element *celem = c.construct(ctx)) {
				logparent.children().add(*celem);
			}
		}
		// triggers
		ctx.register_all_triggers_for(logparent, configuration);
		for (auto &&created : ctx.all_created) {
			ctx.register_all_triggers_for(*created.second, created.first->configuration);
		}
		// additional properties
		// these are set after triggers are registered to enable elements to correctly react to properties being set
		set_properties_of(logparent, configuration.properties);
		for (auto &&created : ctx.all_created) {
			set_properties_of(*created.second, created.first->configuration.properties);
		}
		// construction notification
		for (auto &created : ctx.all_created) {
			if (!created.first->name.empty()) { // call construction notification
				auto it = names.find(created.first->name);
				if (it != names.end()) {
					it->second(created.second);
					names.erase(it);
				}
			}
		}
		// call finish construction handlers
		for (auto &created : ctx.all_created) {
			created.second->_on_logical_parent_constructed();
		}
		return names.size();
	}
}
