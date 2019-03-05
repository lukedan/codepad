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

#include "../ui/renderer.h"

namespace codepad::os {
	/// Base class of OpenGL renderers that implements most platform-independent functionalities.
	class opengl_renderer_base : public ui::renderer_base {
	public:
		/// Calls \ref _begin_render_target with \ref _get_begin_window_func and
		/// \ref _get_end_window_func to start rendering to the given window.
		void begin(const ui::window_base&) override;
		/// Flushes the text buffer, then calls _render_target_stackframe::push_clip.
		void push_clip(recti r) override {
			_rtfstk.back().push_clip(r);
		}
		/// Flushes the text buffer, then calls _render_target_stackframe::pop_clip.
		void pop_clip() override {
			_rtfstk.back().pop_clip();
		}
		/// Flushes the text buffer, then calls \p glDrawArrays to drwa the given triangles.
		void draw_triangles(const ui::texture &t, const ui::vertex_buffer &buf, size_t n) override {
			if (n > 0) {
				_defaultprog.acivate(*this);
				_gl.BindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(_get_id(buf)));
				_gl.VertexAttribPointer(
					0, 2, GL_FLOAT, false, sizeof(ui::vertexf),
					reinterpret_cast<const GLvoid*>(offsetof(ui::vertexf, position))
				);
				_gl.EnableVertexAttribArray(0);
				_gl.VertexAttribPointer(
					1, 2, GL_FLOAT, false, sizeof(ui::vertexf),
					reinterpret_cast<const GLvoid*>(offsetof(ui::vertexf, uv))
				);
				_gl.EnableVertexAttribArray(1);
				_gl.VertexAttribPointer(
					2, 4, GL_FLOAT, false, sizeof(ui::vertexf),
					reinterpret_cast<const GLvoid*>(offsetof(ui::vertexf, color))
				);
				_gl.EnableVertexAttribArray(2);
				_gl.ActiveTexture(GL_TEXTURE0);
				auto tex = static_cast<GLuint>(_get_id(t));
				if (tex == 0) { // _blanktex for no texture
					tex = _blanktex;
				}
				glBindTexture(GL_TEXTURE_2D, tex);
				glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(n));
			}
		}
		/// Flushes the text buffer, then calls \p glDrawArrays to draw the given lines.
		///
		/// \todo Implement draw_lines.
		void draw_lines(const vec2d*, const colord*, size_t) override {
		}
		/// Flushes the text buffer, ends the current render target, removes corresponding contents from the
		/// stacks, then continues the last render target if there is one.
		void end() override {
			_rtfstk.back().end();
			assert_true_usage(_rtfstk.back().clip_stack.empty(), "pushclip/popclip mismatch");
			_rtfstk.pop_back();
			_matstk.pop_back(); // pop default identity matrix
			if (!_rtfstk.empty()) {
				_continue_last_render_target();
			}
			_gl_verify();
		}

		/// Creates a texture using the given size and pixel data.
		ui::texture new_texture(size_t w, size_t h, const std::uint8_t *data) override {
			GLuint texid;
			glGenTextures(1, &texid);
			glBindTexture(GL_TEXTURE_2D, texid);
			_set_default_texture_params();
			glTexImage2D(
				GL_TEXTURE_2D, 0, GL_RGBA8,
				static_cast<GLsizei>(w), static_cast<GLsizei>(h),
				0, GL_RGBA, GL_UNSIGNED_BYTE, data
			);
			if (data) {
				_gl.GenerateMipmap(GL_TEXTURE_2D);
			}
			return _make_texture(static_cast<ui::texture::id_t>(texid), w, h);
		}
		/// Calls \p glTexImage2D to update the contents of the texture.
		void update_texture(ui::texture &tex, const std::uint8_t *data) override {
			ui::texture::id_t id = _get_id(tex);
			glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(id));
			glTexImage2D(
				GL_TEXTURE_2D, 0, GL_RGBA8,
				static_cast<GLsizei>(tex.get_width()), static_cast<GLsizei>(tex.get_height()),
				0, GL_RGBA, GL_UNSIGNED_BYTE, data
			);
			_gl.GenerateMipmap(GL_TEXTURE_2D);
		}
		/// Deletes and erases the given \ref texture.
		void delete_texture(ui::texture &tex) override {
			auto t = static_cast<GLuint>(_get_id(tex));
			glDeleteTextures(1, &t);
			_erase_texture(tex);
		}

		/// Creates a \ref frame_buffer of the given size.
		ui::frame_buffer new_frame_buffer(size_t w, size_t h) override {
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
			return _make_frame_buffer(fbid, _make_texture(static_cast<ui::texture::id_t>(tid), w, h));
		}
		/// Deletes and erases the given \ref frame_buffer.
		void delete_frame_buffer(ui::frame_buffer &fb) override {
			auto id = static_cast<GLuint>(_get_id(fb)), tid = static_cast<GLuint>(_get_id(fb.get_texture()));
			_gl.DeleteFramebuffers(1, &id);
			glDeleteTextures(1, &tid);
			_erase_frame_buffer(fb);
		}
		/// Calls \ref continue_frame_buffer to start rendering to the \ref frame_buffer,
		/// then clears its contents.
		void begin_frame_buffer(const ui::frame_buffer &fb) override {
			continue_frame_buffer(fb);
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		/// Calls \ref _begin_render_target to continue rendering to the given \ref frame_buffer.
		void continue_frame_buffer(const ui::frame_buffer &fb) override {
			assert_true_usage(fb.has_contents(), "cannot draw to an empty frame buffer");
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

		/// Calls \p glGenBuffers() and \p glBufferData() to generate a buffer and allocate memory.
		ui::vertex_buffer new_vertex_buffer(size_t sz) override {
			GLuint buf;
			_gl.GenBuffers(1, &buf);
			_gl.BindBuffer(GL_ARRAY_BUFFER, buf);
			_gl.BufferData(GL_ARRAY_BUFFER, sz * sizeof(ui::vertexf), nullptr, GL_DYNAMIC_DRAW);
			return _make_vertex_buffer(static_cast<size_t>(buf), sz);
		}
		/// Calls \p glDeleteBuffers to delete the given \ref vertex_buffer. The buffer must not be mapped.
		void delete_vertex_buffer(ui::vertex_buffer &buf) override {
			auto id = static_cast<GLuint>(_get_id(buf));
			_gl.DeleteBuffers(1, &id);
			_erase_vertex_buffer(buf);
		}
		/// Calls \p glMapBuffer() to map the given buffer.
		ui::vertexf *map_vertex_buffer(ui::vertex_buffer &buf) override {
			_gl.BindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(_get_id(buf)));
			return static_cast<ui::vertexf*>(_gl.MapBuffer(GL_ARRAY_BUFFER, GL_READ_WRITE));
		}
		/// Calls \p glUnmapBuffer to unmap the given buffer.
		void unmap_vertex_buffer(ui::vertex_buffer &buf) override {
			_gl.BindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(_get_id(buf)));
			_gl.UnmapBuffer(GL_ARRAY_BUFFER);
		}


		/// Flushes the text buffer, then calls \p glPushMatrix and \p glLoadMatrix to push
		/// a matrix onto the stack.
		void push_matrix(const matd3x3 &m) override {
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
			_matstk.pop_back();
			_apply_matrix();
		}

		/// Flushes the text buffer, then calls _render_target_stackframe::push_blend_func to
		/// push a \ref blend_function onto the stack.
		void push_blend_function(ui::blend_function f) override {
			_rtfstk.back().push_blend_func(f);
		}
		/// Flushes the text buffer, then calls _render_target_stackframe::pop_blend_func to
		/// pop a \ref blend_function from the stack.
		void pop_blend_function() override {
			_rtfstk.back().pop_blend_func();
		}
		/// Returns the \ref blend_function on the top of _render_target_stackframe::blend_function_stack.
		ui::blend_function top_blend_function() const override {
			if (_rtfstk.back().blend_func_stack.empty()) {
				return ui::blend_function();
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
		virtual std::function<void()> _get_begin_window_func(const ui::window_base&) = 0;
		/// Called to obtain a function that is called when the renderer has finished drawing
		/// to the window, to present the rendered result on the window.
		virtual std::function<void()> _get_end_window_func(const ui::window_base&) = 0;

		/// Does nothing.
		void _delete_window(ui::window_base&) override {
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
			std::vector<ui::blend_function> blend_func_stack;

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
			void push_blend_func(ui::blend_function bf) {
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
			_defaultprog.dispose(*this);
		}

		/// Flushes the text buffer if necessary, then starts to render to the given target.
		void _begin_render_target(_render_target_stackframe rtf) {
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
		std::vector<matd3x3> _matstk; ///< The stack of matrices.
		_gl_program _defaultprog; ///< The default OpenGL program used for rendering.
		/// A blank texture. This is used since sampling texture 0 in shaders return (0, 0, 0, 1) rather
		/// than (1, 1, 1, 1) in the fixed pipeline (sort of).
		GLuint _vao = 0, _blanktex = 0;
	};
}
