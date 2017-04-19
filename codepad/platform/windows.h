#pragma once

#include <map>
#include <vector>

#include <Windows.h>
#include <windowsx.h>
#undef min
#undef max
#include <gl/GL.h>

#include "interface.h"
#include "../utilities/textconfig.h"

#include <iostream>

namespace codepad {
	namespace platform {
		template <typename T> inline void winapi_check(T v) {
			assert(v);
		}
		inline vec2i get_mouse_position() {
			POINT p;
			winapi_check(GetCursorPos(&p));
			return vec2i(p.x, p.y);
		}
		inline void set_mouse_position(vec2i p) {
			winapi_check(SetCursorPos(p.x, p.y));
		}

		class window : public window_base {
			friend LRESULT CALLBACK _wndproc(HWND, UINT, WPARAM, LPARAM);
			friend class software_renderer;
			friend class opengl_renderer;
		public:
			explicit window(const str_t&);
			window(const window&) = delete;
			window(window&&) = delete;
			window &operator=(const window&) = delete;
			window &operator=(window&&) = delete;
			~window() {
				DestroyWindow(_hwnd);
				UnregisterClass(reinterpret_cast<LPCTSTR>(_wndatom), GetModuleHandle(nullptr));
			}

			bool idle() override {
				MSG msg;
				if (PeekMessage(&msg, _hwnd, 0, 0, PM_REMOVE)) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
					return true;
				}
				return false;
			}

			void set_caption(const str_t &cap) override {
				winapi_check(SetWindowText(_hwnd, cap.c_str()));
			}

			vec2i screen_to_client(vec2i v) const override {
				POINT p;
				p.x = v.x;
				p.y = v.y;
				winapi_check(ScreenToClient(_hwnd, &p));
				return vec2i(p.x, p.y);
			}
			vec2i client_to_screen(vec2i v) const override {
				POINT p;
				p.x = v.x;
				p.y = v.y;
				winapi_check(ClientToScreen(_hwnd, &p));
				return vec2i(p.x, p.y);
			}
		protected:
			HWND _hwnd;
			ATOM _wndatom;
			HDC _dc;

			void _recalc_layout() {
				RECT cln;
				winapi_check(GetClientRect(_hwnd, &cln));
				_layout = rectd::from_xywh(0.0, 0.0, cln.right, cln.bottom);
			}
		};

		class software_renderer : public renderer_base {
		public:
			bool support_partial_redraw() const override {
				return true;
			}

			void new_window(window_base &wnd) override {
				window *w = static_cast<window*>(&wnd);
				_wnd_rec wr;
				wr.create_buffer(w->_dc, static_cast<int>(w->_layout.width()), static_cast<int>(w->_layout.height()));
				auto it = _wnds.insert(std::make_pair(w, wr)).first;
				w->size_changed += [it](size_changed_info &info) {
					it->second.resize_buffer(info.new_size.x, info.new_size.y);
				};
			}
			void delete_window(window_base &wnd) override {
				_wnds[static_cast<window*>(&wnd)].dispose_buffer();
			}

			void begin(window_base &wnd, recti rgn) override {
				_cwnd = static_cast<window*>(&wnd);
				_crgn = rgn;
				_crec = &_wnds[_cwnd];
				for (int y = rgn.ymin; y < rgn.ymax; ++y) {
					for (int x = rgn.xmin; x < rgn.xmax; ++x) {
						_crec->bmp.arr[(_crec->bmp.h - y - 1) * _crec->bmp.w + x] = 0;
					}
				}
			}
			void draw_triangles(
				const vec2d *poss, const vec2d *uvs, const colord *cs,
				size_t sz, texture_id tid
			) override {
				const vec2d *cp = poss, *cuv = uvs;
				const colord *cc = cs;
				for (; sz > 2; sz -= 3, cp += 3, cuv += 3, cc += 3) {
					_draw_triangle(cp, cuv, cc, tid);
				}
			}
			void end() override {
				winapi_check(BitBlt(
					_cwnd->_dc, _crgn.xmin, _crgn.ymin, _crgn.width(), _crgn.height(),
					_crec->dc, _crgn.xmin, _crgn.ymin, SRCCOPY
				));
			}

