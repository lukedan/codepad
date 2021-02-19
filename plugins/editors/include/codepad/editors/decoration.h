// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes used to store and render text decoration.

#include <codepad/ui/renderer.h>
#include <codepad/ui/element_parameters.h>

#include "overlapping_range_registry.h"

namespace codepad::editors {
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
	};

	/// A source of text decoration that handles the rendering of the decorations, as well as querying information
	/// about any specific decoration object.
	class decoration_provider {
	public:
		/// Data associated with a decoration.
		struct decoration_data {
			std::u8string_view description; ///< The scription of this decoration.
			decoration_renderer *renderer = nullptr; ///< The renderer used for rendering this decoration.
		};
		/// The registry type that stores all decorations.
		using registry = overlapping_range_registry<decoration_data>;

		registry decorations; ///< Stores all decorations.
	};


	/// Built-in decoration renderers.
	namespace decoration_renderers {
		/// A renderer that renders the region as a continuous region with rounded corners.
		class rounded_renderer : public decoration_renderer {
		public:
			/// Renders the given decoration.
			void render(ui::renderer_base&, const decoration_layout&, vec2d) const override;

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

			ui::generic_pen_parameters pen; ///< The pen used to draw the squiggle line.
			vec2d control_offset{ 1.5, 1.5 }; ///< Offset of the control points.
			double
				offset = 3.0, ///< The offset of the center of the lines with respect to the baseline.
				width = 3.0; ///< The width of a single squiggle.
		};
	}
}
