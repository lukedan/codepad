// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes and structs related to arrangement configurations.

#include <map>
#include <vector>
#include <functional>

#include "misc.h"
#include "animation.h"
#include "element_parameters.h"

namespace codepad::ui {
	class element;
	class element_collection;
	class panel;

	/// Controls the arrangements of composite elements.
	class class_arrangements {
	public:
		/// Used to notify the composite element when an element with a certain name is constructed.
		using construction_notify = std::function<void(element*)>;
		/// Mapping from names to notification handlers.
		using notify_mapping = std::map<str_view_t, construction_notify>;

		struct child;
		/// Keeps track of the construction of a composite element.
		struct construction_context {
			/// Initializes \ref logical_parent.
			construction_context(panel &p) : logical_parent(p) {
			}

			panel &logical_parent; ///< The logical parent for all constructed elements.
			/// Records all created elements and corresponding information used to create them.
			std::vector<std::pair<const child*, element*>> all_created;
			std::map<str_view_t, element*> name_mapping; ///< The mapping from names to elements.

			/// Registers an element with the given name.
			///
			/// \return \p true if the element has been successfully registered; \p false if an element with the same
			///         name already exists.
			bool register_name(str_view_t name, element &e) {
				return name_mapping.try_emplace(name, &e).second;
			}
			/// Finds the element with the corresponding name, or returns \p self if the id is empty.
			element *find_by_name(str_view_t id, element &self) {
				if (id.empty()) {
					return &self;
				}
				if (auto it = name_mapping.find(id); it != name_mapping.end()) {
					return it->second;
				}
				return nullptr;
			}

			/// Registers all triggers of the given \ref element_configuration.
			void register_triggers_for(element&, const element_configuration&);
		};
		/// Stores information about a child element.
		struct child {
			/// Constructs and returns the corresponding child of a composite element. Also updates
			/// \ref construction_context::all_created and \ref construction_context::name_mapping accordingly.
			///
			/// \sa class_arrangements::construct_children()
			element *construct(construction_context&) const;

			element_configuration configuration; ///< The full configuration of the child.
			std::vector<child> children; ///< The child's children, if it's a \ref panel.
			str_t
				name, ///< The name of this element that's used for event registration.
				type, ///< The child's type.
				element_class; ///< The child's class.
		};

		/// Constructs all children of a composite element with this arrangement, registers event triggers, and calls
		/// \ref element::_on_logical_parent_constructed() on all created elements. This function should typically be
		/// called by the composite elements themselves in \ref element::_initialize().
		///
		/// \param logparent The composite element itself, which will be set as the logical parent of all children
		///                  elements. All children elements will be added to \ref panel::_children.
		/// \param names A mapping between names and notify functions, used by the composite element to properly
		///              acknowledge and register them. Successfully constructed entries are removed from the list,
		///              so that elements with duplicate names can be detected, and the composite element can later
		///              check what items remain to determine which ones of its children are missing.
		void construct_children(panel &logparent, notify_mapping &names) const;
		/// Overload of \ref construct_children(panel&, notify_mapping&) const that doesn't require an actual
		/// \ref notify_mapping instance. This function prints warnings for all names that have not been matched.
		void construct_children(
			panel &logparent, std::initializer_list<notify_mapping::value_type> args
		) const {
			notify_mapping mapping(std::move(args));
			construct_children(logparent, mapping);
			if (!mapping.empty()) {
				logger::get().log_warning(CP_HERE, "there are unmatched names with roles:");
				for (auto &pair : mapping) {
					logger::get().log_warning(CP_HERE, "    ", pair.first);
				}
			}
		}

		element_configuration configuration; ///< The configuration of this element.
		std::vector<child> children; ///< Children of the composite element.
		str_t name; ///< The name of this element. This is currently only used for event registration.
	};
}