			texture_id new_texture_grayscale(size_t w, size_t h, const void *gs) override {
				texture_id nid = _alloc_id();
				_txs[nid].set_grayscale(w, h, static_cast<const unsigned char*>(gs));
				return nid;
			}
			void delete_texture(texture_id id) {
				_txs[id].dispose();
				_id_realloc.push_back(id);
			}
		protected:
			struct _tex_rec {
				void set(size_t ww, size_t hh) {
					w = ww;
					h = hh;
					data = static_cast<colord*>(std::malloc(w * h * sizeof(colord)));
				}
				void set_grayscale(size_t ww, size_t hh, const unsigned char *gs) {
					set(ww, hh);
					colord *target = data;
					const unsigned char *source = gs;
					for (size_t i = w * h; i > 0; --i, ++target, ++source) {
						target->r = target->g = target->b = 1.0;
						target->a = *source / 255.0;
					}
				}
				void dispose() {
					std::free(data);
				}

				inline static void clamp_coords(int &v, size_t max) {
					v %= static_cast<int>(max);
					if (v < 0) {
						v += static_cast<int>(max);
					}
				}
				colord fetch(size_t x, size_t y) const {
					return data[y * w + x];
				}
				colord sample(vec2d uv) const {
					double xf = uv.x * w - 0.5, yf = uv.y * h - 0.5;
					int x = static_cast<int>(std::floor(xf)), y = static_cast<int>(std::floor(yf)), x1 = x + 1, y1 = y + 1;
					xf -= x;
					yf -= y;
					clamp_coords(x, w);
					clamp_coords(x1, w);
					clamp_coords(y, h);
					clamp_coords(y1, h);
					colord v[4] = { fetch(x, y), fetch(x1, y), fetch(x, y1), fetch(x1, y1) };
					return v[0] + (v[1] - v[0]) * (1.0 - yf) * xf + (v[2] - v[0] + (v[3] - v[2]) * xf) * yf;
				}

				size_t w, h;
				colord *data = nullptr;
			};
			struct _dev_bitmap {
				HGDIOBJ create_and_select(HDC dc, size_t ww, size_t hh) {
					w = ww;
					h = hh;
					BITMAPINFO info;
					std::memset(&info, 0, sizeof(info));
					info.bmiHeader.biSize = sizeof(info.bmiHeader);
					info.bmiHeader.biWidth = w;
					info.bmiHeader.biHeight = h;
					info.bmiHeader.biPlanes = 1;
					info.bmiHeader.biBitCount = 32;
					info.bmiHeader.biCompression = BI_RGB;
					winapi_check(handle = CreateDIBSection(dc, &info, DIB_RGB_COLORS, reinterpret_cast<void**>(&arr), nullptr, 0));

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
				size_t w, h;
			};
			struct _wnd_rec {
				void create_buffer(HDC ndc, size_t w, size_t h) {
					winapi_check(dc = CreateCompatibleDC(ndc));
					old = bmp.create_and_select(dc, w, h);
				}
				void resize_buffer(size_t w, size_t h) {
					_dev_bitmap newbmp;
					newbmp.create_and_select(dc, w, h);
					bmp.dispose();
					bmp = newbmp;
				}
				void dispose_buffer() {
					bmp.unselect_and_dispose(dc, old);
					winapi_check(DeleteDC(dc));
				}

				HGDIOBJ old;
				HDC dc;
				_dev_bitmap bmp;
			};

			std::vector<_tex_rec> _txs;
			std::vector<texture_id> _id_realloc;
			std::map<window*, _wnd_rec> _wnds;
			window *_cwnd = nullptr;
			_wnd_rec *_crec = nullptr;
			recti _crgn;

			texture_id _alloc_id() {
				texture_id nid;
				if (_id_realloc.empty()) {
					nid = _txs.size();
					_txs.push_back(_tex_rec());
				} else {
					nid = _id_realloc.back();
					_id_realloc.pop_back();
				}
				return nid;
			}

			struct _pq_params {
				_pq_params(vec2d p1, vec2d p2, vec2d p3) {
					vec2d v12 = p2 - p1, v23 = p3 - p2, v31 = p1 - p3;
					double div_c = 1.0 / (p1.y * v23.x + p2.y * v31.x + p3.y * v12.x);
					xpi = -v23.y * div_c;
					m12c = v23.x * div_c;
					xqi = -v31.y * div_c;
					m22c = v31.x * div_c;
					vxc = 0.5 - p3.x;
					vyc = 0.5 - p3.y;
					xri = -xpi - xqi;
				}

