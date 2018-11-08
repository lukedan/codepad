// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

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
#ifdef CP_CHECK_LOGICAL_ERRORS
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
			if (_lstpg != cd.page) {
				_flush_text_buffer();
				_lstpg = cd.page;
			}
			_textbuf.append(*this, r, cd.uv, color);
		}
		/// Flushes the text buffer, then calls \p glDrawArrays to drwa the given triangles.
		void draw_triangles(const texture &t, const vec2d *ps, const vec2d *us, const colord *cs, size_t n) override {
			if (n > 0) {
				_flush_text_buffer();

				_gl_buffer<GL_ARRAY_BUFFER> buf;
				buf.initialize(*this);
				buf.clear_resize_dynamic_draw(*this, sizeof(_vertex) * n);
				auto ptr = static_cast<_vertex*>(buf.map(*this));
				for (size_t i = 0; i < n; ++i) {
					ptr[i] = _vertex(ps[i], us[i], cs[i]);
				}
				buf.unmap(*this);

				_defaultprog.acivate(*this);
				_gl.VertexAttribPointer(
					0, 2, GL_FLOAT, false, sizeof(_vertex), reinterpret_cast<const GLvoid*>(offsetof(_vertex, pos))
				);
				_gl.EnableVertexAttribArray(0);
				_gl.VertexAttribPointer(
					1, 2, GL_FLOAT, false, sizeof(_vertex), reinterpret_cast<const GLvoid*>(offsetof(_vertex, uv))
				);
				_gl.EnableVertexAttribArray(1);
				_gl.VertexAttribPointer(
					2, 4, GL_FLOAT, false, sizeof(_vertex), reinterpret_cast<const GLvoid*>(offsetof(_vertex, c))
				);
				_gl.EnableVertexAttribArray(2);
				_gl.ActiveTexture(GL_TEXTURE0);
				auto tex = static_cast<GLuint>(_get_id(t));
				if (tex == 0) { // _blanktex for no texture
					tex = _blanktex;
				}
				glBindTexture(GL_TEXTURE_2D, tex);
				glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(n));

				buf.dispose(*this);
			}
		}
		/// Flushes the text buffer, then calls \p glDrawArrays to draw the given lines.
		///
		/// \todo Implement draw_lines.
		void draw_lines(const vec2d*, const colord*, size_t n) override {
			if (n > 0) {
				_flush_text_buffer();
			}
		}
		/// Flushes the text buffer, ends the current render target, removes corresponding contents from the
		/// stacks, then continues the last render target if there is one.
		void end() override {
			_flush_text_buffer();
			_rtfstk.back().end();
			assert_true_usage(_rtfstk.back().clip_stack.empty(), "pushclip/popclip mismatch");
			_rtfstk.pop_back();
			_matstk.pop_back(); // pop default identity matrix
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
			auto t = static_cast<GLuint>(_get_id(tex));
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
			auto id = static_cast<GLuint>(_get_id(fb)), tid = static_cast<GLuint>(_get_id(fb.get_texture()));
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
			_matstk.push_back(m);
			_apply_matrix();
		}
		/// Flushes the text buffer, then calls \p glPushMatrix and \p glMultMatrix to multiply
		/// and push a matrix onto the stack.
		void push_matrix_mult(const matd3x3 &m) override {
			push_matrix(_matstk.back() * m);
		}
		/// Calls \p glGetDoublev to obtain the matrix on the top of the stack.
		matd3x3 top_matrix() const override {
			return _matstk.back();
		}
		/// Flushes the text buffer, then calls \p glPopMatrix to pop a matrix from the stack.
		void pop_matrix() override {
			_flush_text_buffer();
			_matstk.pop_back();
			_apply_matrix();
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
			PFNGLBINDBUFFERPROC BindBuffer = nullptr; ///< \p glBindBuffer.
			PFNGLBUFFERDATAPROC BufferData = nullptr; ///< \p glBufferData.
			PFNGLGETBUFFERPARAMETERIVPROC GetBufferParameteriv = nullptr; ///< \p glGetBufferParameteriv.
			PFNGLMAPBUFFERPROC MapBuffer = nullptr; ///< \p glMapBuffer.
			PFNGLUNMAPBUFFERPROC UnmapBuffer = nullptr; ///< \p glUnmapBuffer.
			PFNGLDELETEBUFFERSPROC DeleteBuffers = nullptr; ///< \p glDeleteBuffers.

			PFNGLGENVERTEXARRAYSPROC GenVertexArrays = nullptr; ///< \p glGenVertexArrays.
			PFNGLBINDVERTEXARRAYPROC BindVertexArray = nullptr; ///< \p glBindVertexArray.
			PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer = nullptr; ///< \p glVertexAttribPointer.
			PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray = nullptr; ///< \p glEnableVertexAttribArray.
			PFNGLDELETEVERTEXARRAYSPROC DeleteVertexArrays = nullptr; ///< \p glDeleteVertexArrays.

			PFNGLGENFRAMEBUFFERSPROC GenFramebuffers = nullptr; ///< \p glGenFramebuffers.
			PFNGLBINDFRAMEBUFFERPROC BindFramebuffer = nullptr; ///< \p glBindFramebuffer.
			PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D = nullptr; ///< \p glFramebufferTexture2D.
			PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus = nullptr; ///< \p glCheckFramebufferStatus.
			PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers = nullptr; ///< \p glDeleteFramebuffers.

			PFNGLACTIVETEXTUREPROC ActiveTexture = nullptr; ///< \p glActiveTexture.
			PFNGLGENERATEMIPMAPPROC GenerateMipmap = nullptr; ///< \p glGenerateMipmap.

			PFNGLCREATESHADERPROC CreateShader = nullptr; /// \p glCreateShader.
			PFNGLSHADERSOURCEPROC ShaderSource = nullptr; /// \p glShaderSource.
			PFNGLCOMPILESHADERPROC CompileShader = nullptr; /// \p glCompileShader.
			PFNGLGETSHADERIVPROC GetShaderiv = nullptr; /// \p glGetShaderiv.
			PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog = nullptr; /// \p glGetShaderInfoLog.
			PFNGLDELETESHADERPROC DeleteShader = nullptr; /// \p glDeleteShader.

			PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation = nullptr; /// \p glGetUniformLocation.
			PFNGLUNIFORM1IPROC Uniform1i = nullptr; /// \p glUniform1i.
			PFNGLUNIFORM2FVPROC Uniform2fv = nullptr; /// \p glUniform2fv.
			PFNGLUNIFORMMATRIX3FVPROC UniformMatrix3fv = nullptr; /// \p glUniformMatrix3fv.

			PFNGLCREATEPROGRAMPROC CreateProgram = nullptr; /// \p glCreateProgram.
			PFNGLATTACHSHADERPROC AttachShader = nullptr; /// \p glAttachShader.
			PFNGLLINKPROGRAMPROC LinkProgram = nullptr; /// \p glLinkProgram.
			PFNGLGETPROGRAMIVPROC GetProgramiv = nullptr; /// \p glGetProgramiv.
			PFNGLUSEPROGRAMPROC UseProgram = nullptr; /// \p glUseProgram.
			PFNGLDELETEPROGRAMPROC DeleteProgram = nullptr; /// \p glDeleteProgram.
		};

		/// Called to obtain a function that is called when the renderer starts or continues
		/// to render to the window. Thus, the returned function should not clear the contents
		/// of the window.
		virtual std::function<void()> _get_begin_window_func(const window_base&) = 0;
		/// Called to obtain a function that is called when the renderer has finished drawing
		/// to the window, to present the rendered result on the window.
		virtual std::function<void()> _get_end_window_func(const window_base&) = 0;

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

		/// Holds an OpenGL program.
		struct _gl_program {
		public:
			/// Initializes the program with the given renderer and code.
			///
			/// \todo Log shader link information.
			void initialize(opengl_renderer_base &rend, const GLchar *vertex_code, const GLchar *frag_code) {
				GLuint
					vert = create_shader<GL_VERTEX_SHADER>(rend, vertex_code),
					frag = create_shader<GL_FRAGMENT_SHADER>(rend, frag_code);
				_id = rend._gl.CreateProgram();
				rend._gl.AttachShader(_id, vert);
				rend._gl.AttachShader(_id, frag);
				rend._gl.LinkProgram(_id);
				GLint status;
				rend._gl.GetProgramiv(_id, GL_LINK_STATUS, &status);
				if (status == GL_FALSE) {
					assert_true_sys(false, "failed to link OpenGL program");
				}
				rend._gl.DeleteShader(vert);
				rend._gl.DeleteShader(frag);
			}
			/// Deletes the OpenGL program.
			void dispose(opengl_renderer_base &rend) {
				rend._gl.DeleteProgram(_id);
			}

			/// Returns the ID of the program.
			GLuint get_id() const {
				return _id;
			}

			void acivate(opengl_renderer_base &rend) {
				rend._gl.UseProgram(_id);
			}

			/// Sets a uniform variable of type \p GLint.
			void set_int(opengl_renderer_base &rend, const GLchar *name, GLint value) {
				rend._gl.Uniform1i(rend._gl.GetUniformLocation(_id, name), value);
			}
			/// Sets a uniform variable of type \ref vec2d.
			void set_vec2(opengl_renderer_base &rend, const GLchar *name, vec2d value) {
				GLfloat v[2]{static_cast<GLfloat>(value.x), static_cast<GLfloat>(value.y)};
				rend._gl.Uniform2fv(rend._gl.GetUniformLocation(_id, name), 1, v);
			}
			/// Sets a uniform variable of type \ref matd3x3.
			void set_mat3(opengl_renderer_base &rend, const GLchar *name, const matd3x3 &mat) {
				GLfloat matdata[9];
				_set_gl_matrix(mat, matdata);
				rend._gl.UniformMatrix3fv(rend._gl.GetUniformLocation(_id, name), 1, false, matdata);
			}

			/// Creates a shader of the given type with the specified renderer and code.
			///
			/// \tparam Type The type of the shader.
			template <GLenum Type> inline static GLuint create_shader(
				opengl_renderer_base &rend, const GLchar *code
			) {
				constexpr size_t _log_length = 500;
				static GLchar _msg[_log_length];

				GLuint shader = rend._gl.CreateShader(Type);
				rend._gl.ShaderSource(shader, 1, &code, nullptr);
				rend._gl.CompileShader(shader);
				GLint result;
				rend._gl.GetShaderiv(shader, GL_COMPILE_STATUS, &result);
				if (result == GL_FALSE) {
					rend._gl.GetShaderInfoLog(shader, _log_length, nullptr, _msg);
					logger::get().log_error(CP_HERE, "shader compilation info: ", _msg);
					assert_true_sys(false, "failed to compile shader");
				}
				return shader;
			}
		protected:
			GLuint _id = 0; ///< The OpenGL program ID.

			/// Copies data from a \ref matd3x3 to an array of floats that is to be passed to OpenGL.
			inline static void _set_gl_matrix(const matd3x3 &m, GLfloat(&res)[9]) {
				res[0] = static_cast<GLfloat>(m[0][0]);
				res[1] = static_cast<GLfloat>(m[1][0]);
				res[2] = static_cast<GLfloat>(m[2][0]);
				res[3] = static_cast<GLfloat>(m[0][1]);
				res[4] = static_cast<GLfloat>(m[1][1]);
				res[5] = static_cast<GLfloat>(m[2][1]);
				res[6] = static_cast<GLfloat>(m[0][2]);
				res[7] = static_cast<GLfloat>(m[1][2]);
				res[8] = static_cast<GLfloat>(m[2][2]);
			}
		};

		/// A vertex.
		struct _vertex {
			/// Default constructor.
			_vertex() = default;
			/// Initializes the vertex with the given data.
			_vertex(vec2d p, vec2d u, colord co) :
				pos(p.convert<float>()), uv(u.convert<float>()), c(co.convert<float>()) {
			}

			vec2f
				pos, ///< Vertex position.
				uv; ///< Texture UV coordinates.
			colorf c; ///< Color.
		};
		/// Stores an OpenGL buffer.
		///
		/// \tparam Target The desired usage of the underlying data.
		template <GLenum Target> struct _gl_buffer {
		public:
			/// Initializes the buffer to empty.
			_gl_buffer() = default;
			/// Move constructor.
			_gl_buffer(_gl_buffer &&r) : _id(r._id) {
				r._id = 0;
			}
			/// No copy construction.
			_gl_buffer(const _gl_buffer&) = delete;
			/// Move assignment.
			_gl_buffer &operator=(_gl_buffer &&r) {
				std::swap(_id, r._id);
				return *this;
			}
			/// No copy assignment.
			_gl_buffer &operator=(const _gl_buffer&) = delete;
			/// Destructor. Checks whether the buffer has been appropriately freed.
			~_gl_buffer() {
				assert_true_logical(!valid(), "unfreed OpenGL buffer");
			}

			/// Initializes the buffer, disposing the previous one if necessary.
			void initialize(opengl_renderer_base &rend) {
				dispose(rend);
				rend._gl.GenBuffers(1, &_id);
			}
			/// If the buffer is valid, unbinds the buffer, calls \p glDeleteBuffers to delete the buffer,
			/// and resets the buffer to empty.
			void dispose(opengl_renderer_base &rend) {
				if (valid()) {
					unbind(rend);
					rend._gl.DeleteBuffers(1, &_id);
					_id = 0;
				}
			}

			/// Calls \p glBindBuffer to bind this buffer.
			void bind(opengl_renderer_base &rend) {
				rend._gl.BindBuffer(Target, _id);
			}
			/// Calls \p glBindBuffer to unbind any previously bound buffer.
			static void unbind(opengl_renderer_base &rend) {
				rend._gl.BindBuffer(Target, 0);
			}

			/// Binds the buffer, then resizes the buffer with the given size and \p GL_DYNAMIC_DRAW usage by
			/// calling \p glBufferData. This operation will erase any data it previously contained.
			void clear_resize_dynamic_draw(opengl_renderer_base &rend, size_t size) {
				bind(rend);
				rend._gl.BufferData(Target, size, nullptr, GL_DYNAMIC_DRAW);
			}
			/// Resizes the buffer with the given size and \p GL_DYNAMIC_DRAW usage while keeping the previously
			/// stored data. This is achived by allocating a new buffer, copying the data, and discarding the old
			/// buffer. The buffer must be in an unmapped state when this function is called. When the call returns,
			/// and the new buffer will be mapped but won't be bound.
			///
			/// \return A pointer to the mapped memory of the new buffer.
			void *copy_resize_dynamic_draw(opengl_renderer_base &rend, size_t size) {
				GLuint newid;
				rend._gl.GenBuffers(1, &newid);
				rend._gl.BindBuffer(Target, newid);
				rend._gl.BufferData(Target, size, nullptr, GL_DYNAMIC_DRAW);
				void *newptr = rend._gl.MapBuffer(Target, GL_READ_WRITE);

				rend._gl.BindBuffer(Target, _id);
				GLint oldsize;
				rend._gl.GetBufferParameteriv(Target, GL_BUFFER_SIZE, &oldsize);
				void *ptr = rend._gl.MapBuffer(Target, GL_READ_ONLY);
				std::memcpy(newptr, ptr, std::min(static_cast<size_t>(oldsize), size));
				rend._gl.DeleteBuffers(1, &_id); // automatically unmaps and unbinds the buffer
				_id = newid;
				return newptr;
			}
			/// Binds the buffer, calls \p glMapBuffer to map the buffer with \p GL_READ_WRITE access, then returns
			/// the resulting pointer.
			void *map(opengl_renderer_base &rend) {
				bind(rend);
				return rend._gl.MapBuffer(Target, GL_READ_WRITE);
			}
			/// Binds the buffer, calls \p glUnmapBuffer to unmap this buffer. This is required before using the
			/// buffer data for rendering.
			void unmap(opengl_renderer_base &rend) {
				bind(rend);
				rend._gl.UnmapBuffer(Target);
			}

			/// Returns whether this struct contains a valid OpenGL buffer, i.e., is not empty.
			bool valid() const {
				return _id != 0;
			}
		protected:
			GLuint _id = 0; ///< The ID of the buffer.
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
					auto src = static_cast<const unsigned char*>(data);
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
#ifdef CP_CHECK_LOGICAL_ERRORS
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
				size_t page = 0; ///< The number of the page that the character is on.
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
#ifdef CP_CHECK_LOGICAL_ERRORS
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
			/// The minimum count of quads that the buffer can contain.
			constexpr static size_t minimum_allocation_size = 10;

			/// The buffer that stores vertex data.
			_gl_buffer<GL_ARRAY_BUFFER> vertex_buffer;
			/// The buffer that stores vertex indices. Its contents is reused between batches.
			_gl_buffer<GL_ELEMENT_ARRAY_BUFFER> id_buffer;
			size_t
				/// The number of quads indexed by \ref id_buffer. This can be larger than \ref quad_count
				/// since the indices can be reused.
				indexed_quad_count = 0,
				allocated_quad_count = 0, ///< The number of quads that the buffer can contain.
				quad_count = 0; ///< The number of quads added to the buffer.
			void
				*vertex_memory = nullptr, ///< Pointer to mapped \ref vertex_buffer.
				*id_memory = nullptr; ///< Pointer to mapped \ref id_buffer.

			/// Initializes \ref id_buffer and \ref vertex_buffer, and allocates memory for them..
			void initialize(opengl_renderer_base &rend) {
				allocated_quad_count = minimum_allocation_size;
				vertex_buffer.initialize(rend);
				vertex_buffer.clear_resize_dynamic_draw(rend, sizeof(_vertex) * 4 * allocated_quad_count);
				vertex_memory = vertex_buffer.map(rend);
				id_buffer.initialize(rend);
				id_buffer.clear_resize_dynamic_draw(rend, sizeof(GLuint) * 6 * allocated_quad_count);
				id_memory = id_buffer.map(rend);
			}
			/// Returns whether the buffers are valid.
			///
			/// \sa _automatic_gl_data_buffer::valid()
			bool valid() const {
				return vertex_buffer.valid();
			}
			/// Disposes \ref id_buffer and \ref vertex_buffer.
			void dispose(opengl_renderer_base &rend) {
				vertex_buffer.dispose(rend);
				id_buffer.dispose(rend);
			}

			/// Appends a character to the buffer.
			void append(opengl_renderer_base &rend, rectd layout, rectd uv, colord c) {
				if (quad_count == allocated_quad_count) {
					_enlarge(rend);
				}
				size_t vertcount = quad_count * 4;
				if (indexed_quad_count == quad_count) {
					// add more indices
					size_t idcount = quad_count * 6;
					_push_back(id_memory, static_cast<GLuint>(vertcount), idcount);
					_push_back(id_memory, static_cast<GLuint>(vertcount + 1), idcount);
					_push_back(id_memory, static_cast<GLuint>(vertcount + 2), idcount);
					_push_back(id_memory, static_cast<GLuint>(vertcount + 1), idcount);
					_push_back(id_memory, static_cast<GLuint>(vertcount + 3), idcount);
					_push_back(id_memory, static_cast<GLuint>(vertcount + 2), idcount);
					++indexed_quad_count;
				}
				// add vertices
				_push_back(vertex_memory, _vertex(layout.xmin_ymin(), uv.xmin_ymin(), c), vertcount);
				_push_back(vertex_memory, _vertex(layout.xmax_ymin(), uv.xmax_ymin(), c), vertcount);
				_push_back(vertex_memory, _vertex(layout.xmin_ymax(), uv.xmin_ymax(), c), vertcount);
				_push_back(vertex_memory, _vertex(layout.xmax_ymax(), uv.xmax_ymax(), c), vertcount);
				++quad_count;
			}
			/// Draws all buffered characters with the given texture. The caller is responsible of checking if there
			/// is actually any characters to render.
			///
			/// \todo Are these glVertexAttribPointer really necessary?
			void flush(opengl_renderer_base &rend, GLuint tex) {
				vertex_buffer.unmap(rend);
				id_buffer.unmap(rend);

				rend._defaultprog.acivate(rend);
				rend._gl.VertexAttribPointer(
					0, 2, GL_FLOAT, false, sizeof(_vertex), reinterpret_cast<const GLvoid*>(offsetof(_vertex, pos))
				);
				rend._gl.EnableVertexAttribArray(0);
				rend._gl.VertexAttribPointer(
					1, 2, GL_FLOAT, false, sizeof(_vertex), reinterpret_cast<const GLvoid*>(offsetof(_vertex, uv))
				);
				rend._gl.EnableVertexAttribArray(1);
				rend._gl.VertexAttribPointer(
					2, 4, GL_FLOAT, false, sizeof(_vertex), reinterpret_cast<const GLvoid*>(offsetof(_vertex, c))
				);
				rend._gl.EnableVertexAttribArray(2);
				rend._gl.ActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, tex);
				glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(quad_count * 6), GL_UNSIGNED_INT, nullptr);
				quad_count = 0;

				vertex_memory = vertex_buffer.map(rend);
				id_memory = id_buffer.map(rend);
			}
		protected:
			/// Enlarges all buffers to twice their previous sizes.
			void _enlarge(opengl_renderer_base &rend) {
				allocated_quad_count *= 2;
				id_buffer.unmap(rend);
				vertex_buffer.unmap(rend);
				id_memory = id_buffer.copy_resize_dynamic_draw(rend, sizeof(GLuint) * 6 * allocated_quad_count);
				vertex_memory = vertex_buffer.copy_resize_dynamic_draw(rend, sizeof(_vertex) * 4 * allocated_quad_count);
			}
			/// Given an array of objects and the current position, puts the given object at the position and then
			/// increments the position.
			template <typename T> void _push_back(void *mem, T obj, size_t &pos) {
				static_cast<T*>(mem)[pos++] = obj;
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

		/// Uses the given platform-specific function to obtain the required OpenGL function.
		template <typename T> inline static void _get_func(
			T &f, const GLchar *name, void *(*get_gl_func)(const GLchar*)
		) {
			f = reinterpret_cast<T>(get_gl_func(name));
		}
		/// Called by derived classes to initialize OpenGL.
		///
		/// \param get_gl_func The function used to obtain OpenGL function pointers. It is responsible
		///        for error handling.
		void _initialize_gl(void *(*get_gl_func)(const GLchar*)) {
			// get all gl function pointers
			_get_func(_gl.GenBuffers, "glGenBuffers", get_gl_func);
			_get_func(_gl.BindBuffer, "glBindBuffer", get_gl_func);
			_get_func(_gl.BufferData, "glBufferData", get_gl_func);
			_get_func(_gl.GetBufferParameteriv, "glGetBufferParameteriv", get_gl_func);
			_get_func(_gl.MapBuffer, "glMapBuffer", get_gl_func);
			_get_func(_gl.UnmapBuffer, "glUnmapBuffer", get_gl_func);
			_get_func(_gl.DeleteBuffers, "glDeleteBuffers", get_gl_func);

			_get_func(_gl.GenVertexArrays, "glGenVertexArrays", get_gl_func);
			_get_func(_gl.BindVertexArray, "glBindVertexArray", get_gl_func);
			_get_func(_gl.VertexAttribPointer, "glVertexAttribPointer", get_gl_func);
			_get_func(_gl.EnableVertexAttribArray, "glEnableVertexAttribArray", get_gl_func);
			_get_func(_gl.DeleteVertexArrays, "glDeleteVertexArrays", get_gl_func);

			_get_func(_gl.GenFramebuffers, "glGenFramebuffers", get_gl_func);
			_get_func(_gl.BindFramebuffer, "glBindFramebuffer", get_gl_func);
			_get_func(_gl.FramebufferTexture2D, "glFramebufferTexture2D", get_gl_func);
			_get_func(_gl.CheckFramebufferStatus, "glCheckFramebufferStatus", get_gl_func);
			_get_func(_gl.DeleteFramebuffers, "glDeleteFramebuffers", get_gl_func);

			_get_func(_gl.ActiveTexture, "glActiveTexture", get_gl_func);
			_get_func(_gl.GenerateMipmap, "glGenerateMipmap", get_gl_func);

			_get_func(_gl.CreateShader, "glCreateShader", get_gl_func);
			_get_func(_gl.ShaderSource, "glShaderSource", get_gl_func);
			_get_func(_gl.CompileShader, "glCompileShader", get_gl_func);
			_get_func(_gl.GetShaderiv, "glGetShaderiv", get_gl_func);
			_get_func(_gl.GetShaderInfoLog, "glGetShaderInfoLog", get_gl_func);
			_get_func(_gl.DeleteShader, "glDeleteShader", get_gl_func);

			_get_func(_gl.GetUniformLocation, "glGetUniformLocation", get_gl_func);
			_get_func(_gl.Uniform1i, "glUniform1i", get_gl_func);
			_get_func(_gl.Uniform2fv, "glUniform2fv", get_gl_func);
			_get_func(_gl.UniformMatrix3fv, "glUniformMatrix3fv", get_gl_func);

			_get_func(_gl.CreateProgram, "glCreateProgram", get_gl_func);
			_get_func(_gl.AttachShader, "glAttachShader", get_gl_func);
			_get_func(_gl.LinkProgram, "glLinkProgram", get_gl_func);
			_get_func(_gl.GetProgramiv, "glGetProgramiv", get_gl_func);
			_get_func(_gl.UseProgram, "glUseProgram", get_gl_func);
			_get_func(_gl.DeleteProgram, "glDeleteProgram", get_gl_func);


			// scissor test is enabled and disabled on the fly
			glEnable(GL_BLEND);
			// uses hard-coded fixed-pipeline-like shaders
			_defaultprog.initialize(*this,
				R"shader(
					#version 330 core

					layout (location = 0) in vec2 inPosition;
					layout (location = 1) in vec2 inUV;
					layout (location = 2) in vec4 inColor;

					out vec2 UV;
					out vec4 Color;

					uniform mat3 Transform;
					uniform vec2 HalfScreenSize;

					void main() {
						gl_Position = vec4(
							((Transform * vec3(inPosition, 1.0f)).xy - abs(HalfScreenSize)) / HalfScreenSize,
							0.0, 1.0
						);
						UV = inUV;
						Color = inColor;
					}
				)shader",
				R"shader(
					#version 330 core

					in vec2 UV;
					in vec4 Color;

					out vec4 outFragColor;

					uniform sampler2D Texture;

					void main() {
						outFragColor = Color * texture(Texture, UV);
					}
				)shader"
			);
			_defaultprog.acivate(*this);
			_defaultprog.set_int(*this, "Texture", 0);
			_textbuf.initialize(*this);

			// generate the default blank texture
			glGenTextures(1, &_blanktex);
			glBindTexture(GL_TEXTURE_2D, _blanktex);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			unsigned char c[4]{255, 255, 255, 255};
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, c);

			// generate VAO
			_gl.GenVertexArrays(1, &_vao);
			_gl.BindVertexArray(_vao);

			_gl_verify();
		}
		/// Called by derived classes to dispose all OpenGL related resources.
		void _dispose_gl_rsrc() {
			_gl.DeleteVertexArrays(1, &_vao);
			glDeleteTextures(1, &_blanktex);
			_atl.dispose();
			_textbuf.dispose(*this);
			_defaultprog.dispose(*this);
		}

		/// Flushes the text buffer if necessary, then starts to render to the given target.
		void _begin_render_target(_render_target_stackframe rtf) {
			if (!_rtfstk.empty()) {
				_flush_text_buffer();
			}
			_rtfstk.push_back(std::move(rtf));
			_matstk.emplace_back();
			_matstk.back().set_identity();
			_continue_last_render_target();
		}
		/// Sets the matrix at the top of \ref _matstk as \p Transform in \ref _defaultprog.
		void _apply_matrix() {
			_defaultprog.set_mat3(*this, "Transform", _matstk.back());
		}
		/// Continues to render to the target at the top of \ref _rtfstk.
		void _continue_last_render_target() {
			_render_target_stackframe &rtf = _rtfstk.back();
			rtf.begin();

			rtf.apply_config();
			glViewport(0, 0, static_cast<GLsizei>(rtf.width), static_cast<GLsizei>(rtf.height));
			vec2d halfsize(static_cast<double>(rtf.width) * 0.5, static_cast<double>(rtf.height) * 0.5);
			if (rtf.invert_y) {
				halfsize.y = -halfsize.y;
			}
			_defaultprog.set_vec2(*this, "HalfScreenSize", halfsize);
			_apply_matrix();

			_gl_verify();
		}

		/// Flushes the text buffer by calling _text_buffer::flush.
		void _flush_text_buffer() {
			if (_textbuf.quad_count > 0) {
				_textbuf.flush(*this, _atl.get_page(_gl, _lstpg).tex_id);
			}
		}
		/// Checks for any OpenGL errors.
		void _gl_verify() {
#ifdef CP_CHECK_SYSTEM_ERRORS
			GLenum errorcode = glGetError();
			if (errorcode != GL_NO_ERROR) {
				logger::get().log_error(CP_HERE, "OpenGL error code ", errorcode);
				assert_true_sys(false, "OpenGL error");
			}
#endif
		}

		_gl_funcs _gl; ///< Additional OpenGL routines needed by the renderer.
		std::vector<_render_target_stackframe> _rtfstk; ///< The stack of render targets.
		_text_atlas _atl; ///< The text atlas.
		_text_buffer _textbuf; ///< The text buffer.
		std::vector<matd3x3> _matstk; ///< The stack of matrices.
		size_t _lstpg = 0; ///< The page that all characters in \ref _textbuf is on.
		_gl_program _defaultprog; ///< The default OpenGL program used for rendering.
		/// A blank texture. This is used since sampling texture 0 in shaders return (0, 0, 0, 1) rather
		/// than (1, 1, 1, 1) in the fixed pipeline (sort of).
		GLuint _vao = 0, _blanktex = 0;
	};
}
