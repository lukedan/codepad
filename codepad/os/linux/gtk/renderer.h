// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

#include <GL/glx.h>
#include <GL/gl.h>
#include <gtk/gtk.h>

#include "../../../ui/renderer.h"
#include "../../software_renderer_base.h"
#include "../../opengl_renderer_base.h"
#include "window.h"

namespace codepad::os {
	class software_renderer : public software_renderer_base {
	public:
		/// \todo Find the cairo operator corresponding to <tt>GL_ONE, GL_ONE_MINUS_SRC_ALPHA</tt>.
		void begin(const ui::window_base &wnd) override {
			auto *cwnd = static_cast<const window*>(&wnd);
			_wnd_rec *crec = &_wnds.find(cwnd)->second;
			_begin_render_target(_render_target_stackframe(
				crec->texture.w, crec->texture.h, crec->texture.data, [cwnd, crec]() {
					if (crec->buf.surface) {
						// copy buffer data to cairo surface
						cairo_surface_flush(crec->buf.surface);
						int rowstride = cairo_image_surface_get_stride(crec->buf.surface);
						unsigned char *dst = cairo_image_surface_get_data(crec->buf.surface);
						_ivec4f *from = crec->texture.data;
						for (size_t y = 0; y < crec->texture.h; ++y, dst += rowstride) {
							unsigned char *cdst = dst;
							for (size_t x = 0; x < crec->texture.w; ++x, ++from, cdst += 4) {
								colori ci = _convert_to_colorf(*from).convert<unsigned char>();
								cdst[0] = ci.b;
								cdst[1] = ci.g;
								cdst[2] = ci.r;
								cdst[3] = 255; // hack for now
							}
						}
						cairo_surface_mark_dirty(crec->buf.surface);
						gtk_widget_queue_draw(cwnd->_wnd);
					}
				}
			));
			_clear_texture(crec->texture.data, crec->texture.w, crec->texture.h);
		}
	protected:
		/// Checks for any errors in a \p cairo_t.
		inline static void _cairo_check(cairo_t *cr) {
			assert_true_sys(cairo_status(cr) == CAIRO_STATUS_SUCCESS, "cairo error");
		}

		struct _cairo_buf {
			_cairo_buf() = default;
			_cairo_buf(size_t w, size_t h) {
				surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, static_cast<int>(w), static_cast<int>(h));
				assert_true_sys(
					cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS, "failed to create Cairo surface"
					);
			}
			_cairo_buf(_cairo_buf &&src) noexcept : surface(src.surface) {
				src.surface = nullptr;
			}
			_cairo_buf(const _cairo_buf&) = delete;
			_cairo_buf &operator=(_cairo_buf &&src) noexcept {
				std::swap(surface, src.surface);
				return *this;
			}
			_cairo_buf &operator=(const _cairo_buf&) = delete;
			~_cairo_buf() {
				if (surface) {
					cairo_surface_destroy(surface);
				}
			}

			cairo_surface_t *surface = nullptr;
		};
		struct _wnd_rec {
			void resize_buffer(size_t w, size_t h) {
				texture.resize(w, h);
				if (texture.w != 0 && texture.h != 0) {
					buf = _cairo_buf(texture.w, texture.h);
				}
			}

			_cairo_buf buf;
			_tex_rec texture;
		};

		inline static gboolean _actual_render(GtkWidget*, cairo_t *cr, _wnd_rec *rend) {
			cairo_set_source_surface(cr, rend->buf.surface, 0.0, 0.0);
			cairo_paint(cr);
			_cairo_check(cr);
			return true;
		}

		void _new_window(ui::window_base &wnd) override {
			auto *w = static_cast<window*>(&wnd);
			_wnd_rec wr;
			auto it = _wnds.emplace(w, std::move(wr)).first;
			w->size_changed += [it, w](ui::size_changed_info &info) {
				it->second.resize_buffer(static_cast<size_t>(info.new_size.x), static_cast<size_t>(info.new_size.y));
				w->get_manager().get_scheduler().update_layout_and_visual();
			};
			g_signal_connect(w->_wnd, "draw", reinterpret_cast<GCallback>(_actual_render), &it->second);
		}
		void _delete_window(ui::window_base &wnd) override {
			assert_true_logical(_wnds.erase(static_cast<window*>(&wnd)) == 1, "corrupted window registry");
		}

