#pragma once

#include <GL/glx.h>
#include <GL/gl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "../../renderer.h"
#include "window.h"

namespace codepad::os {
	class software_renderer : public software_renderer_base {
	public:
		/// \todo Find the cairo operator corresponding to <tt>GL_ONE, GL_ONE_MINUS_SRC_ALPHA</tt>.
		void begin(const window_base &wnd) override {
			auto *cwnd = static_cast<const window*>(&wnd);
			_wnd_rec *crec = &_wnds.find(cwnd)->second;
			_begin_render_target(_render_target_stackframe(
				crec->width, crec->height, crec->buffer, [this, cwnd, crec]() {
					if (crec->buf.buf) {
						// copy buffer data to GdkPixbuf
						int rowstride = gdk_pixbuf_get_rowstride(crec->buf.buf);
						guchar *dst = gdk_pixbuf_get_pixels(crec->buf.buf);
						_color_t *from = crec->buffer;
						for (size_t y = 0; y < crec->height; ++y, dst += rowstride) {
							guchar *cdst = dst;
							for (size_t x = 0; x < crec->width; ++x, ++from, cdst += 4) {
								colori ci = from->convert<unsigned char>();
								cdst[0] = ci.r;
								cdst[1] = ci.g;
								cdst[2] = ci.b;
								cdst[3] = 255; // hack for now
							}
						}
						// use cairo to draw the buffer
						cairo_rectangle_int_t rgnrect{};
						rgnrect.x = rgnrect.y = 0;
						rgnrect.width = static_cast<int>(crec->width);
						rgnrect.height = static_cast<int>(crec->height);
						cairo_region_t *rgn = cairo_region_create_rectangle(&rgnrect);
						GdkDrawingContext *ctx = gdk_window_begin_draw_frame(cwnd->_wnd, rgn);
						cairo_t *cairoctx = gdk_drawing_context_get_cairo_context(ctx);
						gdk_cairo_set_source_pixbuf(cairoctx, crec->buf.buf, 0.0, 0.0);
						cairo_paint(cairoctx);
						gdk_window_end_draw_frame(cwnd->_wnd, ctx);
						cairo_region_destroy(rgn);
					}
				}
			));
			_clear_texture(crec->buffer, crec->width, crec->height);
		}
	protected:
		void _new_window(window_base &wnd) override {
			auto *w = static_cast<window*>(&wnd);
			_wnd_rec wr;
			wr.resize_buffer(static_cast<size_t>(w->_width), static_cast<size_t>(w->_height));
			auto it = _wnds.emplace(w, std::move(wr)).first;
			w->size_changed += [it](size_changed_info &info) {
				it->second.resize_buffer(static_cast<size_t>(info.new_size.x), static_cast<size_t>(info.new_size.y));
			};
		}
		void _delete_window(window_base &wnd) override {
			assert_true_logical(_wnds.erase(static_cast<window*>(&wnd)) == 1, "corrupted window registry");
		}

		struct _pixbuf {
			_pixbuf() = default;
			_pixbuf(size_t w, size_t h) {
				buf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8, static_cast<int>(w), static_cast<int>(h));
				assert_true_sys(buf != nullptr, "cannot create pixbuf");
			}
			_pixbuf(_pixbuf &&src) : buf(src.buf) {
				src.buf = nullptr;
			}
			_pixbuf(const _pixbuf&) = delete;
			_pixbuf &operator=(_pixbuf &&src) {
				std::swap(buf, src.buf);
				return *this;
			}
			_pixbuf &operator=(const _pixbuf&) = delete;
			~_pixbuf() {
				if (buf) {
					g_object_unref(buf);
				}
			}

			GdkPixbuf *buf = nullptr;
		};
		struct _wnd_rec {
			_wnd_rec() = default;
			_wnd_rec(_wnd_rec &&src) :
				buf(std::move(src.buf)), buffer(src.buffer), width(src.width), height(src.height) {
				src.buffer = nullptr;
			}
			_wnd_rec(const _wnd_rec&) = delete;
			_wnd_rec &operator=(_wnd_rec &&src) {
				std::swap(src.buf, buf);
				std::swap(buffer, src.buffer);
				std::swap(width, src.width);
				std::swap(height, src.height);
				return *this;
			}
			_wnd_rec &operator=(const _wnd_rec&) = delete;
			~_wnd_rec() {
				if (buffer) {
					delete[] buffer;
				}
			}

			void resize_buffer(size_t w, size_t h) {
				width = w;
				height = h;
				if (width != 0 && height != 0) {
					buf = _pixbuf(width, height);
				}
				if (buffer) {
					delete[] buffer;
				}
				buffer = new _color_t[width * height];
			}

			_pixbuf buf;
			_color_t *buffer = nullptr;
			size_t width = 0, height = 0;
		};

