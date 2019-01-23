// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes for defining and rendering the visuals of elements.

#include <map>
#include <unordered_map>
#include <memory>

#include "../core/misc.h"
#include "../os/filesystem.h"
#include "misc.h"
#include "renderer.h"

namespace codepad::ui {
	/// A layer in the rendering of objects.
	struct visual_layer {
		/// The state of a layer. Contains the states of all its \ref animated_property "animated properties".
		struct state {
		public:
			/// Default constructor.
			state() = default;
			/// Initializes all property states with the properties of the given layer.
			explicit state(const visual_layer &layer) :
				texture(layer.texture_animation), margin(layer.margin_animation),
				color(layer.color_animation), size(layer.size_animation) {
			}
			/// Initializes all property states with the properties of the layer and the previous state.
			state(const visual_layer &layer, const state &old) :
				texture(layer.texture_animation),
				margin(layer.margin_animation, old.margin.current_value),
				color(layer.color_animation, old.color.current_value),
				size(layer.size_animation, old.size.current_value) {
			}

			animated_property<texture>::state texture; ///< The state of \ref texture_animation.
			animated_property<thickness>::state margin; ///< The state of \ref margin_animation.
			animated_property<colord>::state color; ///< The state of \ref color_animation.
			animated_property<vec2d>::state size; ///< The state of \ref size_animation.
			bool all_stationary = false; ///< Marks if all animations have finished.
		};
		/// The type of a layer.
		enum class type {
			solid, ///< The contents are rendered as a single solid block.
			/// The contents are divided by a grid specified by \ref margin_animation and \ref rect_anchor.
			/// \ref margin_animation is treated as if it were in pixels, which corresponds to pixels in the
			/// textures specified by \ref texture_animation.
			grid
		};

		/// Returns the center region of the layout, calculated from the given rectangle.
		/// If \ref layer_type is type::solid, then the rectangle returned contains the coordinates
		/// where the texture is drawn. Otherwise, the rectangle is the part where part of the texture
		/// is stretched to fill.
		///
		/// \param s The state of the layer.
		/// \param rgn The region of the layout.
		rectd get_center_rect(const state &s, rectd rgn) const;
		/// Updates the given state.
		///
		/// \param s The state to be updated.
		/// \param dt The time since the state was last updated.
		void update(state &s, double dt) const {
			if (!s.all_stationary) {
				texture_animation.update(s.texture, dt);
				margin_animation.update(s.margin, dt);
				color_animation.update(s.color, dt);
				size_animation.update(s.size, dt);
				s.all_stationary =
					s.texture.stationary && s.color.stationary &&
					s.size.stationary && s.margin.stationary;
			}
		}
		/// Renders an object with the given layout and state.
		///
		/// \param r The renderer.
		/// \param rgn The layout of the object.
		/// \param s The current state of the layer.
		void render(renderer_base &r, rectd rgn, const state &s) const;

		animated_property<texture> texture_animation; ///< Textures(s) used to render the layer.
		animated_property<thickness> margin_animation; ///< The margin used to calculate the center region.
		animated_property<colord> color_animation; ///< The color of the layer.
		animated_property<vec2d> size_animation; ///< The size used to calculate the center region.
		anchor rect_anchor = anchor::all; ///< The anchor of the center region.
		size_allocation_type
			/// The allocation type of the width of \ref size_animation.
			width_alloc = size_allocation_type::automatic,
			/// The allocation type of the height of \ref size_animation.
			height_alloc = size_allocation_type::automatic;
		type layer_type = type::solid; ///< Determines how the center region is handled.
	};
	namespace json_object_parsers {
		/// Parses \ref visual_layer::type. The value can either be \p "grid" or \p "solid".
		template <> inline visual_layer::type parse<visual_layer::type>(const json::node_t &obj) {
			if (obj.IsString()) {
				str_t s = json::get_as_string(obj);
				if (s == CP_STRLIT("grid")) {
					return visual_layer::type::grid;
				}
				if (s == CP_STRLIT("solid")) {
					return visual_layer::type::solid;
				}
			}
			return visual_layer::type::solid;
		}
	}

	/// Stores all layers of an object's visual in a certain state.
	class visual_state {
	public:
		/// The state of the visual of an object. Stores the states of all layers, and
		/// additional information to determine if any layer needs to be updated.
		struct state {
			/// Default constructor.
			state() = default;
			/// Initializes the state with the given visual state. All layer states will be initialized
			/// with corresponding layers of the visual state.
			state(const visual_state&);
			/// Initializes the state with the visual state and the previous state. All layer states will
			/// be initialized with corresponding layers of the visual state and layer states in the
			/// previous state. The number of resulting layer states will always be equal to the number of
			/// layers in the visual state. If the previous state has less layers then remaining layer
			/// states will be initialized without a layer state, and if the previous state has more layers
			/// the redundant layers in the end will be ignored.
			state(const visual_state&, const state&);

			std::vector<visual_layer::state> layer_states; ///< The states of all layers.
			bool all_stationary = false; ///< Indicates whether all animations have finished.
		};

		/// Updates the given state.
		///
		/// \param s The state to be updated.
		/// \param dt The time since the state was last updated.
		void update(state &s, double dt) const;
		/// Renders in the given region with the given state.
		void render(renderer_base &r, rectd rgn, const state &s) const {
			auto j = s.layer_states.begin();
			for (auto i = layers.begin(); i != layers.end(); ++i, ++j) {
				assert_true_usage(j != s.layer_states.end(), "invalid layer state data");
				i->render(r, rgn, *j);
			}
		}

		std::vector<visual_layer> layers; ///< All layers of the visual state.
	};

	/// Class that fully determines the visual of an \ref element.
	using class_visual = state_mapping<visual_state>;
}
