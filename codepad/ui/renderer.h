// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes used to generate graphics of the user interface.

#include <functional>
#include <algorithm>
#include <cstring>

#include "../core/misc.h"

namespace codepad::ui {
	class window_base;
	class renderer_base;

	/// The factor that a color (source or destination) is blended with.
	enum class blend_factor {
		zero, ///< The color isn't involved.
		one, ///< The color is not modified.
		source_alpha, ///< The color is multiplied by the alpha of the source color.
		/// The color is multiplied by one minus the alpha of the source color.
		one_minus_source_alpha,
		destination_alpha, ///< The color is multiplied by the alpha of the destination color.
		/// The color is multiplied by one minus the alpha of the destination color.
		one_minus_destination_alpha,
		source_color, ///< Each channel of the color is multiplied by that of the source color.
		/// Each channel of the color is multiplied one minus that of the source color.
		one_minus_source_color,
		destination_color, ///< Each channel of the color is multiplied by that of the destination color.
		/// Each channel of the color is multiplied one minus that of the destination color.
		one_minus_destination_color
	};
	/// The function used to blend two colors (source and destination) together.
	/// The source color is the one to be drawn to the buffer, and the destination color
	/// is the one already in the buffer.
	struct blend_function {
		/// Initializes the \ref blend_function to the one most commonly used.
		blend_function() = default;
		/// Initializes the struct with the given \ref blend_factor "blend_factors".
		blend_function(blend_factor src, blend_factor dst) : source_factor(src), destination_factor(dst) {
		}

		blend_factor
			/// The factor used to blend the source color.
			source_factor = blend_factor::source_alpha,
			/// The factor used to blend the destination color.
			destination_factor = blend_factor::one_minus_source_alpha;
	};

	/// A texture. The texture's automatically removed from the renderer when this object is disposed.
	struct texture {
		friend renderer_base;
	public:
		/// The type of the underlying ID of the texture.
		using id_t = size_t;

		/// Initializes the \ref texture_base to empty.
		texture() = default;
		/// Move constructor.
		texture(texture &&t) noexcept : _id(t._id), _rend(t._rend), _w(t._w), _h(t._h) {
			t._rend = nullptr;
			t._w = t._h = 0;
		}
		/// No copy construction.
		texture(const texture&) = delete;
		/// Move assignment.
		texture &operator=(texture &&t) noexcept {
			std::swap(_id, t._id);
			std::swap(_rend, t._rend);
			std::swap(_w, t._w);
			std::swap(_h, t._h);
			return *this;
		}
		/// No copy assignment.
		texture &operator=(const texture&) = delete;
		/// Automatically calls renderer_base::delete_texture to dispose of the underlying texture if it's not empty.
		~texture();

		/// Returns the renderer that created this \ref texture_base.
		renderer_base *get_renderer() const {
			return _rend;
		}
		/// Returns the width, in pixels, of the texture.
		size_t get_width() const {
			return _w;
		}
		/// Returns the height, in pixels, of the texture.
		size_t get_height() const {
			return _h;
		}

		/// Checks if the texture is non-empty.
		bool has_contents() const {
			return _rend != nullptr;
		}
	protected:
		/// Protected constructor that \ref renderer_base uses to initialize the texture.
		texture(id_t id, renderer_base &r, size_t w, size_t h) : _id(id), _rend(&r), _w(w), _h(h) {
		}

		id_t _id = 0; ///< The underlying ID of the texture used by the renderer.
		renderer_base *_rend = nullptr; ///< The renderer that created this texture.
		size_t
			_w = 0, ///< The width of the texture.
			_h = 0; ///< The height of the texture.
	};

	/// A buffer that can be drawn onto, and can be then used as a texture.
	struct frame_buffer {
		friend class renderer_base;
	public:
		/// The type of the underlying ID of the \ref frame_buffer.
		using id_t = size_t;

		/// Initializes the \ref frame_buffer to empty.
		frame_buffer() = default;
		/// Move constructor.
		frame_buffer(frame_buffer &&r) noexcept : _id(r._id), _tex(std::move(r._tex)) {
		}
		/// No copy construction.
		frame_buffer(const frame_buffer&) = delete;
		/// Move assignment.
		frame_buffer &operator=(frame_buffer &&r) noexcept {
			std::swap(_id, r._id);
			_tex = std::move(r._tex);
			return *this;
		}
		/// No copy assignment.
		frame_buffer &operator=(const frame_buffer&) = delete;
		/// Automatically calls \ref renderer_base::delete_frame_buffer to dispose of the underlying frame buffer if
		/// it's not empty.
		~frame_buffer();

		/// Returns the \ref texture that contains the contents of the frame buffer.
		const texture &get_texture() const {
			return _tex;
		}

		/// Checks if the frame buffer is empty.
		bool has_contents() const {
			return _tex.has_contents();
		}
	protected:
		/// Protected constructor that \ref renderer_base uses to initialize the frame buffer.
		frame_buffer(id_t rid, texture &&t) : _id(rid), _tex(std::move(t)) {
		}

