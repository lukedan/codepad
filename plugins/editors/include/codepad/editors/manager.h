// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Declaration and implementation of manager classes.

#include <stack>

#include <codepad/os/filesystem.h>

#include "code/interpretation.h"
#include "code/caret_set.h"
#include "code/contents_region.h"
#include "binary/contents_region.h"

#include "buffer.h"
#include "buffer_manager.h"
#include "decoration.h"
#include "theme_manager.h"

namespace codepad::editors {
	/// A registry for decoration renderers.
	class decoration_renderer_registry {
	public:
		/// Information about a specific type of decoration renderer.
		struct renderer_type_info {
			/// Function that creates a new \ref decoration_renderer.
			using creator = std::function<std::shared_ptr<decoration_renderer>()>;
			/// Function used to retrieve property path information.
			using property_path_finder = std::function<
				ui::property_info(ui::component_property_accessor_builder&, ui::manager&)
			>;

			/// Default constructor.
			renderer_type_info() = default;
			/// Initializes all fields of this struct.
			renderer_type_info(creator c, property_path_finder p) :
				create(std::move(c)), property_finder(std::move(p)) {
			}

			creator create; ///< Creator.
			property_path_finder property_finder; ///< Property finder.
		};

		/// Registers a new renderer type. Does nothing if a type with the existing name already exists.
		///
		/// \return \p true if no decoration renderer with the same name exists.
		bool register_renderer(std::u8string name, renderer_type_info type) {
			auto [it, inserted] = _mapping.try_emplace(std::move(name), std::move(type));
			return inserted;
		}
		/// Registers a renderer given by the template parameter. This function is only valid for renderers that have
		/// a default constructor. This function expects a static member function named \p find_property_info() that
		/// will be used as part of \ref renderer_type_info::property_finder. In the property finder before invoking
		/// \p Renderer::find_property_info(), A
		/// \ref ui::property_path::address_accessor_components::dynamic_cast_component is added automatically.
		template <typename Renderer> bool register_renderer(std::u8string name) {
			return register_renderer(std::move(name), renderer_type_info(
				[]() {
					return std::make_shared<Renderer>();
				},
				[](ui::component_property_accessor_builder &builder, ui::manager &man) {
					builder.make_append_accessor_component<
						ui::property_path::address_accessor_components::dynamic_cast_component<
							Renderer, decoration_renderer
						>
					>();
					return Renderer::find_property_info(builder, man);
				}
			));
		}
		/// Unregisters the given renderer type.
		///
		/// \return Whether the type has been successfully unregistered.
		bool unregister_renderer(std::u8string_view name) {
			if (auto it = _mapping.find(name); it != _mapping.end()) {
				_mapping.erase(it);
				return true;
			}
			return false;
		}

		/// Finds the creation function for decoration renderers with the given type.
		[[nodiscard]] const renderer_type_info *find_renderer_type(std::u8string_view name) const {
			auto it = _mapping.find(name);
			if (it == _mapping.end()) {
				return nullptr;
			}
			return &it->second;
		}
		/// Creates a \ref decoration_renderer of the given type. Returns \p nullptr if no such type has been
		/// registered.
		[[nodiscard]] std::shared_ptr<decoration_renderer> create_renderer(std::u8string_view name) const {
			if (auto *info = find_renderer_type(name)) {
				if (info->create) {
					return info->create();
				}
			}
			return nullptr;
		}
	protected:
		/// Mapping between decoration renderer type names and creator functions.
		std::unordered_map<std::u8string, renderer_type_info, string_hash<>, std::equal_to<>> _mapping;
	};


	/// Manages everything related to editors. Essentially a hub for \ref buffer_manager, \ref encoding_manager,
	class manager {
	public:
		/// Constructor.
		explicit manager(ui::manager &man) : buffers(this), themes(man) {
		}

		/// Registers built-in interaction modes.
		void register_builtin_interactions() {
			code_interactions.mapping.emplace(
				u8"prepare_drag", []() {
					return std::make_unique<
						interaction_modes::mouse_prepare_drag_mode_activator<code::caret_set>
					>();
				}
			);
			code_interactions.mapping.emplace(
				u8"single_selection", []() {
					return std::make_unique<
						interaction_modes::mouse_single_selection_mode_activator<code::caret_set>
					>();
				}
			);

			binary_interactions.mapping.emplace(
				u8"prepare_drag", []() {
					return std::make_unique<
						interaction_modes::mouse_prepare_drag_mode_activator<binary::caret_set>
					>();
				}
			);
			binary_interactions.mapping.emplace(
				u8"single_selection", []() {
					return std::make_unique<
						interaction_modes::mouse_single_selection_mode_activator<binary::caret_set>
					>();
				}
			);
		}

		/// Registers built-in decoration renderers.
		void register_builtin_decoration_renderers() {
			decoration_renderers.register_renderer<decoration_renderers::rounded_renderer>(
				u8"rounded_decoration_renderer"
			);
			decoration_renderers.register_renderer<decoration_renderers::squiggle_renderer>(
				u8"squiggle_decoration_renderer"
			);
		}


		buffer_manager buffers; ///< Manager of all buffers.
		code::encoding_registry encodings; ///< Encodings.
		/// \ref interaction_mode_registry for code editors.
		interaction_mode_registry<code::caret_set> code_interactions;
		/// \ref interaction_mode_registry for binary editors.
		interaction_mode_registry<binary::caret_set> binary_interactions;
		decoration_renderer_registry decoration_renderers; ///< A registry of decoration renderer types.
		theme_manager themes; ///< Theme information of different languages.
	};
}
