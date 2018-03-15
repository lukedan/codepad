#pragma once

/// \file
/// Implementation of platform-independent functions of the OpenGL renderer.

#include <vector>

// gl.h on windows depends on Windows.h
#ifdef CP_PLATFORM_WINDOWS
#	include <Windows.h>
#	include <windowsx.h>
#endif

#include <GL/gl.h>
#include <GL/glext.h>

#include "renderer.h"

namespace codepad::os {
	/// Base class of OpenGL renderers that implements most platform-independent functionalities.
	class opengl_renderer_base : public renderer_base {
	public:
		/// Checks if the derived class has called _text_atlas::dispose().
		~opengl_renderer_base() override {
#ifdef CP_DETECT_LOGICAL_ERRORS
			assert_true_logical(_atl.disposed, "derived classes must manually dispose _atl");
#endif
		}

		/// Calls \ref _begin_render_target with \ref _get_begin_window_func and
		/// \ref _get_end_window_func to start rendering to the given window.
		void begin(const window_base&) override;
		/// Flushes the text buffer, then calls _render_target_stackframe::push_clip.
		void push_clip(recti r) override {
			_flush_text_buffer();
			_rtfstk.back().push_clip(r);
		}
		/// Flushes the text buffer, then calls _render_target_stackframe::pop_clip.
		void pop_clip() override {
			_flush_text_buffer();
			_rtfstk.back().pop_clip();
		}
		/// Initializes \ref _textbuf if necessary, flushes the text buffer if the given character is
		/// on a different page, then adds the character to the text buffer.
		void draw_character_custom(const char_texture &id, rectd r, colord color) override {
			const _text_atlas::char_data &cd = _atl.get_char_data(_get_id(id));
			if (!_textbuf.valid()) {
				_textbuf.initialize(*this);
			}
			if (_lstpg != cd.page) {
				_flush_text_buffer();
				_lstpg = cd.page;
			}
			_textbuf.append(r, cd.uv, color);
		}
		/// Flushes the text buffer, then calls \p glDrawArrays to drwa the given triangles.
		void draw_triangles(const texture &t, const vec2d *ps, const vec2d *us, const colord *cs, size_t n) override {
			_flush_text_buffer();
			glVertexPointer(2, GL_DOUBLE, sizeof(vec2d), ps);
			glTexCoordPointer(2, GL_DOUBLE, sizeof(vec2d), us);
			glColorPointer(4, GL_DOUBLE, sizeof(colord), cs);
			glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(_get_id(t)));
			glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(n));
		}
		/// Flushes the text buffer, then calls \p glDrawArrays to draw the given lines.
		void draw_lines(const vec2d *ps, const colord *cs, size_t n) override {
			_flush_text_buffer();
			glVertexPointer(2, GL_DOUBLE, sizeof(vec2d), ps);
			glColorPointer(4, GL_DOUBLE, sizeof(colord), cs);
			glBindTexture(GL_TEXTURE_2D, 0);
			glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(n));
		}
		/// Flushes the text buffer, ends the current render target, removes corresponding contents from the
		/// stacks, then continues the last render target if there is one.
		void end() override {
			_flush_text_buffer();
			_rtfstk.back().end();
			assert_true_usage(_rtfstk.back().clip_stack.empty(), "pushclip/popclip mismatch");
			_rtfstk.pop_back();
			glMatrixMode(GL_MODELVIEW);
			glPopMatrix();
			if (!_rtfstk.empty()) {
				_continue_last_render_target();
			}
			_gl_verify();
		}

		/// Calls _text_atlas::new_char to create a new character texture.
		char_texture new_character_texture(size_t w, size_t h, const void *data) override {
			return _atl.new_char(*this, w, h, data);
		}
		/// Calls _text_atlas::delete_char to dispose of the given \ref char_texture.
		void delete_character_texture(char_texture &id) override {
			_atl.delete_char(id);
		}
		/// Creates a texture using the given size and pixel data.
		texture new_texture(size_t w, size_t h, const void *data) override {
			GLuint texid;
			glGenTextures(1, &texid);
			glBindTexture(GL_TEXTURE_2D, texid);
			_set_default_texture_params();
			glTexImage2D(
				GL_TEXTURE_2D, 0, GL_RGBA8,
				static_cast<GLsizei>(w), static_cast<GLsizei>(h),
				0, GL_RGBA, GL_UNSIGNED_BYTE, data
			);
			_gl.GenerateMipmap(GL_TEXTURE_2D);
			return _make_texture(static_cast<texture::id_t>(texid), w, h);
		}
		/// Deletes and erases the given \ref texture.
		void delete_texture(texture &tex) override {
			GLuint t = static_cast<GLuint>(_get_id(tex));
			glDeleteTextures(1, &t);
			_erase_texture(tex);
		}

		/// Creates a \ref framebuffer of the given size.
		framebuffer new_framebuffer(size_t w, size_t h) override {
			GLuint fbid, tid;
			_gl.GenFramebuffers(1, &fbid);
			glGenTextures(1, &tid);
			_gl.BindFramebuffer(GL_FRAMEBUFFER, fbid);
			glBindTexture(GL_TEXTURE_2D, tid);
			_set_default_texture_params();
			glTexImage2D(
				GL_TEXTURE_2D, 0, GL_RGBA8,
				static_cast<GLsizei>(w), static_cast<GLsizei>(h),
				0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr
			);
			_gl.GenerateMipmap(GL_TEXTURE_2D);
			_gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tid, 0);
			auto res = _gl.CheckFramebufferStatus(GL_FRAMEBUFFER);
			if (res != GL_FRAMEBUFFER_COMPLETE) {
				logger::get().log_error(CP_HERE, "glCheckFramebufferStatus returned ", res);
				assert_true_sys(false, "OpenGL error: unable to create framebuffer: ");
			}
			return _make_framebuffer(fbid, _make_texture(static_cast<texture::id_t>(tid), w, h));
		}
		/// Deletes and erases the given \ref framebuffer.
		void delete_framebuffer(framebuffer &fb) override {
			GLuint id = static_cast<GLuint>(_get_id(fb)), tid = static_cast<GLuint>(_get_id(fb.get_texture()));
			_gl.DeleteFramebuffers(1, &id);
			glDeleteTextures(1, &tid);
			_erase_framebuffer(fb);
		}
		/// Calls \ref continue_framebuffer to start rendering to the \ref framebuffer,
		/// then clears its contents.
		void begin_framebuffer(const framebuffer &fb) override {
			continue_framebuffer(fb);
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		/// Calls \ref _begin_render_target to continue rendering to the given \ref framebuffer.
		void continue_framebuffer(const framebuffer &fb) override {
			assert_true_usage(fb.has_content(), "cannot draw to an empty frame buffer");
			_begin_render_target(_render_target_stackframe(
				false, fb.get_texture().get_width(), fb.get_texture().get_height(),
				[this, id = static_cast<GLuint>(_get_id(fb))]() {
				_gl.BindFramebuffer(GL_FRAMEBUFFER, id);
			},
				[this, tex = static_cast<GLuint>(_get_id(fb.get_texture()))]() {
				_gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
				glBindTexture(GL_TEXTURE_2D, tex);
				_gl.GenerateMipmap(GL_TEXTURE_2D);
			}
			));
			_gl_verify();
		}

		/// Flushes the text buffer, then calls \p glPushMatrix and \p glLoadMatrix to push
		/// a matrix onto the stack.
		void push_matrix(const matd3x3 &m) override {
			_flush_text_buffer();
			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();
			double d[16];
			_set_gl_matrix(m, d);
			glLoadMatrixd(d);
		}
		/// Flushes the text buffer, then calls \p glPushMatrix and \p glMultMatrix to multiply
		/// and push a matrix onto the stack.
		void push_matrix_mult(const matd3x3 &m) override {
			_flush_text_buffer();
			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();
			double d[16];
			_set_gl_matrix(m, d);
			glMultMatrixd(d);
		}
		/// Calls \p glGetDoublev to obtain the matrix on the top of the stack.
		matd3x3 top_matrix() const override {
			double d[16];
			glGetDoublev(GL_MODELVIEW_MATRIX, d);
			return _get_gl_matrix(d);
		}
		/// Flushes the text buffer, then calls \p glPopMatrix to pop a matrix from the stack.
		void pop_matrix() override {
			_flush_text_buffer();
			glMatrixMode(GL_MODELVIEW);
			glPopMatrix();
		}

		/// Flushes the text buffer, then calls _render_target_stackframe::push_blend_func to
		/// push a \ref blend_function onto the stack.
		void push_blend_function(blend_function f) override {
			_flush_text_buffer();
			_rtfstk.back().push_blend_func(f);
		}
		/// Flushes the text buffer, then calls _render_target_stackframe::pop_blend_func to
		/// pop a \ref blend_function from the stack.
		void pop_blend_function() override {
			_flush_text_buffer();
			_rtfstk.back().pop_blend_func();
		}
		/// Returns the \ref blend_function on the top of _render_target_stackframe::blend_function_stack.
		blend_function top_blend_function() const override {
			if (_rtfstk.back().blend_func_stack.empty()) {
				return blend_function();
			}
			return _rtfstk.back().blend_func_stack.back();
		}
	protected:
		/// Maps a \ref blend_function to an OpenGL blend function enumeration.
		static GLenum _blend_func_mapping[10];

		/// Stores all OpenGL function pointers needed by the renderer. Derived classes should
		/// obtain all these functions from the system (runtime library).
		struct _gl_funcs {
			PFNGLGENBUFFERSPROC GenBuffers = nullptr; ///< \p glGenBuffers.
			PFNGLDELETEBUFFERSPROC DeleteBuffers = nullptr; ///< \p glDeleteBuffers.
			PFNGLBINDBUFFERPROC BindBuffer = nullptr; ///< \p glBindBuffer.
			PFNGLBUFFERDATAPROC BufferData = nullptr; ///< \p glBufferData.
			PFNGLMAPBUFFERPROC MapBuffer = nullptr; ///< \p glMapBuffer.
			PFNGLUNMAPBUFFERPROC UnmapBuffer = nullptr; ///< \p glUnmapBuffer.
			PFNGLGENFRAMEBUFFERSPROC GenFramebuffers = nullptr; ///< \p glGenFramebuffers.
			PFNGLBINDFRAMEBUFFERPROC BindFramebuffer = nullptr; ///< \p glBindFramebuffer.
			PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D = nullptr; ///< \p glFramebufferTexture2D.
			PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus = nullptr; ///< \p glCheckFramebufferStatus.
			PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers = nullptr; ///< \p glDeleteFramebuffers.
			PFNGLGENERATEMIPMAPPROC GenerateMipmap = nullptr; ///< \p glGenerateMipmap.
		};

		/// Called in \ref _new_window to initializes a newly created window.
		virtual void _init_new_window(window_base&) = 0;
		/// Called to obtain a function that is called when the renderer starts or continues
		/// to render to the window. Thus, the returned function should not clear the contents
		/// of the window.
		virtual std::function<void()> _get_begin_window_func(const window_base&) = 0;
		/// Called to obtain a function that is called when the renderer has finished drawing
		/// to the window, to present the rendered result on the window.
		virtual std::function<void()> _get_end_window_func(const window_base&) = 0;

		/// Calls \ref _init_new_window to initialize the window, and enable the features needed
		/// by the renderer.
		void _new_window(window_base &wnd) override {
			_init_new_window(wnd);
			glEnable(GL_BLEND);
			glEnable(GL_TEXTURE_2D);
			glEnableClientState(GL_VERTEX_ARRAY);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glEnableClientState(GL_COLOR_ARRAY);
		}
		/// Does nothing.
		void _delete_window(window_base&) override {
		}

		/// Sets the default parameters for the currently bound texture.
		inline static void _set_default_texture_params() {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		/// Copies data from a \ref matd3x3 to an array of doubles that is to be passed to OpenGL.
		inline static void _set_gl_matrix(const matd3x3 &m, double(&res)[16]) {
			res[0] = m[0][0];
			res[1] = m[1][0];
			res[3] = m[2][0];
			res[4] = m[0][1];
			res[5] = m[1][1];
			res[7] = m[2][1];
			res[12] = m[0][2];
			res[13] = m[1][2];
			res[15] = m[2][2];
			res[2] = res[6] = res[8] = res[9] = res[11] = res[14] = 0.0;
			res[10] = 1.0;
		}
		/// Copies data from an array of doubles obtained from OpenGL to a \ref matd3x3.
		inline static matd3x3 _get_gl_matrix(const double(&res)[16]) {
			matd3x3 result;
			result[0][0] = res[0];
			result[1][0] = res[1];
			result[2][0] = res[3];
			result[0][1] = res[4];
			result[1][1] = res[5];
			result[2][1] = res[7];
			result[0][2] = res[12];
			result[1][2] = res[13];
			result[2][2] = res[15];
			return result;
		}

		/// Stores an OpenGL data buffer.
		///
		/// \tparam Target The desired usage of the underlying data.
		template <GLenum Target> struct _gl_data_buffer {
		public:
			/// Initializes the buffer to empty.
			_gl_data_buffer() = default;
			/// Move constructor.
			_gl_data_buffer(_gl_data_buffer &&r) : _rend(r._rend), _id(r._id) {
				r._rend = nullptr;
				r._id = 0;
			}
			/// No copy construction.
			_gl_data_buffer(const _gl_data_buffer&) = delete;
			/// Move assignment.
			_gl_data_buffer &operator=(_gl_data_buffer &&r) {
				std::swap(_rend, r._rend);
				std::swap(_id, r._id);
				return *this;
			}
			/// No copy assignment.
			_gl_data_buffer &operator=(const _gl_data_buffer&) = delete;
			/// Calls dispose() to dispose the buffer.
			~_gl_data_buffer() {
				dispose();
			}

			/// Initializes the buffer to a new one, disposing the previous one if necessary.
			///
			/// \param rend The renderer used to initialize the buffer.
			void initialize(opengl_renderer_base &rend) {
				dispose();
				_rend = &rend;
				_rend->_gl.GenBuffers(1, &_id);
			}
			/// If the buffer is valid, unbinds the buffer, calls \p glDeleteBuffers to delete the buffer,
			/// and resets the buffer to empty.
			void dispose() {
				if (valid()) {
					unbind();
					_rend->_gl.DeleteBuffers(1, &_id);
					_rend = nullptr;
					_id = 0;
				}
			}

			/// Calls \p glBindBuffer to bind this buffer.
			void bind() {
				_rend->_gl.BindBuffer(Target, _id);
			}
			/// Calls \p glBindBuffer to unbind any previously bound buffer.
			void unbind() {
				_rend->_gl.BindBuffer(Target, 0);
			}

			/// Resizes the buffer with the given size and \p GL_DYNAMIC_DRAW usage by calling
			/// \p glBufferData. This operation will erase any data it previously contained.
			void resize_dynamic_draw(size_t size) {
				bind();
				_rend->_gl.BufferData(Target, size, nullptr, GL_DYNAMIC_DRAW);
			}
			/// Calls \p glMapBuffer to map the buffer with \p GL_READ_WRITE access, then returns
			/// the resulting pointer.
			void *map() {
				bind();
				return _rend->_gl.MapBuffer(Target, GL_READ_WRITE);
			}
			/// Calls \p glUnmapBuffer to unmap this buffer. This is required before using the
			/// buffer data for rendering.
			void unmap() {
				bind();
				_rend->_gl.UnmapBuffer(Target);
			}

			/// Returns the \ref opengl_renderer_base used to initialize this buffer.
			opengl_renderer_base *get_context() const {
				return _rend;
			}
			/// Returns whether this struct contains a valid OpenGL buffer, i.e., is not empty.
			bool valid() const {
				return _rend != nullptr && _id != 0;
			}
		protected:
			opengl_renderer_base * _rend = nullptr; ///< Pointer to the renderer that created this buffer.
			GLuint _id = 0; ///< The ID of the buffer.
		};
		/// OpenGL data buffer that automatically grows in size when necessary.
		///
		/// \tparam T The type of the elements. Must be POD.
		/// \tparam Target The desired usage of this buffer.
		template <typename T, GLenum Target> struct _automatic_gl_data_buffer {
		public:
			constexpr static size_t minimum_size = 20; ///< The initial capacity of the buffer.

			/// Calls _gl_data_buffer::initialize to initialize the buffer, allocates the minimum capacity,
			/// then maps the buffer.
			void initialize(opengl_renderer_base &rend) {
				_buf.initialize(rend);
				_cap = minimum_size;
				_buf.resize_dynamic_draw(sizeof(T) * minimum_size);
				_ptr = static_cast<T*>(_buf.map());
				_buf.unbind(); // keep glDrawArrays available
			}
			/// Returns whether \ref _buf is valid.
			///
			/// \sa _gl_data_buffer::valid()
			bool valid() const {
				return _buf.valid();
			}
			/// Disposes the underlying \ref _gl_data_buffer.
			void dispose() {
				_buf.dispose();
			}

			/// Pushes an element onto the back of the buffer, enlarging its capacity if necessary.
			void push_back(const T &obj) {
				if (_cnt == _cap) {
					_set_capacity(_cap * 2);
				}
				_ptr[_cnt++] = obj;
			}
			/// Reserves enough space for the given number of elements.
			void reserve(size_t cnt) {
				if (cnt > _cap) {
					_set_capacity(cnt);
				}
			}

			/// Unmaps the buffer, then binds it. The data cannot be accessed until \ref unbind()
			/// is called. This is required to make the data available for rendering.
			void bind() {
				_buf.unmap();
				_buf.bind();
			}
			/// Maps then unbinds the buffer.
			///
			/// \remark \p glDrawArrays() will fail if a buffer is bound, so this should be called
			///         as soon as the buffer data is not needed.
			void unbind() {
				_ptr = static_cast<T*>(_buf.map());
				_buf.unbind();
			}

			/// Returns a pointer to the data of the buffer. The return value is not valid if
			/// \ref bind() has been called but \ref unbind() hasn't.
			T *data() {
				return _ptr;
			}
			/// Const version of T *data().
			const T *data() const {
				return _ptr;
			}

			/// Returns the underlying \ref _gl_data_buffer.
			const _gl_data_buffer &get_raw() const {
				return _buf;
			}

			/// Returns the number of elements in the buffer.
			size_t size() const {
				return _cnt;
			}
			/// Returns the capacity (i.e., how many elements it can contain) of the buffer.
			size_t capacity() const {
				return _cap;
			}
			/// Clears the contents of the buffer.
			void clear() {
				_cnt = 0;
			}
		protected:
			_gl_data_buffer<Target> _buf; ///< The underlying buffer.
			T *_ptr = nullptr; ///< The mapped pointer.
			size_t
				_cnt = 0, ///< The number of elements in the buffer.
				_cap = 0; ///< The capacity of the buffer.

			/// Sets the capacity of the buffer, by allocating a new one with the desired size
			/// and replacing the original one with it.
			void _set_capacity(size_t cp) {
				_cap = cp;
				_gl_data_buffer<Target> newbuf;
				newbuf.initialize(*_buf.get_context());
				newbuf.resize_dynamic_draw(sizeof(T) * _cap);
				T *nptr = static_cast<T*>(newbuf.map());
				std::memcpy(nptr, _ptr, sizeof(T) * _cnt);
				newbuf.unbind(); // keep glDrawArrays available
				_ptr = nptr;
				_buf = std::move(newbuf);
			}
		};

		/// Stores character images in large `pages' of textures. This is mainly intended to reduce
		/// the number of draw calls to draw large numbers of characters.
		struct _text_atlas {
		public:
			/// Inserts a new character image into the last page. If the page doesn't have enough space,
			/// a new page will be created.
			char_texture new_char(opengl_renderer_base &r, size_t w, size_t h, const void *data) {
				if (_pages.empty()) {
					_new_page();
				}
				texture::id_t id = _alloc_id();
				char_data &cd = _cd_slots[id];
				if (w == 0 || h == 0) { // the character is blank
					cd.uv = rectd(0.0, 0.0, 0.0, 0.0);
					cd.page = _pages.size() - 1;
				} else {
					std::reference_wrapper<page> curp = _pages.back();
					if (_cx + w + 2 * border > curp.get().width) {
						// the current row doesn't have enough space; move to next row
						_cx = 0;
						_cy += _my;
						_my = 0;
					}
					size_t t, l; // coords of the the new character's top left corner
					if (_cy + h + 2 * border > curp.get().height) {
						// the current page doesn't have enough space; create new page
						if (_lpdirty) {
							curp.get().flush(r._gl);
						}
						curp.get().freeze();
						curp = _new_page();
						_cy = 0;
						l = t = border;
						_my = h + 2 * border;
					} else {
						l = _cx + border;
						t = _cy + border;
						_my = std::max(_my, h + 2 * border);
					}
					_cx = l + w;
					// copy image data
					const unsigned char *src = static_cast<const unsigned char*>(data);
					for (size_t y = 0; y < h; ++y) {
						unsigned char *cur = curp.get().data + ((y + t) * curp.get().width + l) * 4;
						for (size_t x = 0; x < w; ++x, src += 4, cur += 4) {
							cur[0] = src[0];
							cur[1] = src[1];
							cur[2] = src[2];
							cur[3] = src[3];
						}
					}
					// calculate UV coordinates
					cd.uv = rectd(
						static_cast<double>(l) / static_cast<double>(curp.get().width),
						static_cast<double>(l + w) / static_cast<double>(curp.get().width),
						static_cast<double>(t) / static_cast<double>(curp.get().height),
						static_cast<double>(t + h) / static_cast<double>(curp.get().height)
					);
					cd.page = _pages.size() - 1;
					_lpdirty = true; // mark the last page as dirty
				}
				return r._make_texture<false>(id, w, h);
			}
			/// Adds the ID of the deleted texture to \ref _cd_alloc, and erases the texture.
			void delete_char(char_texture &id) {
				_cd_alloc.push_back(_get_id(id));
				_erase_texture(id);
			}
			/// Frees all resources allocated by \ref _text_atlas.
			void dispose() {
#ifdef CP_DETECT_LOGICAL_ERRORS
				assert_true_logical(!disposed, "text atlas already disposed");
				disposed = true;
#endif
				_pages.clear();
			}

			/// Stores a page.
			struct page {
				/// Initializes the page to empty.
				page() = default;
				/// Allocates a texture and pixel data of the given size.
				page(size_t w, size_t h) : width(w), height(h) {
					size_t sz = width * height * 4;
					data = static_cast<unsigned char*>(std::malloc(sz));
					std::memset(data, 0, sz);
					glGenTextures(1, &tex_id);
					glBindTexture(GL_TEXTURE_2D, tex_id);
					_set_default_texture_params();
				}
				/// Move constructor.
				page(page &&src) : width(src.width), height(src.height), data(src.data), tex_id(src.tex_id) {
					src.width = src.height = 0;
					src.data = nullptr;
					src.tex_id = 0;
				}
				/// No copy construction.
				page(const page&) = delete;
				/// Move assignment.
				page &operator=(page &&src) {
					std::swap(width, src.width);
					std::swap(height, src.height);
					std::swap(data, src.data);
					std::swap(tex_id, src.tex_id);
					return *this;
				}
				/// No copy assignment.
				page &operator=(const page&) = delete;
				/// Frees the allocated resources.
				~page() {
					if (tex_id != 0) { // valid
						glDeleteTextures(1, &tex_id);
						if (data != nullptr) { // not frozen
							std::free(data);
						}
					}
				}

				/// Copies the pixel data from \ref data to OpenGL.
				void flush(const _gl_funcs &gl) {
					glBindTexture(GL_TEXTURE_2D, tex_id);
					glTexImage2D(
						GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
						0, GL_RGBA, GL_UNSIGNED_BYTE, data
					);
					gl.GenerateMipmap(GL_TEXTURE_2D);
				}
				/// Frees \ref data to reduce memory usage when the page is full (and thus won't change anymore).
				void freeze() {
					std::free(data);
					data = nullptr;
				}

				/// Returns whether the page contains a valid OpenGL texture.
				bool valid() const {
					return tex_id != 0;
				}

				size_t
					width = 0, ///< The width of the page.
					height = 0; ///< The height of the page.
				unsigned char *data = nullptr; ///< The pixel data.
				GLuint tex_id = 0; ///< The OpenGL texture ID.
			};
			/// Stores the information about a character in the atlas.
			struct char_data {
				rectd uv; ///< The UV coordinates of the character in the page.
				size_t page; ///< The number of the page that the character is in.
			};

			/// Returns the \ref char_data corresponding to the given ID.
			const char_data &get_char_data(size_t id) const {
				return _cd_slots[id];
			}
			/// Retrieves a \ref page for rendering. Calls page::flush to flush it if necessary.
			const page &get_page(const _gl_funcs &gl, size_t page) {
				if (_lpdirty && page + 1 == _pages.size()) {
					_pages.back().flush(gl);
					_lpdirty = false;
				}
				return _pages[page];
			}

			size_t
				/// The width of a page. Modifying this only affects pages created afterwards.
				page_width = 600,
				/// The height of a page. Modifying this only affects pages created afterwards.
				page_height = 300,
				/// The margin between characters. Modifying this only affects characters added afterwards.
				border = 1;
#ifdef CP_DETECT_LOGICAL_ERRORS
			bool disposed = false; ///< Records whether \ref dispose() has been called.
#endif
		protected:
			size_t
				_cx = 0, ///< The X coordinate of the next character, including its border.
				_cy = 0, ///< The Y coordinate of the next character, including its border.
				_my = 0; ///< The height of the tallest character of this row, including both of its borders.
			std::vector<page> _pages; ///< The pages.
			std::vector<char_data> _cd_slots; ///< Stores all \ref char_data "char_datas".
			std::vector<char_texture::id_t> _cd_alloc; ///< Stores all freed character IDs.
			bool _lpdirty = false; ///< Marks whether the last page is dirty.

			/// Creates a new page and initializes all its pixels to transparent white.
			///
			/// \return An reference to the new page.
			page &_new_page() {
				page np(page_width, page_height);
				for (size_t y = 0; y < np.height; ++y) {
					unsigned char *cur = np.data + y * np.width * 4;
					for (size_t x = 0; x < np.width; ++x) {
						*(cur++) = 255;
						*(cur++) = 255;
						*(cur++) = 255;
						*(cur++) = 0;
					}
				}
				_pages.push_back(std::move(np));
				return _pages.back();
			}
			/// Allocates an ID for a character texture.
			char_texture::id_t _alloc_id() {
				texture::id_t res;
				if (!_cd_alloc.empty()) { // use deleted id
					res = _cd_alloc.back();
					_cd_alloc.pop_back();
				} else { // allocate new id
					res = _cd_slots.size();
					_cd_slots.emplace_back(); // add new slot
				}
				return res;
			}
		};
		/// Buffers the text to render, and draws them all at once when necessary.
		struct _text_buffer {
			/// Stores the position, UV, and color of a vertex.
			struct vertex {
				/// Default constructor.
				vertex() = default;
				/// Initializes the vertex with the given position, UV, and color.
				vertex(vec2d p, vec2d u, colord color) : pos(p), uv(u), c(color) {
				}

				vec2d
					pos, ///< The position.
					uv; ///< The UV coordinates.
				colord c; ///< The color.
			};
			/// The buffer that stores vertex data.
			_automatic_gl_data_buffer<vertex, GL_ARRAY_BUFFER> vertex_buffer;
			/// The buffer that stores vertex indices. Its contents is reused between batches.
			_automatic_gl_data_buffer<unsigned int, GL_ELEMENT_ARRAY_BUFFER> id_buffer;
			size_t
				/// The number of vertices indexed by \ref id_buffer. This can be larger than the number
				/// of elements in \ref vertex_buffer.
				indexed_vertex_count = 0,
				/// The number of indices in \ref id_buffer that refer to valid indices in \ref vertex_buffer,
				/// i.e., 6 times the number of buffered characters.
				used_index_count = 0;

			/// Initializes \ref id_buffer and \ref vertex_buffer with the given renderer.
			void initialize(opengl_renderer_base &rend) {
				vertex_buffer.initialize(rend);
				id_buffer.initialize(rend);
			}
			/// Returns whether the buffers are valid.
			///
			/// \sa _automatic_gl_data_buffer::valid()
			bool valid() const {
				return vertex_buffer.valid();
			}
			/// Disposes \ref id_buffer and \ref vertex_buffer.
			void dispose() {
				vertex_buffer.dispose();
				id_buffer.dispose();
			}

			/// Appends a character to the buffer.
			void append(rectd layout, rectd uv, colord c) {
				used_index_count += 6;
				if (indexed_vertex_count == vertex_buffer.size()) {
					// add more indices
					unsigned int id = static_cast<unsigned int>(vertex_buffer.size());
					id_buffer.push_back(id);
					id_buffer.push_back(id + 1);
					id_buffer.push_back(id + 2);
					id_buffer.push_back(id + 1);
					id_buffer.push_back(id + 3);
					id_buffer.push_back(id + 2);
					indexed_vertex_count += 4;
				}
				// add vertices
				vertex_buffer.push_back(vertex(layout.xmin_ymin(), uv.xmin_ymin(), c));
				vertex_buffer.push_back(vertex(layout.xmax_ymin(), uv.xmax_ymin(), c));
				vertex_buffer.push_back(vertex(layout.xmin_ymax(), uv.xmin_ymax(), c));
				vertex_buffer.push_back(vertex(layout.xmax_ymax(), uv.xmax_ymax(), c));
			}
			/// Draws all buffered characters with the given texture.
			void flush(GLuint tex) {
				if (used_index_count > 0) {
					vertex_buffer.bind();
					id_buffer.bind();
					glVertexPointer(2, GL_DOUBLE, sizeof(vertex), reinterpret_cast<const GLvoid*>(offsetof(vertex, pos)));
					glTexCoordPointer(2, GL_DOUBLE, sizeof(vertex), reinterpret_cast<const GLvoid*>(offsetof(vertex, uv)));
					glColorPointer(4, GL_DOUBLE, sizeof(vertex), reinterpret_cast<const GLvoid*>(offsetof(vertex, c)));
					glBindTexture(GL_TEXTURE_2D, tex);
					glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(used_index_count), GL_UNSIGNED_INT, nullptr);
					vertex_buffer.clear();
					used_index_count = 0;
					vertex_buffer.unbind();
					id_buffer.unbind();
				}
			}
		};

		/// Stores the information about a render target that's being rendered to.
		struct _render_target_stackframe {
			template <typename T, typename U> _render_target_stackframe(
				bool invy, size_t w, size_t h, T &&fbeg, U &&fend
			) : invert_y(invy), width(w), height(h), begin(std::forward<T>(fbeg)), end(std::forward<U>(fend)) {
			}

			bool invert_y = false; ///< Indicates whether the rendering should be performed upside-down.
			size_t
				width = 0, ///< The width of the canvas.
				height = 0; ///< The height of the canvas.
			std::function<void()>
				begin, ///< The function to be called to continue rendering to the target.
				end; ///< The function to be called when the rendering has finished.
			std::vector<recti> clip_stack; ///< The stack of clip regions for this rendering target.
			/// The stack of \ref blend_function "blend_functions" for this rendering target.
			std::vector<blend_function> blend_func_stack;

			/// Pushes a clip onto \ref clip_stack.
			void push_clip(recti r) {
				if (invert_y) { // invert the clip region
					int ymin = r.ymin, ch = static_cast<int>(height);
					r.ymin = ch - r.ymax;
					r.ymax = ch - ymin;
				}
				if (!clip_stack.empty()) { // calculate the union of all clip regions
					r = recti::common_part(r, clip_stack.back());
				}
				r.make_valid_max();
				clip_stack.push_back(r);
				apply_clip();
			}
			/// Pops a clip from \ref clip_stack.
			void pop_clip() {
				clip_stack.pop_back();
				apply_clip();
			}
			/// Applies the current clip region.
			void apply_clip() {
				if (clip_stack.empty()) {
					glDisable(GL_SCISSOR_TEST);
				} else {
					glEnable(GL_SCISSOR_TEST);
					const recti &r = clip_stack.back();
					glScissor(r.xmin, r.ymin, r.width(), r.height());
				}
			}

			/// Pushes a \ref blend_function onto \ref blend_func_stack.
			void push_blend_func(blend_function bf) {
				blend_func_stack.push_back(bf);
				apply_blend_func();
			}
			/// Pops a \ref blend_function from \ref blend_func_stack.
			void pop_blend_func() {
				blend_func_stack.pop_back();
				apply_blend_func();
			}
			/// Applies the current \ref blend_function.
			void apply_blend_func() {
				if (blend_func_stack.empty()) {
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				} else {
					glBlendFunc(
						_blend_func_mapping[static_cast<int>(blend_func_stack.back().source_factor)],
						_blend_func_mapping[static_cast<int>(blend_func_stack.back().destination_factor)]
					);
				}
			}

			/// Calls \ref apply_clip and \ref apply_blend_func to apply the current rendering configuration.
			void apply_config() {
				apply_clip();
				apply_blend_func();
			}
		};

		/// Disposes all OpenGL related resources, namely \ref _atl and \ref _textbuf.
		void _dispose_gl_rsrc() {
			_atl.dispose();
			_textbuf.dispose();
		}

		/// Flushes the text buffer if necessary, then starts to render to the given target.
		void _begin_render_target(_render_target_stackframe rtf) {
			if (!_rtfstk.empty()) {
				_flush_text_buffer();
			}
			_rtfstk.push_back(std::move(rtf));
			_continue_last_render_target();
			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();
			glLoadIdentity();
		}
		/// Continues to render to the target at the top of \ref _rtfstk.
		void _continue_last_render_target() {
			_render_target_stackframe &rtf = _rtfstk.back();
			rtf.begin();
			rtf.apply_config();
			glViewport(0, 0, static_cast<GLsizei>(rtf.width), static_cast<GLsizei>(rtf.height));
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			if (rtf.invert_y) {
				glOrtho(0.0, static_cast<GLdouble>(rtf.width), static_cast<GLdouble>(rtf.height), 0.0, 0.0, -1.0);
			} else {
				glOrtho(0.0, static_cast<GLdouble>(rtf.width), 0.0, static_cast<GLdouble>(rtf.height), 0.0, -1.0);
			}
			_gl_verify();
		}

		/// Flushes the text buffer by calling _text_buffer::flush.
		void _flush_text_buffer() {
			_textbuf.flush(_atl.get_page(_gl, _lstpg).tex_id);
		}
		/// Checks for any OpenGL errors.
		void _gl_verify() {
#ifdef CP_DETECT_SYSTEM_ERRORS
			GLenum errorcode = glGetError();
			if (errorcode != GL_NO_ERROR) {
				logger::get().log_error(CP_HERE, "OpenGL error code ", errorcode);
				assert_true_sys(false, "OpenGL error");
			}
#endif
		}

		std::vector<_render_target_stackframe> _rtfstk; ///< The stack of render targets.
		_gl_funcs _gl; ///< Additional OpenGL routines needed by the renderer.
		_text_atlas _atl; ///< The text atlas.
		_text_buffer _textbuf; ///< The text buffer.
		size_t _lstpg = 0; ///< The page that all characters in \ref _textbuf is on.
	};
}
