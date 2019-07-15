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
	/// Data relavent to the starting of animations. Shared pointers are used to allow for duplication.
	struct _animation_starter {
		/// Default constructor.
		_animation_starter() = default;
		/// Initializes all fields of this struct.
		_animation_starter(
			std::shared_ptr<animation_path::subject_creator<element>> sbj,
			std::shared_ptr<animation_definition_base> def
		) : subject_creator(std::move(sbj)), definition(std::move(def)) {
		}

		/// Used to create animation subjects.
		std::shared_ptr<animation_path::subject_creator<element>> subject_creator;
		std::shared_ptr<animation_definition_base> definition; ///< Definition of this animation.

		/// Starts this animation for the given \ref element, and returns the resulting \ref playing_animation_base.
		std::unique_ptr<playing_animation_base> start_for(element &e) const {
			return definition->start(subject_creator->create_for(e));
		}
	};

	void class_arrangements::construction_context::register_triggers_for(
		element &elem, const element_configuration &config
	) {
		for (auto &trig : config.event_triggers) {
			element *subj = find_by_name(trig.identifier.subject, elem);
			if (subj == nullptr) {
				logger::get().log_warning(CP_HERE, "cannot find element with name: ", trig.identifier.subject);
				continue;
			}
			std::vector<_animation_starter> anis;
			for (auto &ani : trig.animations) {
				animation_path::bootstrapper<element> bs = elem._parse_animation_path(ani.subject);
				if (bs.parser == nullptr || bs.subject_creator == nullptr) {
					logger::get().log_warning(CP_HERE, "failed to parse animation path"); // TODO maybe print the path
					continue;
				}
				auto &&definition = bs.parser->parse_keyframe_animation(ani.definition, elem.get_manager());
				anis.emplace_back(std::move(bs.subject_creator), std::move(definition));
			}
			subj->_register_event(trig.identifier.name, [target = &elem, animations = std::move(anis)]() {
				for (auto &ani : animations) {
					target->get_manager().get_scheduler().start_animation(ani.start_for(*target), target);
				}
			});
		}
	}


	element *class_arrangements::child::construct(construction_context &ctx) const {
		element *e = ctx.logical_parent.get_manager().create_element_custom(type, element_class, configuration);
		if (e) {
			e->_logical_parent = &ctx.logical_parent;
			if (!children.empty()) { // construct children
				auto *pnl = dynamic_cast<panel*>(e);
				if (pnl == nullptr) {
					logger::get().log_warning(CP_HERE, "invalid children for non-panel type: ", type);
				} else {
					for (const child &c : children) {
						if (element * celem = c.construct(ctx)) {
							pnl->children().add(*celem);
						}
					}
				}
			}
			if (!name.empty()) { // register name
				if (!ctx.register_name(name, *e)) {
					logger::get().log_warning(CP_HERE, "duplicate element names: ", name);
				}
			}
			ctx.all_created.emplace_back(this, e); // register creation
		} else {
			logger::get().log_warning(CP_HERE, "failed to construct element with type ", type);
		}
		return e;
	}


	void class_arrangements::construct_children(panel &logparent, notify_mapping &names) const {
		construction_context ctx(logparent);
		ctx.register_name(name, logparent);
		for (const child &c : children) {
			if (element * celem = c.construct(ctx)) {
				logparent.children().add(*celem);
			}
		}
		// triggers
		ctx.register_triggers_for(logparent, configuration);
		for (auto &created : ctx.all_created) {
			ctx.register_triggers_for(*created.second, created.first->configuration);
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
	}
}