		id_t _id = 0; ///< The underlying ID of the frame buffer.
		texture _tex; ///< The underlying texture.
	};

	/// Stores data about a vertex.
	template <typename PosType, typename UVType, typename ColorType> struct basic_vertex {
		/// Default constructor.
		basic_vertex() = default;
		/// Initializes all fields of this struct.
		template <typename AltPos, typename AltUV, typename AltColor> basic_vertex(
			vec2<AltPos> p, vec2<AltUV> u, color<AltColor> c
		) noexcept : position(p.convert<PosType>()), uv(u.convert<UVType>()), color(c.convert<ColorType>()) {
		}

		vec2<PosType> position; ///< The vertex position.
		vec2<UVType> uv; ///< The UV coordinates.
		color<ColorType> color; ///< The vertex color.
	};
	/// Vertices whose data is all represented using floats. Vertices of this type are used in
	/// \ref vertex_buffer "vertex buffers".
	using vertexf = basic_vertex<float, float, float>;
	/// A buffer used to hold vertices.
	struct vertex_buffer {
		friend class renderer_base;
	public:
		/// The type of the underlying identifier.
		using id_t = size_t;

		/// Initializes this \ref vertex_buffer to empty.
		vertex_buffer() = default;
		/// Move constructor.
		vertex_buffer(vertex_buffer &&src) noexcept : _id(src._id), _rend(src._rend), _size(src._size) {
			src._rend = nullptr;
			src._size = 0;
		}
		/// No copy construction.
		vertex_buffer(const vertex_buffer&) = delete;
		/// Move assignment.
		vertex_buffer &operator=(vertex_buffer &&src) noexcept {
			std::swap(_id, src._id);
			std::swap(_rend, src._rend);
			std::swap(_size, src._size);
			return *this;
		}
		/// No copy assignment.
		vertex_buffer &operator=(const vertex_buffer&) = delete;
		/// Automatically disposes of this vertex buffer.
		~vertex_buffer();

		/// Returns the \ref renderer_base that created this vertex buffer.
		renderer_base *get_renderer() const {
			return _rend;
		}
		/// Returns the number of \ref vertexf this vertex buffer can hold.
		size_t get_size() const {
			return _size;
		}

		/// Checks if this vertex buffer is empty.
		bool has_contents() const {
			return _rend != nullptr;
		}
	protected:
		/// Initializes all fields of this struct.
		vertex_buffer(id_t id, renderer_base &r, size_t s) : _id(id), _rend(&r), _size(s) {
		}

		id_t _id = 0; ///< The identifier of this vertex buffer.
		renderer_base *_rend = nullptr; ///< The renderer that created this vertex buffer.
		size_t _size = 0; ///< The size of this vertex buffer.
	};


	/// The base class of all renderers used to draw the user interface.
	class renderer_base {
		friend class window_base;
	public:
		/// Default virtual destructor.
		virtual ~renderer_base() = default;

		/// Called to begin rendering to a \ref window_base. The contents of the window is cleared.
		virtual void begin(const window_base&) = 0;
		/// Pushes a clip onto the stack. Drawing only applies to the union of all clips of
		/// the current render target.
		virtual void push_clip(recti) = 0;
		/// Pops a clip from the stack.
		virtual void pop_clip() = 0;

		/// Draws an array of triangles from a \ref vertex_buffer. Every three elements of the arrays are drawn as
		/// one triangle.
		///
		/// \param tex The texture used for rendering the triangles.
		/// \param buf The vertex buffer.
		/// \param n Only the first \p n vertices will be taken into account.
		virtual void draw_triangles(const texture &tex, const vertex_buffer &buf, size_t n) = 0;
		// TODO either remove draw_lines or add vertex buffer edition
		/// Draws an array of lines. Every two elements of the arrays are drawn as one line.
		///
		/// \param vs The positions of the vertices of the lines.
		/// \param uvs The texture coordinates of the vertices of the lines.
		/// \param n The total number of vertices, i.e., two times the number of lines.
		virtual void draw_lines(const vec2d *vs, const colord *uvs, size_t n) = 0;
		/// Ends the current render target, which can either be a window or a \ref frame_buffer.
		virtual void end() = 0;

		/// Creaes a texture from the given data.
		///
		/// \param w The width of the texture.
		/// \param h The height of the texture.
		/// \param pixels The pixel data, in 8-bit RGBA format. If this \p nullptr, the texture should still be
		///               allocated but the user should not use it until \ref update_texture() has been called.
		virtual texture new_texture(size_t w, size_t h, const std::uint8_t *pixels = nullptr) = 0;
		/// Updates the contents of the texture.
		virtual void update_texture(texture&, const std::uint8_t*) = 0;
		/// Deletes the specified texture. The texture will become empty. Users normally don't have to call this
		/// manually.
		///
		/// \see texture::~texture()
		virtual void delete_texture(texture&) = 0;