		std::map<const window*, _wnd_rec> _wnds;
	};

	/// \todo No display, crashes, etc.
	class opengl_renderer : public opengl_renderer_base {
	public:
		opengl_renderer() = default;
		~opengl_renderer() override {
			_dispose_gl_rsrc();
		}
	protected:
		struct _wnd_rec {
			_wnd_rec() = default;
			explicit _wnd_rec(GtkWidget *w) : widget(w) {
			}

			GtkWidget *widget = nullptr;
			ui::framebuffer buffer;
		};

		inline static gboolean _on_glarea_render(GtkGLArea *area, GdkGLContext *ctx, window *wnd) {
			logger::get().log_error(CP_HERE, "fuck no");
			auto &rend = static_cast<opengl_renderer&>(wnd->get_manager().get_renderer());
			_wnd_rec &rec = rend._wndmapping[wnd];
			_gl_buffer<GL_ARRAY_BUFFER> buf;
			buf.initialize(rend);
			buf.clear_resize_dynamic_draw(rend, sizeof(_vertex) * 6);
			auto ptr = static_cast<_vertex*>(buf.map(rend));
			vec2d sz = wnd->get_client_size().convert<double>();
			ptr[0] = _vertex(vec2d(0.0, 0.0), vec2d(0.0, 0.0), colord());
			ptr[1] = _vertex(vec2d(sz.x, 0.0), vec2d(1.0, 0.0), colord());
			ptr[2] = _vertex(vec2d(0.0, sz.y), vec2d(0.0, 1.0), colord());
			ptr[3] = _vertex(vec2d(sz.x, 0.0), vec2d(1.0, 0.0), colord());
			ptr[4] = _vertex(sz, vec2d(1.0, 1.0), colord());
			ptr[5] = _vertex(vec2d(0.0, sz.y), vec2d(0.0, 1.0), colord());
			buf.unmap(rend);

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
			glBindTexture(GL_TEXTURE_2D, static_cast<GLsizei>(_get_id(rec.buffer.get_texture())));
			glDrawArrays(GL_TRIANGLES, 0, 6);

			buf.dispose(rend);
			return true;
		}

		void _new_window(ui::window_base &wnd) override {
#ifdef CP_CHECK_LOGICAL_ERRORS
			auto w = dynamic_cast<window*>(&wnd);
			assert_true_logical(w != nullptr, "invalid window passed to renderer");
#else
			window *w = static_cast<window*>(&wnd);
#endif
			GtkWidget *glarea = gtk_gl_area_new();
			gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(glarea), false);
			gtk_gl_area_set_auto_render(GTK_GL_AREA(glarea), false);
			gtk_gl_area_set_required_version(GTK_GL_AREA(glarea), 3, 3);
			gtk_container_add(GTK_CONTAINER(w->_wnd), glarea);
			g_signal_connect(glarea, "render", reinterpret_cast<GCallback>(_on_glarea_render), w);
			gtk_widget_realize(glarea);
			gtk_gl_area_make_current(GTK_GL_AREA(glarea));
			GError *err = gtk_gl_area_get_error(GTK_GL_AREA(glarea));
			if (err) {
				logger::get().log_error(CP_HERE, "GTK GLarea error: ", err->message);
				assert_true_sys(false, "GLarea error");
			}
			if (!_init) {
				_init = true;
				_initialize_gl(_get_gl_func);
			}
			auto it = _wndmapping.emplace(w, glarea);
			w->size_changed += [this, it = it.first](ui::size_changed_info &info) {
				it->second.buffer = new_framebuffer(
					static_cast<size_t>(info.new_size.x), static_cast<size_t>(info.new_size.y)
				);
			};
		}
		void _delete_window(ui::window_base &wnd) override {
#ifdef CP_CHECK_LOGICAL_ERRORS
			auto w = dynamic_cast<window*>(&wnd);
			assert_true_logical(w != nullptr, "invalid window passed to renderer");
#else
			window *w = static_cast<window*>(&wnd);
#endif
			_wndmapping.erase(w);
		}
		std::function<void()> _get_begin_window_func(const ui::window_base &wnd) override {
			auto *cw = static_cast<const window*>(&wnd);
			auto it = _wndmapping.find(cw);
			gtk_gl_area_make_current(GTK_GL_AREA(it->second.widget));
			return [this, it]() {
				_gl.BindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(_get_id(it->second.buffer)));
			};
		}
		std::function<void()> _get_end_window_func(const ui::window_base &wnd) override {
			auto *cw = static_cast<const window*>(&wnd);
			auto it = _wndmapping.find(cw);
			return [this, it]() {
				_gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
				gtk_gl_area_queue_render(GTK_GL_AREA(it->second.widget));
			};
		}

		/// Wrapper of \p glXGetProcAddress().
		inline static void *_get_gl_func(const GLchar *name) {
			auto res = reinterpret_cast<void*>(glXGetProcAddress(reinterpret_cast<const GLubyte*>(name)));
			assert_true_sys(res != nullptr, "failed to acquire OpenGL functions");
			return res;
		}

		std::map<const window*, _wnd_rec> _wndmapping;
		bool _init = false;
	};
}