		std::unordered_map<const window*, _wnd_rec> _wnds;
	};

	/// \todo Crashes on startup.
	class opengl_renderer : public opengl_renderer_base {
	public:
		opengl_renderer() = default;
		~opengl_renderer() override {
			_dispose_gl_rsrc();
			// will crash if these aren't disabled before disposal
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_BLEND);
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);
			gdk_gl_context_clear_current();
			// TODO GdkGLContext no need for disposal?
		}
	protected:
		const _gl_funcs &_get_gl_funcs() override {
			return _gl;
		}
		void _init_new_window(window_base &wnd) override {
#ifdef CP_DETECT_LOGICAL_ERRORS
			window *w = dynamic_cast<window*>(&wnd);
			assert_true_logical(w != nullptr, "invalid window passed to renderer");
#else
			window *w = static_cast<window*>(&wnd);
#endif
			if (_ctx == nullptr) { // create the context, which will always be the current context
				GError *err = nullptr;
				_ctx = gdk_window_create_gl_context(w->_wnd, &err);
				if (err == nullptr) {
					gdk_gl_context_set_required_version(_ctx, 4, 0);
					assert_true_sys(gdk_gl_context_realize(_ctx, &err) == TRUE, "cannot realize OpenGL context");
					int minor, major;
					gdk_gl_context_get_version(_ctx, &major, &minor);
					logger::get().log_verbose(CP_HERE, "OpenGL version: ", major, ".", minor);
#ifdef CP_DETECT_LOGICAL_ERRORS
					if (err == nullptr) {
						gdk_gl_context_set_debug_enabled(_ctx, true);
					}
#endif
				}
				if (err) {
					logger::get().log_error(CP_HERE, "GDK error: ", err->message);
					assert_true_sys(false, "failed to create OpenGL context");
				}
				gdk_gl_context_make_current(_ctx);
				_init_gl_funcs();
			}
		}
		std::function<void()> _get_begin_window_func(const window_base &wnd) override {
			const window *cw = static_cast<const window*>(&wnd);
			framebuffer fb = new_framebuffer(
				static_cast<size_t>(cw->_width), static_cast<size_t>(cw->_height)
			);
			GLuint id = static_cast<GLuint>(_get_id(fb));
			_wnd_buffers.push_back(std::move(fb));
			return [this, id]() {
				_gl.BindFramebuffer(GL_FRAMEBUFFER, id);
			};
		}
		std::function<void()> _get_end_window_func(const window_base &wnd) override {
			const window *cw = static_cast<const window*>(&wnd);
			return [this, cw]() {
				framebuffer fb = std::move(_wnd_buffers.back()); // retrieve the buffer
				_wnd_buffers.pop_back();
				_gl.BindFramebuffer(GL_FRAMEBUFFER, 0); // unbind the buffer
				// use cairo to draw the buffer
				cairo_rectangle_int_t rgnrect;
				rgnrect.x = rgnrect.y = 0;
				rgnrect.width = cw->_width;
				rgnrect.height = cw->_height;
				cairo_region_t *rgn = cairo_region_create_rectangle(&rgnrect);
				GdkDrawingContext *ctx = gdk_window_begin_draw_frame(cw->_wnd, rgn);
				cairo_t *cairoctx = gdk_drawing_context_get_cairo_context(ctx);
				gdk_cairo_draw_from_gl(
					cairoctx, cw->_wnd, static_cast<int>(_get_id(fb.get_texture())),
					GL_TEXTURE, 1, 0, 0, cw->_width, cw->_height
				);
				gdk_window_end_draw_frame(cw->_wnd, ctx);
				cairo_region_destroy(rgn);
			};
		}

		/// Acquires the function pointer to the specified OpenGL function by calling \p glXGetProcAddress().
		///
		/// \param f A reference to the function pointer.
		/// \param name The name of the OpenGL function.
		template <typename T> inline static void _get_func(T &f, const char *name) {
			assert_true_sys((f = reinterpret_cast<T>(
				glXGetProcAddress(reinterpret_cast<const GLubyte*>(name))
			)) != nullptr, "extension not found");
		}
		/// Acquires all functions needed by \ref opengl_renderer_base.
		void _init_gl_funcs() {
			_get_func(_gl.GenBuffers, "glGenBuffers");
			_get_func(_gl.DeleteBuffers, "glDeleteBuffers");
			_get_func(_gl.BindBuffer, "glBindBuffer");
			_get_func(_gl.BufferData, "glBufferData");
			_get_func(_gl.MapBuffer, "glMapBuffer");
			_get_func(_gl.UnmapBuffer, "glUnmapBuffer");
			_get_func(_gl.GenFramebuffers, "glGenFramebuffers");
			_get_func(_gl.BindFramebuffer, "glBindFramebuffer");
			_get_func(_gl.FramebufferTexture2D, "glFramebufferTexture2D");
			_get_func(_gl.CheckFramebufferStatus, "glCheckFramebufferStatus");
			_get_func(_gl.DeleteFramebuffers, "glDeleteFramebuffers");
			_get_func(_gl.GenerateMipmap, "glGenerateMipmap");
		}

		std::vector<framebuffer> _wnd_buffers;
		_gl_funcs _gl; ///< The list of OpenGL functions.
		GdkGLContext *_ctx = nullptr;
	};
}