		/// Creates a new \ref frame_buffer of the given size.
		///
		/// \param w The width of the frame buffer.
		/// \param h The height of the frame buffer.
		virtual frame_buffer new_frame_buffer(size_t w, size_t h) = 0;
		/// Deletes the given frame buffer. The frame buffer will become empty.
		/// Users normally don't have to normally call this.
		///
		/// \see frame_buffer::~frame_buffer()
		virtual void delete_frame_buffer(frame_buffer&) = 0;
		/// Starts to render to the given frame buffer. The contents of the frame buffer is cleared.
		/// Call \ref end() to finish rendering and make the underlying \ref texture of the frame buffer available.
		virtual void begin_frame_buffer(const frame_buffer&) = 0;
		/// Starts to render to the given frame buffer without clearning its contents.
		/// Call \ref end() to finish rendering and make the underlying \ref texture of the frame buffer available.
		virtual void continue_frame_buffer(const frame_buffer&) = 0;

		/// Creates a new \ref vertex_buffer that contains the given number of \ref vertexf.
		///
		/// \todo Usage hints?
		virtual vertex_buffer new_vertex_buffer(size_t) = 0;
		/// Deletes the given \ref vertex_buffer. The buffer will become empty.
		virtual void delete_vertex_buffer(vertex_buffer&) = 0;
		/// Maps the given \ref vertex_buffer.
		virtual vertexf *map_vertex_buffer(vertex_buffer&) = 0;
		/// Unmaps the given \ref vertex_buffer.
		virtual void unmap_vertex_buffer(vertex_buffer&) = 0;

		/// Pushes a matrix onto the stack. The matrix is then used for vertex transformations.
		virtual void push_matrix(const matd3x3&) = 0;
		/// Multiplies the matrix with the current matrix and pushes the result onto the stack.
		virtual void push_matrix_mult(const matd3x3&) = 0;
		/// Obtains the current matrix used for vertex transformations.
		virtual matd3x3 top_matrix() const = 0;
		/// Pops a matrix from the stack.
		virtual void pop_matrix() = 0;

		/// Pushes a blend function onto the stack. The blend function is used for render operations.
		virtual void push_blend_function(blend_function) = 0;
		/// Pops a blend function from the stack.
		virtual void pop_blend_function() = 0;
		/// Obtains the current blend function used for render operations.
		virtual blend_function top_blend_function() const = 0;
	protected:
		/// Called when a new window is created.
		virtual void _new_window(window_base&) = 0;
		/// Called when a window is deleted.
		virtual void _delete_window(window_base&) = 0;

		/// Returns the underlying identifier of the \ref texture.
		inline static typename texture::id_t _get_id(const texture &t) {
			return t._id;
		}
		/// Return the underlying identifier of the \ref frame_buffer.
		inline static frame_buffer::id_t _get_id(const frame_buffer &f) {
			return f._id;
		}
		/// Return the underlying identifier of the \ref vertex_buffer.
		inline static vertex_buffer::id_t _get_id(const vertex_buffer &v) {
			return v._id;
		}
		/// Creates a \ref texture_base from the given arguments.
		texture _make_texture(texture::id_t id, size_t w, size_t h) {
			return texture(id, *this, w, h);
		}
		/// Creates a \ref frame_buffer from the given arguments.
		frame_buffer _make_frame_buffer(frame_buffer::id_t id, texture &&tex) {
			return frame_buffer(id, std::move(tex));
		}
		/// Creates a \ref vertex_buffer from the given arguments.
		vertex_buffer _make_vertex_buffer(vertex_buffer::id_t id, size_t sz) {
			return vertex_buffer(id, *this, sz);
		}
		/// Erases the contents of the given \ref texture_base.
		inline static void _erase_texture(texture &t) {
			t._id = 0;
			t._rend = nullptr;
			t._w = t._h = 0;
		}
		/// Erases the contents of the given \ref frame_buffer.
		inline static void _erase_frame_buffer(frame_buffer &fb) {
			fb._id = 0;
			_erase_texture(fb._tex);
		}
		/// Erases the contents of the given \ref vertex_buffer.
		inline static void _erase_vertex_buffer(vertex_buffer &vb) {
			vb._id = 0;
			vb._rend = nullptr;
			vb._size = 0;
		}
	};

	/// Loads an image from the given file name, and returns the corresponding texture created
	/// with the given renderer. This function is implemented in a platform-specific manner.
	texture load_image(renderer_base&, const std::filesystem::path&);

	inline texture::~texture() {
		if (has_contents()) {
			_rend->delete_texture(*this);
		}
	}
	inline frame_buffer::~frame_buffer() {
		if (has_contents()) {
			_tex.get_renderer()->delete_frame_buffer(*this);
		}
	}
	inline vertex_buffer::~vertex_buffer() {
		if (has_contents()) {
			_rend->delete_vertex_buffer(*this);
		}
	}
}
