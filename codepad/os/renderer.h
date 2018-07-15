#pragma once

/// \file
/// Classes used to generate graphics of the user interface.

#include <functional>
#include <algorithm>
#include <cstring>

#include "../core/misc.h"

namespace codepad::os {
	class window_base;
	class renderer_base;
	class opengl_renderer_base;

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

	/// Base struct of all textures.
	///
	/// \tparam IsNormal \p false if the texture is a character texture, or \p true if it is a normal one.
	template <bool IsNormal> struct texture_base {
		friend class renderer_base;
	public:
		/// Indicates whether the texture is a normal texture.
		constexpr static bool is_normal_texture = IsNormal;

		/// The type of the underlying ID of the texture.
		using id_t = size_t;

		/// Initializes the \ref texture_base to empty.
		texture_base() = default;
		/// Move constructor.
		texture_base(texture_base &&t) noexcept : _id(t._id), _rend(t._rend), _w(t._w), _h(t._h) {
			t._id = 0;
			t._rend = nullptr;
			t._w = t._h = 0;
		}
		/// No copy construction.
		texture_base(const texture_base&) = delete;
		/// Move assignment.
		texture_base &operator=(texture_base &&t) noexcept {
			std::swap(_id, t._id);
			std::swap(_rend, t._rend);
			std::swap(_w, t._w);
			std::swap(_h, t._h);
			return *this;
		}
		/// No copy assignment.
		texture_base &operator=(const texture_base&) = delete;
		/// Automatically calls renderer_base::delete_texture to dispose of the underlying texture if it's not empty.
		~texture_base();

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
		bool has_content() const {
			return _rend != nullptr;
		}
	protected:
		/// Protected constructor that \ref renderer_base uses to initialize the texture.
		texture_base(id_t id, renderer_base *r, size_t w, size_t h) : _id(id), _rend(r), _w(w), _h(h) {
		}

		id_t _id = 0; ///< The underlying ID of the texture used by the renderer.
		renderer_base *_rend = nullptr; ///< The renderer that created this texture.
		size_t
			_w = 0, ///< The width of the texture.
			_h = 0; ///< The height of the texture.
	};
	/// Normal textures.
	using texture = texture_base<true>;
	/// Character textures.
	using char_texture = texture_base<false>;

	/// A buffer that can be drawn onto, and can be then used as a texture.
	struct framebuffer {
		friend class renderer_base;
	public:
		/// The type of the underlying ID of the \ref framebuffer.
		using id_t = size_t;

		/// Initializes the \ref framebuffer to empty.
		framebuffer() = default;
		/// Move constructor.
		framebuffer(framebuffer &&r) noexcept : _id(r._id), _tex(std::move(r._tex)) {
		}
		/// No copy construction.
		framebuffer(const framebuffer&) = delete;
		/// Move assignment.
		framebuffer &operator=(framebuffer &&r) noexcept {
			std::swap(_id, r._id);
			_tex = std::move(r._tex);
			return *this;
		}
		/// No copy assignment.
		framebuffer &operator=(const framebuffer&) = delete;
		/// Automatically calls renderer_base::delete_framebuffer to
		/// dispose of the underlying frame buffer if it's not empty.
		~framebuffer();

		/// Returns the \ref texture that contains the contents of the frame buffer.
		const texture &get_texture() const {
			return _tex;
		}

		/// Checks if the frame buffer is empty.
		bool has_content() const {
			return _tex.has_content();
		}
	protected:
		/// Protected constructor that \ref renderer_base uses to initialize the frame buffer.
		framebuffer(id_t rid, texture &&t) : _id(rid), _tex(std::move(t)) {
		}

		id_t _id = 0; ///< The underlying ID of the frame buffer.
		texture _tex; ///< The underlying texture.
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

		/// Draws a character in the given rectangle with the given color. \ref font::draw_character should be
		/// preffered whenever possible.
		virtual void draw_character_custom(const char_texture&, rectd, colord) = 0;

		/// Draws an array of triangles. Every three elements of the arrays are drawn as one triangle.
		///
		/// \param tex The texture used to draw the triangles.
		/// \param vs The positions of the vertices of the triangles.
		/// \param uvs The texture coordinates of the vertices of the triangles.
		/// \param cs The colors of the vertices of the triangles.
		/// \param n The total number of vertices, i.e., three times the number of triangles.
		virtual void draw_triangles(
			const texture &tex, const vec2d *vs, const vec2d *uvs, const colord *cs, size_t n
		) = 0;
		/// Draws an array of lines. Every two elements of the arrays are drawn as one line.
		///
		/// \param vs The positions of the vertices of the lines.
		/// \param uvs The texture coordinates of the vertices of the lines.
		/// \param n The total number of vertices, i.e., two times the number of lines.
		virtual void draw_lines(const vec2d *vs, const colord *uvs, size_t n) = 0;
		/// Draws a rectangle.
		///
		/// \param tex The texture used to draw the rectangle.
		/// \param r The coordinates of the rectangle.
		/// \param t The texture coordinates.
		/// \param c The color used to draw the rectangle.
		virtual void draw_quad(const texture &tex, rectd r, rectd t, colord c) {
			vec2d vs[6]{
				r.xmin_ymin(), r.xmax_ymin(), r.xmin_ymax(),
				r.xmax_ymin(), r.xmax_ymax(), r.xmin_ymax()
			}, uvs[6]{
				t.xmin_ymin(), t.xmax_ymin(), t.xmin_ymax(),
				t.xmax_ymin(), t.xmax_ymax(), t.xmin_ymax()
			};
			colord cs[6] = {c, c, c, c, c, c};
			draw_triangles(tex, vs, uvs, cs, 6);
		}
		/// Ends the current render target, which can either be a window or a \ref framebuffer.
		virtual void end() = 0;

