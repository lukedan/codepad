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
	class panel_base;

	/// Controls the arrangements of composite elements.
	class class_arrangements {
	public:
		/// Used to notify the composite element when an element with a certain role is constructed.
		using construction_notify = std::function<void(element*)>;
		/// Mapping from roles to notification handlers.
		using notify_mapping = std::map<str_view_t, construction_notify>;

		/// Stores information about a child element.
		struct child {
			/// Constructs the corresponding child of a composite element, and schedules them to be updated if
			/// necessary. All created elements are added to the given \p std::vector.
			///
			/// \sa class_arrangements::construct_children()
			void construct(element_collection&, panel_base&, notify_mapping&, std::vector<element*>&) const;

			element_configuration configuration; ///< The full configuration of the child.
			std::vector<child> children; ///< The child's children, if it's a \ref panel.
			str_t
				role, ///< The child's role in the composite element.
				type, ///< The child's type.
				element_class; ///< The child's class.
		};

		/// Constructs all children of a composite element with this arrangement, marks them to be updated if
		/// necessary, and calls \ref element::_on_logical_parent_constructed() on all created elements. This
		/// function should typically be called by the composite elements themselves in \ref element::_initialize().
		///
		/// \param logparent The composite element itself, which will be set as the logical parent of all children
		///                  elements. All children elements will be added to \ref panel_base::_children.
		/// \param roles A mapping between roles and notify functions, used by the composite element to properly
		///              acknowledge and register them. Successfully constructed entries are removed from the list,
		///              so that elements with duplicate roles can be detected, and the composite element can later
		///              check what items remain to determine which ones of its children are missing.
		void construct_children(panel_base &logparent, notify_mapping &roles) const;
		/// Overload of \ref construct_children(panel_base&, notify_mapping&) const that doesn't require an actual
		/// \ref notify_mapping instance. The caller can still check if the corresponding pointers are unchanged to
		/// determine if those objects are constructed.
		void construct_children(
			panel_base &logparent, std::initializer_list<notify_mapping::value_type> args
		) const {
			notify_mapping mapping(std::move(args));
			construct_children(logparent, mapping);
		}

		element_configuration configuration; ///< The configuration of this element.
		std::vector<child> children; ///< Children of the composite element.
	};
}
