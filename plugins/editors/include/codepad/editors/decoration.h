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
}
