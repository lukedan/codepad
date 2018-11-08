// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

#include <cstring>

#include "misc.h"
#include <gl/GL.h>
#include <gl/glext.h>

#include "../../ui/element.h"
#include "window.h"
#include "../opengl_renderer_base.h"
#include "../software_renderer_base.h"

namespace codepad {
	namespace os {
		class software_renderer : public software_renderer_base {
		public:
			void begin(const window_base &wnd) override {
				const auto *cwnd = static_cast<const window*>(&wnd);
				_wnd_rec *crec = &_wnds.find(cwnd)->second;
				_begin_render_target(_render_target_stackframe(
					crec->texture.w, crec->texture.h, crec->texture.data, [cwnd, crec]() {
						DWORD *to = crec->bmp.arr;
						_ivec4f *from = crec->texture.data;
						for (size_t i = crec->texture.w * crec->texture.h; i > 0; --i, ++from, ++to) {
							*to = static_cast<DWORD>(
								(from->shuffle<3, 2, 1, 0>() * 255.0f).convert_to_int_truncate().pack()
								);
						}
						winapi_check(BitBlt(
							cwnd->_dc, 0, 0, static_cast<int>(crec->texture.w), static_cast<int>(crec->texture.h),
							crec->dc, 0, 0, SRCCOPY
						));
					}
				));
				_render_target_stackframe &frame = _rtfstk.back();
				_clear_texture(frame.buffer, frame.width, frame.height);
			}
		protected:
			void _new_window(window_base &wnd) override {
				auto *w = static_cast<window*>(&wnd);
				_wnd_rec wr;
				vec2i sz = w->get_layout().size().convert<int>();
				wr.create_buffer(w->_dc, sz.x, sz.y);
				auto it = _wnds.insert(std::make_pair(w, std::move(wr))).first;
				w->size_changed += [it](size_changed_info &info) {
					it->second.resize_buffer(static_cast<size_t>(info.new_size.x), static_cast<size_t>(info.new_size.y));
				};
			}
			void _delete_window(window_base &wnd) override {
				auto it = _wnds.find(static_cast<window*>(&wnd));
				assert_true_logical(it != _wnds.end(), "corrupted window registry");
				it->second.dispose_buffer();
				_wnds.erase(it);
			}

			struct _dev_bitmap {
				HGDIOBJ create_and_select(HDC dc, size_t w, size_t h) {
					BITMAPINFO info;
					std::memset(&info, 0, sizeof(info));
					info.bmiHeader.biSize = sizeof(info.bmiHeader);
					info.bmiHeader.biWidth = static_cast<LONG>(w);
					info.bmiHeader.biHeight = -static_cast<LONG>(h);
					info.bmiHeader.biPlanes = 1;
					info.bmiHeader.biBitCount = 32;
					info.bmiHeader.biCompression = BI_RGB;
					winapi_check(handle = CreateDIBSection(
						dc, &info, DIB_RGB_COLORS, reinterpret_cast<void**>(&arr), nullptr, 0
					));

					HGDIOBJ obj = SelectObject(dc, handle);
					winapi_check(obj);
					return obj;
				}
				void dispose() {
					winapi_check(DeleteObject(handle));
				}
				void unselect_and_dispose(HDC dc, HGDIOBJ old) {
					winapi_check(SelectObject(dc, old));
					dispose();
				}

				HBITMAP handle;
				DWORD *arr;
			};
			struct _wnd_rec {
				void create_buffer(HDC ndc, size_t w, size_t h) {
					texture.resize(w, h);
					winapi_check(dc = CreateCompatibleDC(ndc));
					if (texture.w != 0 && texture.h != 0) {
						old = bmp.create_and_select(dc, texture.w, texture.h);
					}
				}
				void resize_buffer(size_t w, size_t h) {
					texture.resize(w, h);
					if (texture.w != 0 && texture.h != 0) {
						_dev_bitmap newbmp;
						HGDIOBJ ov = newbmp.create_and_select(dc, texture.w, texture.h);
						if (old == nullptr) {
							old = ov;
						} else {
							bmp.dispose();
						}
						bmp = newbmp;
					}
				}
				void dispose_buffer() {
					if (old != nullptr) {
						bmp.unselect_and_dispose(dc, old);
					}
					winapi_check(DeleteDC(dc));
					texture.dispose();
				}

				_tex_rec texture;
				_dev_bitmap bmp;
				HGDIOBJ old = nullptr;
				HDC dc;
			};

			std::unordered_map<const window*, _wnd_rec> _wnds;
		};

		class opengl_renderer : public opengl_renderer_base {
		public:
			opengl_renderer() {
				std::memset(&_pfd, 0, sizeof(_pfd));
				_pfd.nSize = sizeof(_pfd);
				_pfd.nVersion = 1;
				_pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
				_pfd.iPixelType = PFD_TYPE_RGBA;
				_pfd.cColorBits = 32;
				_pfd.iLayerType = PFD_MAIN_PLANE;
				winapi_check(_pformat = ChoosePixelFormat(GetDC(nullptr), &_pfd));
			}
			~opengl_renderer() override {
				_dispose_gl_rsrc();
				winapi_check(wglMakeCurrent(nullptr, nullptr));
				winapi_check(wglDeleteContext(_rc));
			}
		protected:
			inline static void *_get_gl_func(const GLchar *name) {
				void *p = reinterpret_cast<void*>(wglGetProcAddress(name));
				winapi_check(p);
				return p;
			}

			void _new_window(window_base &wnd) override {
				auto *cw = static_cast<window*>(&wnd);
				winapi_check(SetPixelFormat(cw->_dc, _pformat, &_pfd));
				bool initgl = false;
				if (!_rc) {
					winapi_check(_rc = wglCreateContext(cw->_dc));
					initgl = true;
				}
				winapi_check(wglMakeCurrent(cw->_dc, _rc));
				if (initgl) {
					_initialize_gl(_get_gl_func);
				}
			}
			std::function<void()> _get_begin_window_func(const window_base &wnd) override {
				const auto *cw = static_cast<const window*>(&wnd);
				return[this, dc = cw->_dc]() {
					winapi_check(wglMakeCurrent(dc, _rc));
				};
			}
			std::function<void()> _get_end_window_func(const window_base &wnd) override {
				const auto *cw = static_cast<const window*>(&wnd);
				return[dc = cw->_dc]() {
					winapi_check(SwapBuffers(dc));
				};
			}

			HGLRC _rc = nullptr;
			PIXELFORMATDESCRIPTOR _pfd;
			int _pformat;
		};
	}
}
