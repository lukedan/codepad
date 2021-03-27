// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/manager.h"

/// \file
/// Implementation of certain methods of codepad::ui::manager.

#include <deque>

#include "codepad/ui/window.h"
#include "codepad/ui/element.h"
#include "codepad/ui/panel.h"
#include "codepad/ui/elements/label.h"
#include "codepad/ui/elements/popup.h"
#include "codepad/ui/elements/text_edit.h"
#include "codepad/ui/elements/tabs/manager.h"
#include "codepad/ui/elements/tabs/animated_tab_button_panel.h"

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

	element *manager::_construction_context::create_tree(std::u8string_view type, std::u8string_view cls) {
		// step 1: create the entire tree of elements and initialize them
		auto &arrangements = _manager.get_class_arrangements().get_or_default(cls);
		if (element *root = _create_element(type, cls)) {
			// add to _created manually since it's not handled by _create_children_recursive()
			_created.emplace_back(*root, *root, nullptr, arrangements.configuration);
			if (!arrangements.children.empty()) {
				if (auto *pnl = dynamic_cast<panel*>(root)) {
					// create a new name scope & add the root to the lookup table if necessary
					auto [scope_it, inserted] = _scopes.try_emplace(
						reinterpret_cast<std::uintptr_t>(root), _name_mapping()
					); // inserted must be true
					if (!arrangements.configuration.name.empty()) {
						scope_it->second.emplace(arrangements.configuration.name, pnl);
					}
					_create_children_recursive(*pnl, arrangements.children, *root, scope_it->second);
				} else {
					logger::get().log_error(CP_HERE) << "children specified for non-panel elements";
				}
			}
			// step 2: handle references
			for (const auto &elem : _created) {
				if (elem.class_config) {
					_handle_references(*elem.elem, *elem.elem, elem.class_config->references);
				}
				_handle_references(*elem.elem, *elem.custom_subtree_root, elem.custom_config->references);
			}
			// step 3: handle events
			for (const auto &elem : _created) {
				if (elem.class_config) {
					_handle_event_triggers(*elem.elem, *elem.elem, elem.class_config->event_triggers);
				}
				_handle_event_triggers(*elem.elem, *elem.custom_subtree_root, elem.custom_config->event_triggers);
			}
			// step 4: handle properties
			for (const auto &elem : _created) {
				if (elem.class_config) {
					_handle_properties(*elem.elem, elem.class_config->properties);
				}
				_handle_properties(*elem.elem, elem.custom_config->properties);
			}
			return root;
		}
		return nullptr;
	}

	element *manager::_construction_context::_create_element(std::u8string_view type, std::u8string_view cls) {
		auto it = _manager._element_registry.find(type);
		if (it == _manager._element_registry.end()) {
			logger::get().log_error(CP_HERE) << "failed to create element of type: " << type;
			return nullptr;
		}
		element *elem = it->second(); // the constructor must not use element::_manager
		elem->_manager = &_manager;
		elem->_hotkeys = &_manager.get_class_hotkeys().get_or_default(cls);
		elem->_initialize();
#ifdef CP_CHECK_USAGE_ERRORS
		assert_true_usage(elem->_initialized, "element::_initialize() must be called by derived classes");
#endif
		return elem;
	}

	void manager::_construction_context::_create_children_recursive(
		panel &parent, const std::vector<class_arrangements::child> &children,
		element &custom_subtree_root, _name_mapping &scope
	) {
		for (auto &child : children) {
			if (element *elem = _create_element(child.type, child.element_class)) {
				parent.children().add(*elem);
				if (!child.configuration.name.empty()) {
					auto [it, inserted] = scope.emplace(child.configuration.name, elem);
					if (!inserted) {
						logger::get().log_error(CP_HERE) << "duplicate element names: " << child.configuration.name;
					}
				}

				const class_arrangements *default_arrangements = nullptr;
				if (!child.element_class.empty()) {
					default_arrangements = _manager.get_class_arrangements().get(child.element_class);
					if (!default_arrangements) {
						logger::get().log_error(CP_HERE) << "element class not found: " << child.element_class;
					}
				}
				auto *default_configuration = default_arrangements ? &default_arrangements->configuration : nullptr;
				_created.emplace_back(*elem, custom_subtree_root, default_configuration, child.configuration);

				// create the children of this element
				if ((default_arrangements && !default_arrangements->children.empty()) || !child.children.empty()) {
					if (auto *pnl = dynamic_cast<panel*>(elem)) {
						if (default_arrangements) {
							// create a new name scope & add the root to the lookup table if necessary
							auto [scope_it, inserted] = _scopes.emplace(
								reinterpret_cast<std::uintptr_t>(pnl), _name_mapping()
							); // inserted must be true
							if (!default_arrangements->configuration.name.empty()) {
								scope_it->second.emplace(default_arrangements->configuration.name, pnl);
							}
							_create_children_recursive(*pnl, default_arrangements->children, *pnl, scope_it->second);
						}
						_create_children_recursive(*pnl, child.children, custom_subtree_root, scope);
					} else {
						logger::get().log_error(CP_HERE) << "children specified for non-panel elements";
					}
				}
			}
		}
	}

	element *manager::_construction_context::_find_element_by_name(
		element &root, const std::vector<std::u8string> &name
	) const {
		element *current = &root;
		for (const auto &level : name) {
			auto it = _scopes.find(reinterpret_cast<std::uintptr_t>(current));
			if (it == _scopes.end()) { // no scope associated with the element
				current = nullptr;
				break;
			}
			auto next = it->second.find(level);
			if (next == it->second.end()) { // name not found
				current = nullptr;
				break;
			}
			current = next->second;
		}
		if (current) {
			return current;
		}
		auto entry = logger::get().log_warning(CP_HERE);
		entry << "failed to find element by name: ";
		bool first = true;
		for (const auto &level : name) {
			if (!first) {
				entry << ".";
			}
			first = false;
			entry << level;
		}
		return nullptr;
	}

	void manager::_construction_context::_register_trigger_for(
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
		if (!trigger._register_event(ev.event_name, [target = &affected, animations = std::move(anis)]() {
			for (auto &ani : animations) {
				target->_start_animation(ani.animation->start(target, ani.property.accessor));
			}
		})) {
			logger::get().log_error(CP_HERE) << "unknown event name: " << ev.event_name;
		}
	}

	void manager::_construction_context::_handle_references(
		element &elem, element &root, const std::vector<element_configuration::reference> &refs
	) const {
		for (const auto &ref : refs) {
			auto *ref_elem = _find_element_by_name(root, ref.name);
			if (!elem._handle_reference(ref.role, ref_elem)) {
				auto entry = logger::get().log_error(CP_HERE);
				entry << "unhandled reference: " << ref.role << " -> ";
				bool first = true;
				for (const auto &part : ref.name) {
					if (!first) {
						entry << ".";
					}
					first = false;
					entry << part;
				}
			}
		}
	}

	void manager::_construction_context::_handle_event_triggers(
		element &elem, element &root, const std::vector<element_configuration::event_trigger> &triggers
	) const {
		for (const auto &trigger : triggers) {
			auto *target_elem = &elem;
			if (!trigger.instigator.empty()) {
				target_elem = _find_element_by_name(root, trigger.instigator);
				if (!target_elem) {
					logger::get().log_error(CP_HERE) << "failed to find event trigger element";
					continue;
				}
			}
			_register_trigger_for(*target_elem, elem, trigger);
		}
	}

	void manager::_construction_context::_handle_properties(
		element &elem, const std::vector<element_configuration::property_value> &attrs
	) {
		for (const auto &attr : attrs) {
			if (attr.property.empty()) {
				logger::get().log_error(CP_HERE) << "empty property path";
				continue;
			}
			property_info prop = elem._find_property_path(attr.property);
			if (prop.accessor == nullptr || prop.value_handler == nullptr) {
				logger::get().log_error(CP_HERE) <<
					"failed to find property corresponding to property path: " <<
					property_path::to_string(attr.property.begin(), attr.property.end());
				continue;
			}
			prop.value_handler->set_value(&elem, *prop.accessor, attr.value);
		}
	}


	manager::manager(settings &s) : _settings(s) {
		// TODO use reflection in C++23 (?) for everything below
		register_transition_function(u8"linear", transition_functions::linear);
		register_transition_function(u8"smoothstep", transition_functions::smoothstep);
		register_transition_function(u8"concave_quadratic", transition_functions::concave_quadratic);
		register_transition_function(u8"convex_quadratic", transition_functions::convex_quadratic);
		register_transition_function(u8"concave_cubic", transition_functions::concave_cubic);
		register_transition_function(u8"convex_cubic", transition_functions::convex_cubic);


		register_element_type<element>();
		register_element_type<panel>();
		register_element_type<stack_panel>();
		register_element_type<label>();
		register_element_type<button>();
		register_element_type<popup>();
		register_element_type<scrollbar>();
		register_element_type<scroll_viewport>();
		register_element_type<scroll_view>();
		register_element_type<text_edit>();
		register_element_type<textbox>();
		register_element_type<window>();

		register_element_type<tabs::split_panel>();
		register_element_type<tabs::tab_button>();
		register_element_type<tabs::tab>();
		register_element_type<tabs::drag_destination_selector>();
		register_element_type<tabs::host>();
		register_element_type<tabs::animated_tab_buttons_panel>();


		_scheduler.get_hotkey_listener().triggered += [this](hotkey_info &info) {
			const auto *cmd = _commands.find_command(info.command.identifier);
			if (cmd) {
				(*cmd)(info.subject, info.command.arguments);
			} else {
				logger::get().log_warning(CP_HERE) << "invalid command: " << info.command.identifier;
			}
		};
	}
}