		/// Creaes a character texture from the given data.
		///
		/// \param w The width of the texture.
		/// \param h The height of the texture.
		/// \param pixels The pixel data, in 8-bit RGBA format.
		virtual char_texture new_character_texture(size_t w, size_t h, const void *pixels) = 0;
		/// Deletes the specified charcter texture. The texture will become empty.
		/// Users normally don't have to manually call this.
		///
		/// \see texture_base::~texture_base()
		virtual void delete_character_texture(char_texture&) = 0;
		/// Creaes a texture from the given data.
		///
		/// \param w The width of the texture.
		/// \param h The height of the texture.
		/// \param pixels The pixel data, in 8-bit RGBA format.
		virtual texture new_texture(size_t w, size_t h, const void *pixels) = 0;
		/// Deletes the specified texture. The texture will become empty.
		/// Users normally don't have to manually call this.
		///
		/// \see texture_base::~texture_base()
		virtual void delete_texture(texture&) = 0;
		/// \overload
		void delete_texture(char_texture &tex) {
			delete_character_texture(tex);
		}

		/// Creates a new \ref framebuffer of the given size.
		///
		/// \param w The width of the frame buffer.
		/// \param h The height of the frame buffer.
		virtual framebuffer new_framebuffer(size_t w, size_t h) = 0;
		/// Deletes the given frame buffer. The frame buffer will become empty.
		/// Users normally don't have to normally call this.
		///
		/// \see framebuffer::~framebuffer()
		virtual void delete_framebuffer(framebuffer&) = 0;
		/// Starts to render to the given frame buffer. The contents of the frame buffer is cleared.
		/// Call end() to finish rendering and make the underlying \ref texture of the frame buffer available.
		virtual void begin_framebuffer(const framebuffer&) = 0;
		/// Starts to render to the given frame buffer without clearning its contents.
		/// Call end() to finish rendering and make the underlying \ref texture of the frame buffer available.
		virtual void continue_framebuffer(const framebuffer&) = 0;

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

		/// Obtains the default renderer.
		inline static renderer_base &get() {
			assert_true_usage(_get_rend().rend != nullptr, "renderer not yet created");
			return *_get_rend().rend;
		}
		/// Creates the default renderer.
		///
		/// \tparam T The type of the renderer. Must be derived from \ref renderer_base.
		template <typename T, typename ...Args> inline static void create_default(Args &&...args) {
			static_assert(std::is_base_of_v<renderer_base, T>, "renderers must be derived from renderer_base");
			_get_rend().assign(std::make_unique<T>(std::forward<Args>(args)...));
		}
	protected:
		/// Called when a new window is created.
		virtual void _new_window(window_base&) = 0;
		/// Called when a window is deleted.
		virtual void _delete_window(window_base&) = 0;

		/// Returns the underlying ID of the \ref texture_base.
		template <bool V> inline static typename texture_base<V>::id_t _get_id(const texture_base<V> &t) {
			return t._id;
		}
		/// Return the underlying ID of the \ref framebuffer.
		inline static framebuffer::id_t _get_id(const framebuffer &f) {
			return f._id;
		}
		/// Creates a \ref texture_base from the given data.
		template <bool Normal = true> texture_base<Normal> _make_texture(typename texture_base<Normal>::id_t id, size_t w, size_t h) {
			return texture_base<Normal>(id, this, w, h);
		}
		/// Creates a \ref framebuffer from the given data.
		framebuffer _make_framebuffer(framebuffer::id_t id, texture &&tex) {
			return framebuffer(id, std::move(tex));
		}
		/// Erases the contents of a \ref texture_base.
		template <bool Normal> inline static void _erase_texture(texture_base<Normal> &t) {
			t._id = 0;
			t._w = t._h = 0;
			t._rend = nullptr;
		}
		/// Erases the contents of a \ref framebuffer.
		inline static void _erase_framebuffer(framebuffer &fb) {
			fb._id = 0;
			_erase_texture(fb._tex);
		}

		/// Holds the curent renderer
		struct _default_renderer {
			/// Sets the given renderer as the default. This should only be called once and only once.
			void assign(std::unique_ptr<renderer_base> &&r) {
				assert_true_usage(!rend, "renderer already created");
				rend = std::move(r);
			}
			std::unique_ptr<renderer_base> rend; ///< Pointer to the current renderer.
		};
		/// Returns the global default renderer.
		static _default_renderer &_get_rend();
	};

	/// Loads an image from the given file name, and returns the corresponding texture created
	/// with the given renderer. This function is implemented in a platform-specific manner.
	texture load_image(renderer_base&, const std::filesystem::path&);
	/// Shorthand for \ref load_image(renderer_base&, const std::filesystem::path&) "load_image"
	/// with the default renderer.
	inline texture load_image(const std::filesystem::path &filename) {
		return load_image(renderer_base::get(), filename);
	}

	template <bool Normal> inline texture_base<Normal>::~texture_base() {
		if (has_content()) {
			_rend->delete_texture(*this);
		}
	}
	inline framebuffer::~framebuffer() {
		if (has_content()) {
			_tex.get_renderer()->delete_framebuffer(*this);
		}
	}
}
