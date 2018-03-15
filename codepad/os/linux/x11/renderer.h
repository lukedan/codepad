#pragma once

/// \file
/// Implementation of renderers for the linux platform when using X11.

#include <GL/glx.h>
#include <GL/gl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "../../renderer.h"
#include "window.h"

namespace codepad::os {
	/// Implementation of the OpenGL renderer.
	class opengl_renderer : public opengl_renderer_base {
	public:
		/// Initializes the renderer.
		opengl_renderer() : opengl_renderer_base() {
			_details::xlib_link &di = _details::xlib_link::get();
			int nconfigs = 0;
			GLXFBConfig *cfgs = glXChooseFBConfig(di.display, DefaultScreen(di.display), _fbattribs, &nconfigs);
			assert_true_sys(cfgs != nullptr, "no suitable config found");
			_fbc = cfgs[0];
			XFree(cfgs);
			di.visual_info = glXGetVisualFromFBConfig(di.display, _fbc);
			assert_true_sys(di.visual_info != nullptr, "invalid config");
			_ctx = glXCreateNewContext(di.display, _fbc, GLX_RGBA_TYPE, nullptr, true);
			assert_true_sys(_ctx != nullptr, "cannot create context");
			di.attributes.colormap = XCreateColormap(
				di.display, RootWindow(di.display, di.visual_info->screen), di.visual_info->visual, AllocNone
			);
			di.attributes.background_pixel = BlackPixel(di.display, di.visual_info->screen);
			di.attributes.border_pixel = BlackPixel(di.display, di.visual_info->screen);
			assert_true_sys(
				glXMakeCurrent(_details::xlib_link::get().display, None, _ctx) != False,
				"failed to bind context"
			);
			_init_gl_funcs();
			XSync(di.display, false);
		}
		/// Disposes the renderer.
		~opengl_renderer() override {
			// will crash if these aren't disabled
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_BLEND);
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);
			Display *disp = _details::xlib_link::get().display;
			_dispose_gl_rsrc();
			XSync(disp, false);
			glXDestroyContext(disp, _ctx);
			assert_true_sys(glXMakeCurrent(disp, None, nullptr) != False, "failed to unbind context");
		}
	protected:
		/// Returns the OpenGL extension functions.
		const _gl_funcs &_get_gl_funcs() override {
			return _gl;
		}
		/// Windows don't need special initialization.
		void _init_new_window(window_base&) override {
		}
		/// Returns a function that calls \p glXMakeCurrent to switch the current context.
		std::function<void()> _get_begin_window_func(const window_base &wnd) override {
			const window *cw = static_cast<const window*>(&wnd);
			return [this, cw]() {
				assert_true_sys(
					glXMakeCurrent(_details::xlib_link::get().display, cw->_win, _ctx) != False,
					"failed to switch context"
				);
			};
		}
		/// Returns a function that calls \p glXSwapBuffers to refresh the display.
		std::function<void()> _get_end_window_func(const window_base &wnd) override {
			const window *cw = static_cast<const window*>(&wnd);
			return [cw]() {
				glXSwapBuffers(_details::xlib_link::get().display, cw->_win);
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

		static const int _fbattribs[19]; ///< The desired configuration of the OpenGL context.

		GLXContext _ctx = nullptr; ///< The GLX context.
		GLXFBConfig _fbc; ///< The GLX framebuffer config.
		_gl_funcs _gl; ///< The list of OpenGL functions.
	};
}
