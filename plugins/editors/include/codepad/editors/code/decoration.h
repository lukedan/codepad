// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes used to store and render text decoration.

#include <codepad/ui/renderer.h>

#include "../overlapping_range_registry.h"

namespace codepad::editors::code {
	/// Basic interface for rendering decorations.
	class decoration_renderer {
	public:
		/// Information indicating how a decoration should be rendered.
		struct decoration_info {
			double
				top = 0.0, ///< The top position of the first line.
				line_height = 0.0, ///< The height of a line.
				baseline = 0.0; ///< The position of the baseline, relative to the top of the line.
			/// The horizontal minimum and maximum bounds of each line.
			std::vector<std::pair<double, double>> line_bounds;
		};

		/// Default virtual destructor.
		virtual ~decoration_renderer() = default;

		/// Renderers the given decoration using the given renderer.
		virtual void render(ui::renderer_base&, const decoration_info&) const = 0;
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

		overlapping_range_registry<decoration_data> decorations; ///< Stores all decorations.
	};
}