				double xpi, m12c, xqi, m22c, vxc, vyc, xri;
				void get_pq(size_t x, size_t y, double &p, double &q) const {
					double vx = vxc + x, vy = vyc + y;
					p = xpi * vx + m12c * vy;
					q = xqi * vx + m22c * vy;
				}
			};
			void _draw_triangle(
				const vec2d *ps, const vec2d *uvs, const colord *cs, texture_id tex
			) {
				const vec2d *yi[3]{ ps, ps + 1, ps + 2 };
				if (yi[0]->y > yi[1]->y) {
					std::swap(yi[0], yi[1]);
				}
				if (yi[1]->y > yi[2]->y) {
					std::swap(yi[1], yi[2]);
				}
				if (yi[0]->y > yi[1]->y) {
					std::swap(yi[0], yi[1]);
				}
				const _tex_rec &tr = _txs[tex];
				double
					invk_01 = (yi[1]->x - yi[0]->x) / (yi[1]->y - yi[0]->y),
					invk_02 = (yi[2]->x - yi[0]->x) / (yi[2]->y - yi[0]->y),
					invk_12 = (yi[2]->x - yi[1]->x) / (yi[2]->y - yi[1]->y);
				_pq_params pq(ps[0], ps[1], ps[2]);
				if (invk_01 > invk_02) {
					_draw_triangle_half(yi[0]->x, yi[0]->y, invk_02, invk_01, yi[0]->y, yi[1]->y, tr, pq, uvs, cs);
				} else {
					_draw_triangle_half(yi[0]->x, yi[0]->y, invk_01, invk_02, yi[0]->y, yi[1]->y, tr, pq, uvs, cs);
				}
				if (invk_02 > invk_12) {
					_draw_triangle_half(yi[2]->x, yi[2]->y, invk_02, invk_12, yi[1]->y, yi[2]->y, tr, pq, uvs, cs);
				} else {
					_draw_triangle_half(yi[2]->x, yi[2]->y, invk_12, invk_02, yi[1]->y, yi[2]->y, tr, pq, uvs, cs);
				}
			}
			inline static DWORD _conv_to_dword(colori cv) {
				return
					static_cast<DWORD>(cv.a) << 24 | static_cast<DWORD>(cv.r) << 16 |
					static_cast<DWORD>(cv.g) << 8 | cv.b;
			}
			inline static colori _conv_to_uchar(DWORD dv) {
				return colori((dv >> 16) & 0xFF, (dv >> 8) & 0xFF, dv & 0xFF, dv >> 24);
			}
			void _draw_triangle_half(
				double sx, double sy, double invk1, double invk2, double ymin, double ymax,
				const _tex_rec &tex, const _pq_params &params,
				const vec2d *uvs, const colord *cs
			) {
				sx += 0.5;
				sy -= 0.5;
				size_t
					miny = static_cast<size_t>(std::max<double>(ymin + 0.5, _crgn.ymin)),
					maxy = static_cast<size_t>(clamp<double>(ymax + 0.5, _crgn.ymin, _crgn.ymax));
				for (size_t y = miny; y < maxy; ++y) {
					double diff = y - sy, left = diff * invk1 + sx, right = diff * invk2 + sx;
					size_t
						l = static_cast<size_t>(std::max<double>(left, _crgn.xmin)),
						r = static_cast<size_t>(clamp<double>(right, _crgn.xmin, _crgn.xmax));
					DWORD *pixel = _crec->bmp.arr + (_crec->bmp.h - y - 1) * _crec->bmp.w + l;
					double p, q, mpq;
					params.get_pq(l, y, p, q);
					mpq = 1.0 - p - q;
					for (size_t cx = l; cx < r; ++cx, ++pixel, p += params.xpi, q += params.xqi, mpq += params.xri) {
						vec2d uv = uvs[0] * p + uvs[1] * q + uvs[2] * mpq;
						colord cc = tex.sample(uv) * (cs[0] * p + cs[1] * q + cs[2] * mpq);
						*pixel = _conv_to_dword((
							cc * cc.a + _conv_to_uchar(*pixel).convert<double>() * (1.0 - cc.a)
							).convert<unsigned char>());
					}
				}
			}
		};
		class opengl_renderer : public renderer_base {
		public:
			bool support_partial_redraw() const override {
				return false;
			}
		protected:
			HGLRC _rc;
		};
	}
}