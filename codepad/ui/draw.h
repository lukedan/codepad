// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Auxiliary classes for rendering text, primitives, etc.

#include "../core/encodings.h"
#include "renderer.h"

namespace codepad::ui {
	/// Stores a batch of vertices in a \ref vertex_buffer for rendering to reduce the number of draw calls.
	struct render_batch {
	public:
		constexpr static size_t minimum_capacity = 50; ///< The minimum capacity.
		/// The factor by which to enlarge the container if there's not enough capacity.
		constexpr static double enlarge_factor = 3.0;

		/// Creates a vertex buffer with the given capacity.
		render_batch(renderer_base &r, size_t cap) : _buf(r.new_vertex_buffer(cap)) {
			_ptr = r.map_vertex_buffer(_buf);
		}
		/// Creates a vertex buffer with \ref minimum_capacity.
		render_batch(renderer_base &r) : render_batch(r, minimum_capacity) {
		}
		/// Move construction.
		render_batch(render_batch &&src) : _buf(std::move(src._buf)), _ptr(src._ptr), _count(src._count) {
			src._ptr = nullptr;
			src._count = 0;
		}
		/// Move assignment.
		render_batch &operator=(render_batch &&src) {
			std::swap(_buf, src._buf);
			std::swap(_ptr, src._ptr);
			std::swap(_count, src._count);
			return *this;
		}

		/// Adds a triangle to the render batch, given the coordinates, uvs, and colors of its vertices.
		void add_triangle(
			vec2d v1, vec2d v2, vec2d v3, vec2d uv1, vec2d uv2, vec2d uv3,
			colord c1, colord c2, colord c3
		) {
			_check_resize(_count + 3);
			_add_triangle_unchecked(v1, v2, v3, uv1, uv2, uv3, c1, c2, c3);
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
			_check_resize(_count + 6);
			_add_triangle_unchecked(r.xmin_ymin(), r.xmax_ymin(), r.xmin_ymax(), uvtl, uvtr, uvbl, ctl, ctr, cbl);
			_add_triangle_unchecked(r.xmax_ymin(), r.xmax_ymax(), r.xmin_ymax(), uvtr, uvbr, uvbl, ctr, cbr, cbl);
		}

		/// Unmaps the vertex buffer. This \ref render_batch will become immutable, but can still be used for
		/// rendering.
		void freeze() {
			_buf.get_renderer()->unmap_vertex_buffer(_buf);
			_ptr = nullptr;
		}
		/// Renders all stored triangles using the given renderer and texture.
		void draw(const texture &tex) {
			assert_true_logical(
				tex.get_renderer() == _buf.get_renderer(),
				"renderer of the texture does not match that of the vertex buffer"
			);
			bool mut = _ptr == nullptr;
			if (mut) {
				_buf.get_renderer()->unmap_vertex_buffer(_buf);
			}
			_buf.get_renderer()->draw_triangles(tex, _buf, _count);
			if (mut) {
				_ptr = _buf.get_renderer()->map_vertex_buffer(_buf);
			}
		}
		/// Renders all stored triangles using the given renderer and texture, then discards the vertex buffer. This
		/// \ref render_batch should not be used afterwards.
		void draw_and_discard(const texture &tex) {
			assert_true_logical(
				tex.get_renderer() == _buf.get_renderer(),
				"renderer of the texture does not match that of the vertex buffer"
			);
			_buf.get_renderer()->unmap_vertex_buffer(_buf);
			_buf.get_renderer()->draw_triangles(tex, _buf, _count);
			_buf.get_renderer()->delete_vertex_buffer(_buf);
			_ptr = nullptr;
		}

		/// Allocates enough memory for the given number of triangles.
		void reserve_triangles(size_t tris) {
			_reserve_vertices(tris * 3);
		}
		/// Allocates enough memory for the given number of quads.
		void reserve_quads(size_t quads) {
			_reserve_vertices(quads * 6);
		}
		/// Clears the sprites to be rendered.
		void clear() {
			_count = 0;
		}
		/// Returns the associated \ref renderer_base. This will not work after \ref draw_and_discard() have been
		/// called.
		renderer_base *get_renderer() const {
			return _buf.get_renderer();
		}
		/// Returns whether this batch is empty.
		bool empty() const {
			return _count == 0;
		}
	protected:
		vertex_buffer _buf; ///< The \ref vertex_buffer.
		vertexf *_ptr = nullptr; ///< The pointer to the mapped buffer.
		size_t _count = 0; ///< The number of existing vertices.

		/// Checks if this buffer has enough capacity for the given number of vertices, and resizes it if not.
		void _check_resize(size_t n) {
			if (n > _buf.get_size()) {
				size_t newsz = _buf.get_size();
				do {
					newsz = static_cast<size_t>(newsz * enlarge_factor);
				} while (newsz < n);
				_resize_buffer(newsz);
			}
		}
		/// Adds a triangle to the back of the vertex buffer without checking its capacity.
		void _add_triangle_unchecked(
			vec2d v1, vec2d v2, vec2d v3, vec2d uv1, vec2d uv2, vec2d uv3,
			colord c1, colord c2, colord c3
		) {
			_ptr[_count] = vertexf(v1, uv1, c1);
			_ptr[_count + 1] = vertexf(v2, uv2, c2);
			_ptr[_count + 2] = vertexf(v3, uv3, c3);
			_count += 3;
		}
		/// Allocates enough memory for the given number of vertices.
		void _reserve_vertices(size_t verts) {
			if (verts > _buf.get_size()) {
				_resize_buffer(verts);
			}
		}

		/// Resizes \ref _buf while keeping its data. This is done by allocating a new buffer and copying all data to
		/// it.
		void _resize_buffer(size_t sz) {
			if (sz > _buf.get_size()) { // only if there's enough room for all data
				renderer_base &r = *_buf.get_renderer();
				vertex_buffer newbuf(r.new_vertex_buffer(sz));
				vertexf *newptr = r.map_vertex_buffer(newbuf);
				std::memcpy(newptr, _ptr, sizeof(vertexf) * _count);
				r.unmap_vertex_buffer(_buf);
				_buf = std::move(newbuf);
				_ptr = newptr;
			}
		}
	};
}
