// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Auxiliary classes for rendering text, primitives, etc.

#include "../core/encodings.h"
#include "renderer.h"

namespace codepad::ui {
	/// Stores a batch of vertices for rendering to reduce the number of draw calls.
	struct render_batch {
	public:
		/// Adds a triangle to the render batch, given the coordinates, uvs, and colors of its vertices.
		void add_triangle(
			vec2d v1, vec2d v2, vec2d v3, vec2d uv1, vec2d uv2, vec2d uv3,
			colord c1, colord c2, colord c3
		) {
			_vs.emplace_back(v1);
			_vs.emplace_back(v2);
			_vs.emplace_back(v3);
			_uvs.emplace_back(uv1);
			_uvs.emplace_back(uv2);
			_uvs.emplace_back(uv3);
			_cs.emplace_back(c1);
			_cs.emplace_back(c2);
			_cs.emplace_back(c3);
		}
		/// Adds a rectangle to the render batch, given its coordinates, UV's, and color.
		void add_quad(rectd r, rectd uv, colord c) {
			add_quad(r, uv, c, c, c, c);
		}
		/// \overload
		///
		/// \param r The rectangle.
		/// \param uv Rectangle of the texture.
		/// \param ctl Color of the top left corner of the rectangle.
		/// \param ctr Color of the top right corner of the rectangle.
		/// \param cbl Color of the bottom left corner of the rectangle.
		/// \param cbr Color of the bottom right corner of the rectangle.
		void add_quad(rectd r, rectd uv, colord ctl, colord ctr, colord cbl, colord cbr) {
			add_quad(r, uv.xmin_ymin(), uv.xmax_ymin(), uv.xmin_ymax(), uv.xmax_ymax(), ctl, ctr, cbl, cbr);
		}
		/// \overload
		///
		/// \param r The rectangle.
		/// \param uvtl The UV of the top left corner of the rectangle.
		/// \param uvtr The UV of the top right corner of the rectangle.
		/// \param uvbl The UV of the bottom left corner of the rectangle.
		/// \param uvbr The UV of the bottom right corner of the rectangle.
		/// \param ctl Color of the top left corner of the rectangle.
		/// \param ctr Color of the top right corner of the rectangle.
		/// \param cbl Color of the bottom left corner of the rectangle.
		/// \param cbr Color of the bottom right corner of the rectangle.
		/// \remark This version allows the caller to set all four UV's of the rectangle,
		///         but since the rectangle is rendered as two traingles, settings the UV's in a
		///         non-rectangular fashion can cause improper stretching of the texture.
		void add_quad(
			rectd r, vec2d uvtl, vec2d uvtr, vec2d uvbl, vec2d uvbr, colord ctl, colord ctr, colord cbl, colord cbr
		) {
			add_triangle(r.xmin_ymin(), r.xmax_ymin(), r.xmin_ymax(), uvtl, uvtr, uvbl, ctl, ctr, cbl);
			add_triangle(r.xmax_ymin(), r.xmax_ymax(), r.xmin_ymax(), uvtr, uvbr, uvbl, ctr, cbr, cbl);
		}

		/// Renders all stored triangles using the given renderer and texture.
		void draw(renderer_base &r, const texture &tex) {
			assert_true_logical(
				tex.get_renderer() == nullptr || tex.get_renderer() == &r,
				"renderer of the texture does not match the given renderer"
			);
			r.draw_triangles(tex, _vs.data(), _uvs.data(), _cs.data(), _vs.size());
		}

		/// Allocates enough memory for the given number of triangles.
		void reserve(size_t numtris) {
			numtris *= 3;
			_vs.reserve(numtris);
			_uvs.reserve(numtris);
			_cs.reserve(numtris);
		}
		/// Clears the sprites to be rendered.
		void clear() {
			_vs.clear();
			_uvs.clear();
			_cs.clear();
		}
		/// Returns whether this batch is empty.
		bool empty() const {
			return _vs.empty();
		}
	protected:
		std::vector<vec2d>
			_vs, ///< Vertices.
			_uvs; ///< UV's.
		std::vector<colord> _cs; ///< Vertex colors.
	};
}
