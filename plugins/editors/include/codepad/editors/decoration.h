// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes used to store and render text decoration.

#include <codepad/ui/renderer.h>
#include <codepad/ui/element_parameters.h>

#include "overlapping_range_registry.h"

namespace codepad::editors {
	class manager;

	/// Information indicating how a decoration should be rendered.
	struct decoration_layout {
		double
			top = 0.0, ///< The top position of the first line.
			line_height = 0.0, ///< The height of a line.
			baseline = 0.0; ///< The position of the baseline, relative to the top of the line.
		/// The horizontal minimum and maximum bounds of each line.
		std::vector<std::pair<double, double>> line_bounds;
	};
	/// Basic interface for rendering decorations.
	class decoration_renderer {
	public:
		/// Default virtual destructor.
		virtual ~decoration_renderer() = default;

		/// Renderers the given decoration using the given renderer. The third parameter is the size of the rectangle
		/// that is used by the pen and brush parameters for anchor points.
		virtual void render(ui::renderer_base&, const decoration_layout&, vec2d) const = 0;

		// property path related functions
		/// Parses this renderer from the given JSON object.
		virtual void parse(const json::storage::object_t&, ui::manager&) = 0;
		/// Handles the case where the property path points to a \ref decoration_renderer and immediately ends. In
		/// this case, A custom \ref ui::typed_animation_value_handler is used for the return value.
		[[nodiscard]] static ui::property_info find_property_info_handler(
			ui::component_property_accessor_builder&, ui::manager&, manager&
		);

		/// Parses the given JSON into a \ref decoration_renderer object. Returns \p nullptr if parsing fails.
		[[nodiscard]] static std::shared_ptr<decoration_renderer> parse_static(
			const json::storage::value_t&, ui::manager&, manager&
		);
	};

	/// A source of text decoration that handles the rendering of the decorations, as well as querying information
	/// about any specific decoration object.
	class decoration_provider {
	public:
		/// Data associated with a decoration.
		struct decoration_data {
			decoration_renderer *renderer = nullptr; ///< The renderer used for rendering this decoration.
			/// A cookie used to identify this decoration and provide additional information.
			std::int32_t cookie = 0;
		};
		/// The registry type that stores all decorations.
		using registry = overlapping_range_registry<decoration_data>;

		registry decorations; ///< Stores all decorations.
		/// Renderers. This does not necessarily contain any elements, and is only here for the purpose of ensuring
		/// that the renderers outlive the contents of this provider. Specifically, this will only be read or written
		/// to by the creator of this provider.
		std::vector<std::shared_ptr<decoration_renderer>> renderers;
	};


	/// Built-in decoration renderers.
	namespace decoration_renderers {
		/// A renderer that renders the region as a continuous region with rounded corners.
		class rounded_renderer : public decoration_renderer {
		public:
			/// Renders the given decoration.
			void render(ui::renderer_base&, const decoration_layout&, vec2d) const override;

			/// Handles the \p pen, \p brush, and \p radius properties.
			[[nodiscard]] static ui::property_info find_property_info(
				ui::component_property_accessor_builder&, ui::manager&
			);
			/// Parses this renderer.
			void parse(const json::storage::object_t&, ui::manager&) override;

			ui::generic_pen_parameters pen; ///< The pen used for rendering regions.
			ui::generic_brush_parameters brush; ///< The brush used for rendering regions.
			double radius = 4.0; ///< The maximum radius of the corners.
		protected:
			/// Returns the smaller value between half the input and \ref radius.
			[[nodiscard]] double _half_radius(double v) const {
				return std::min(0.5 * v, radius);
			}
		};

		/// A decoration renderer that renders squiggles under the text.
		class squiggle_renderer : public decoration_renderer {
		public:
			/// Renders the decoration.
			void render(ui::renderer_base&, const decoration_layout&, vec2d) const override;

			/// Handles the \p pen, \p control_offset, \p offset, and \p width properties.
			[[nodiscard]] static ui::property_info find_property_info(
				ui::component_property_accessor_builder&, ui::manager&
			);
			/// Parses this renderer.
			void parse(const json::storage::object_t&, ui::manager&) override;

			ui::generic_pen_parameters pen; ///< The pen used to draw the squiggle line.
			vec2d control_offset{ 1.5, 1.5 }; ///< Offset of the control points.
			double
				offset = 3.0, ///< The offset of the center of the lines with respect to the baseline.
				width = 3.0; ///< The width of a single squiggle.
		};
	}


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
}
