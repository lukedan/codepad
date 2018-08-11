#pragma once

#include <map>
#include <functional>

#include "misc.h"

namespace codepad::ui {
	class element;
	class element_collection;
	class panel_base;

	/// Contains (animations of) metrics that determines the layout of an element in a certain state.
	class metrics_state {
	public:
		/// The state of an element's metrics. Contains the states of all
		/// \ref animated_property "animated_proprties".
		struct state {
			/// Default constructor.
			state() = default;
			/// Initializes this struct to be the initial state of the given \ref metrics_state.
			explicit state(const metrics_state &metrics) :
				current_margin(metrics.margin_animation), current_padding(metrics.padding_animation),
				current_size(metrics.size_animation) {
			}
			/// Initializes this struct to be the initial state of the given \ref metrics_state, with given initial
			/// values that are taken if the corresponding animation doesn't have one.
			state(const metrics_state &metrics, const state &s) :
				current_margin(metrics.margin_animation, s.current_margin.current_value),
				current_padding(metrics.padding_animation, s.current_padding.current_value),
				current_size(metrics.size_animation, s.current_size.current_value) {
			}

			animated_property<thickness>::state
				current_margin, ///< The state of \ref margin_animation.
				current_padding; ///< The state of \ref padding_animation.
			animated_property<vec2d>::state current_size; ///< The state of \ref size_animation.
			bool all_stationary = false; ///< Marks if all animations have finished.
		};

		/// Updates the given \ref state with the given time delta.
		void update(state &s, double dt) const {
			if (!s.all_stationary) {
				margin_animation.update(s.current_margin, dt);
				padding_animation.update(s.current_padding, dt);
				size_animation.update(s.current_size, dt);
				s.all_stationary =
					s.current_margin.stationary && s.current_padding.stationary && s.current_size.stationary;
			}
		}

		animated_property<thickness>
			margin_animation, ///< The element's margin.
			padding_animation; ///< The element's internal padding.
		animated_property<vec2d> size_animation; ///< The element's size.
		anchor elem_anchor = anchor::all; ///< The element's anchor.
		size_allocation_type
			width_alloc = size_allocation_type::automatic, ///< Determines how the element's width is allocated.
			height_alloc = size_allocation_type::automatic; ///< Determines how the element's height is allocated.
	};

	/// Class that fully determines the metrics of an \ref element.
	using element_metrics = state_mapping<metrics_state>;

	/// Controls the arrangements of composite elements.
	class class_arrangements {
	public:
		/// Used to notify the composite element when an element with a certain role is constructed.
		using construction_notify = std::function<void(element*)>;
		/// Mapping from roles to notification handlers.
		using notify_mapping = std::map<str_t, construction_notify>;

		/// Stores information about a child element.
		struct child {
			/// Constructs the corresponding child of a composite element, and schedules them to be updated if
			/// necessary.
			///
			/// \sa class_arrangements::construct_children
			void construct(element_collection&, panel_base*, notify_mapping&) const;

			element_metrics metrics; ///< The child's metrics.
			std::vector<child> children; ///< The child's children, if it's a \ref panel.
			str_t
				role, ///< The child's role in the composite element.
				type, ///< The child's type.
				element_class; ///< The child's class.
			/// The set of \ref element_state_id bits that'll be set for this child.
			element_state_id set_states = normal_element_state_id;
		};

		/// Constructs all children of a composite element with this arrangement, and marks elements for update if
		/// necessary. This function should typically be called by the composite elements themselves in
		/// \ref element::_initialize().
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

		element_metrics metrics; ///< Metrics of the composite element.
		std::vector<child> children; ///< Children of the composite element.
	};
}
