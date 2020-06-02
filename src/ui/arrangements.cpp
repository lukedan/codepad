// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/arrangements.h"

/// \file
/// Implementation of certain methods related to arrangement configurations.

#include "codepad/ui/element.h"
#include "codepad/ui/panel.h"
#include "codepad/ui/manager.h"

namespace codepad::ui {
	/// Data relavent to the starting of animations. Shared pointers are used to allow for duplication.
	struct _animation_starter {
		/// Default constructor.
		_animation_starter() = default;
		/// Initializes all fields of this struct.
		_animation_starter(
			std::shared_ptr<animation_subject_base> sbj,
			std::shared_ptr<animation_definition_base> def,
			std::any data
		) : subject(std::move(sbj)), definition(std::move(def)), subject_data(std::move(data)) {
		}

		std::shared_ptr<animation_subject_base> subject; ///< Used to create animation subjects.
		std::shared_ptr<animation_definition_base> definition; ///< Definition of this animation.
		std::any subject_data; ///< Data used by \ref subject.
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
		element *e = ctx.logical_parent.get_manager().create_element_custom_no_event_or_attribute(
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
		const auto &properties = affected.get_properties();
		std::vector<_animation_starter> anis;
		for (auto &ani : ev.animations) {
			if (ani.subject.empty()) {
				logger::get().log_warning(CP_HERE) << "empty animation path";
				continue;
			}
			if (!ani.subject.front().type.empty()) {
				logger::get().log_warning(CP_HERE) << "type at the very start of the animation path is ignored";
			}
			auto it = properties.find(ani.subject.front().property);
			if (it == properties.end()) {
				logger::get().log_warning(CP_HERE) <<
					"property not found: " << 
					animation_path::to_string(ani.subject.begin(), ani.subject.end());
				continue;
			}
			animation_subject_information subject = it->second->parse_animation_path(affected, ani.subject);
			if (subject.parser == nullptr || subject.subject == nullptr) {
				logger::get().log_warning(CP_HERE) <<
					"failed to parse animation path: " <<
					animation_path::to_string(ani.subject.begin(), ani.subject.end());
				continue;
			}
			auto &&definition = subject.parser->parse_keyframe_animation(ani.definition, affected.get_manager());
			anis.emplace_back(
				std::move(subject.subject), std::move(definition), std::move(subject.subject_data)
			);
		}
		if (!trigger._register_event(ev.identifier.name, [target = &affected, animations = std::move(anis)]() {
			for (auto &ani : animations) {
				target->get_manager().get_scheduler().start_animation(
					ani.definition->start(ani.subject), target
				);
			}
		})) {
			logger::get().log_warning(CP_HERE) << "unknown event name: " << ev.identifier.name;
		}
	}

	void class_arrangements::set_attributes_of(
		element &elem, const std::map<std::u8string, json::value_storage> &attrs
	) {
		auto &properties = elem.get_properties();
		for (const auto &[name, attr] : attrs) {
			auto it = properties.find(name);
			if (it != properties.end()) {
				it->second->set_value(elem, attr);
			} else {
				logger::get().log_warning(CP_HERE) << "unknown attribute name: " << name;
			}
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
		// additional attributes
		// these are set after triggers are registered to enable elements to correctly react to attributes being set
		set_attributes_of(logparent, configuration.attributes);
		for (auto &&created : ctx.all_created) {
			set_attributes_of(*created.second, created.first->configuration.attributes);
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
