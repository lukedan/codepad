#pragma once

#include <cstring>

#include <gl/GL.h>
#include <gl/glext.h>

#include "../../ui/element.h"
#include "misc.h"
#include "window.h"

namespace codepad {
	namespace os {
		class software_renderer : public renderer_base {
		public:
			software_renderer() : renderer_base() {
				_txs.push_back(_tex_rec());
			}
			~software_renderer() override {
				_dispose_buffer();
			}

			void begin(const window_base &wnd) override {
				_cwnd = static_cast<const window*>(&wnd);
				_wcsz = _cwnd->get_actual_size().convert<int>();
				_crgn = recti(0, _wcsz.x, 0, _wcsz.y);
				_crec = &_wnds.find(_cwnd)->second;
				_check_buffer();
				for (size_t y = 0; y < _crec->bmp.h; ++y) {
					for (size_t x = 0; x < _crec->bmp.w; ++x) {
						_dbuf[(_crec->bmp.h - y - 1) * _crec->bmp.w + x] = colord(0.0, 0.0, 0.0, 0.0);
					}
				}
			}
			void draw_character(texture_id tex, vec2d pos, colord c) override {
				const _tex_rec &tr = _txs[tex];
				rectd rv = rectd::from_xywh(pos.x, pos.y, static_cast<double>(tr.w), static_cast<double>(tr.h));
				vec2d vxs[6] = {
					rv.xmin_ymin(), rv.xmax_ymin(), rv.xmin_ymax(),
					rv.xmax_ymin(), rv.xmax_ymax(), rv.xmin_ymax()
				}, uvs[6] = {
					vec2d(0.0, 0.0), vec2d(1.0, 0.0), vec2d(0.0, 1.0),
					vec2d(1.0, 0.0), vec2d(1.0, 1.0), vec2d(0.0, 1.0)
				};
				colord cs[6] = {c, c, c, c, c, c};
				draw_triangles(vxs, uvs, cs, 6, tex);
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
			void draw_lines(const vec2d *vs, const colord *cs, size_t sz) override {
				for (size_t i = 0; i < sz; i += 2) {
					_draw_line(vs[i], vs[i + 1], cs[i]); // TODO get the color right
				}
			}
			void end() override {
				DWORD *to = _crec->bmp.arr;
				colord *from = _dbuf;
				for (size_t i = _wcsz.x * _wcsz.y; i > 0; --i, ++from, ++to) {
					*to = _conv_to_dword(from->convert<unsigned char>());
				}
				winapi_check(BitBlt(
					_cwnd->_dc, _crgn.xmin, _crgn.ymin, _crgn.width(), _crgn.height(),
					_crec->dc, _crgn.xmin, _crgn.ymin, SRCCOPY
				));
			}

			texture_id new_character_texture(size_t w, size_t h, const void *gs) override {
				texture_id nid = _alloc_id();
				_txs[nid].set_grayscale(w, h, static_cast<const unsigned char*>(gs));
				return nid;
			}
			void delete_character_texture(texture_id id) override {
				_txs[id].dispose();
				_id_realloc.push_back(id);
			}
			void push_clip(recti r) override {
				if (_clpstk.size() > 0) {
					r = recti::common_part(r, _clpstk.back());
				}
				r.make_valid_max();
				_crgn = r;
				_clpstk.push_back(r);
			}
			void pop_clip() override {
				_clpstk.pop_back();
				if (_clpstk.size() > 0) {
					_crgn = _clpstk.back();
				} else {
					_crgn = _cwnd->get_layout().minimum_bounding_box<int>();
				}
			}
		protected:
			void _new_window(window_base &wnd) override {
				window *w = static_cast<window*>(&wnd);
				_wnd_rec wr;
				vec2i sz = w->get_actual_size().convert<int>();
				wr.create_buffer(w->_dc, sz.x, sz.y);
				auto it = _wnds.insert(std::make_pair(w, wr)).first;
				w->size_changed += [it](size_changed_info &info) {
					it->second.resize_buffer(static_cast<size_t>(info.new_size.x), static_cast<size_t>(info.new_size.y));
				};
			}
			void _delete_window(window_base &wnd) override {
				_wnds[static_cast<window*>(&wnd)].dispose_buffer();
			}

			colord *_dbuf = nullptr;
			vec2i _bufsz, _wcsz;
			void _dispose_buffer() {
				if (_dbuf) {
					std::free(_dbuf);
				}
			}
			void _check_buffer() {
				if (_wcsz.x > _bufsz.x || _wcsz.y > _bufsz.y) {
					_dispose_buffer();
					_bufsz.x = std::max(_bufsz.x, _wcsz.x);
					_bufsz.y = std::max(_bufsz.y, _wcsz.y);
					_dbuf = static_cast<colord*>(std::malloc(sizeof(colord) * _bufsz.x * _bufsz.y));
				}
			}

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
					//double xf = (uv.x - std::floor(uv.x)) * static_cast<double>(w), yf = (uv.y - std::floor(uv.y)) * static_cast<double>(h);
					//return fetch(static_cast<size_t>(xf), static_cast<size_t>(yf));

					double xf = uv.x * static_cast<double>(w) - 0.5, yf = uv.y * static_cast<double>(h) - 0.5;
					int x = static_cast<int>(std::floor(xf)), y = static_cast<int>(std::floor(yf)), x1 = x + 1, y1 = y + 1;
					xf -= x;
					yf -= y;
					clamp_coords(x, w);
					clamp_coords(x1, w);
					clamp_coords(y, h);
					clamp_coords(y1, h);
					colord v[4] = {
						fetch(static_cast<size_t>(x), static_cast<size_t>(y)),
						fetch(static_cast<size_t>(x1), static_cast<size_t>(y)),
						fetch(static_cast<size_t>(x), static_cast<size_t>(y1)),
						fetch(static_cast<size_t>(x1), static_cast<size_t>(y1))
					};
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
					info.bmiHeader.biWidth = static_cast<LONG>(w);
					info.bmiHeader.biHeight = static_cast<LONG>(h);
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
			std::unordered_map<const window*, _wnd_rec> _wnds;
			const window *_cwnd = nullptr;
			_wnd_rec *_crec = nullptr;
			std::vector<recti> _clpstk;
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
					double vx = vxc + static_cast<double>(x), vy = vyc + static_cast<double>(y);
					p = xpi * vx + m12c * vy;
					q = xqi * vx + m22c * vy;
				}
			};
			void _draw_triangle(
				const vec2d *ps, const vec2d *uvs, const colord *cs, texture_id tex
			) {
				const vec2d *yi[3]{ps, ps + 1, ps + 2};
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
				vec2d uvd = uvs[0] * params.xpi + uvs[1] * params.xqi + uvs[2] * params.xri;
				colord cd = cs[0] * params.xpi + cs[1] * params.xqi + cs[2] * params.xri;
				for (size_t y = miny; y < maxy; ++y) {
					double diff = static_cast<double>(y) - sy, left = diff * invk1 + sx, right = diff * invk2 + sx;
					size_t
						l = static_cast<size_t>(std::max<double>(left, _crgn.xmin)),
						r = static_cast<size_t>(clamp<double>(right, _crgn.xmin, _crgn.xmax));
					colord *pixel = _dbuf + (_crec->bmp.h - y - 1) * _crec->bmp.w + l;
					double p, q, mpq;
					params.get_pq(l, y, p, q);
					mpq = 1.0 - p - q;
					vec2d uv = uvs[0] * p + uvs[1] * q + uvs[2] * mpq;
					colord cc = cs[0] * p + cs[1] * q + cs[2] * mpq;
					for (size_t cx = l; cx < r; ++cx, ++pixel, uv += uvd, cc += cd) {
						if (tex.data) {
							colord ncc = cc * tex.sample(uv);
							*pixel += (ncc - *pixel) * ncc.a;
						} else {
							*pixel += (cc - *pixel) * cc.a;
						}
					}
				}
			}

			inline static void _clip_line_onedir_fixup(double &y, double &k, double &v, double &x) {
				y += k * (v - x);
				x = v;
			}
			inline static bool _clip_line_onedir(double &fx, double &fy, double &tx, double &ty, double xmin, double xmax) {
				if (fx < tx) {
					if (tx < xmin || fx > xmax) {
						return false;
					}
					double k = (ty - fy) / (tx - fx);
					if (fx < xmin) {
						_clip_line_onedir_fixup(fx, fy, xmin, k);
					}
					if (tx > xmax) {
						_clip_line_onedir_fixup(tx, ty, xmax, k);
					}
				} else {
					if (fx < xmin || tx > xmax) {
						return false;
					}
					double k = (ty - fy) / (tx - fx);
					if (fx > xmax) {
						_clip_line_onedir_fixup(fx, fy, xmax, k);
					}
					if (tx < xmin) {
						_clip_line_onedir_fixup(tx, ty, xmin, k);
					}
				}
				return true;
			}
			void _draw_line(vec2d p1, vec2d p2, colord c) {
				p1.y = _wcsz.y - p1.y;
				p2.y = _wcsz.y - p2.y;
				if (p1.x + p1.y > p2.x + p2.y) {
					std::swap(p1, p2);
				}
				vec2d diff = p2 - p1;
				if (diff.x < 0.0 ? true : (diff.y < 0.0 ? false : (std::fabs(diff.y) > std::fabs(diff.x)))) {
					if (_clip_line_onedir(p1.x, p1.y, p2.x, p2.y, _crgn.xmin + 0.5, _crgn.xmax - 0.5)) {
						_draw_line_up(p1.y, p1.x, p2.y, diff.x / diff.y, c);
					}
				} else {
					if (_clip_line_onedir(p1.y, p1.x, p2.y, p2.x, _crgn.ymin + 0.5, _crgn.ymax - 0.5)) {
						_draw_line_right(p1.x, p1.y, p2.x, diff.y / diff.x, c);
					}
				}
			}
			void _draw_pixel_with_blend(size_t x, size_t y, colord c) {
				colord *pixel = _dbuf + (y * _wcsz.x + x);
				*pixel += (c - *pixel) * c.a;
			}
			void _draw_line_right(double fx, double fy, double tx, double k, colord c) {
				size_t t = static_cast<size_t>(clamp(tx, 0.0, static_cast<double>(_wcsz.x - 1)));
				tx -= fx;
				fx -= 0.5;
				for (size_t cx = static_cast<size_t>(std::max(fx + 0.5, 0.0)); cx <= t; ++cx) {
					_draw_pixel_with_blend(cx, static_cast<size_t>(fy + k * clamp(static_cast<double>(cx) - fx, 0.0, tx)), c);
				}
			}
			void _draw_line_up(double by, double bx, double ty, double invk, colord c) {
				size_t t = static_cast<size_t>(clamp(ty, 0.0, static_cast<double>(_wcsz.y - 1)));
				ty -= by;
				by -= 0.5;
				for (size_t cy = static_cast<size_t>(std::max(by + 0.5, 0.0)); cy <= t; ++cy) {
					_draw_pixel_with_blend(static_cast<size_t>(bx + invk * clamp(static_cast<double>(cy) - by, 0.0, ty)), cy, c);
				}
			}
		};

		class opengl_renderer : public opengl_renderer_base {
		public:
			opengl_renderer() : opengl_renderer_base() {
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
				_atl.dispose();
				winapi_check(wglMakeCurrent(nullptr, nullptr));
				winapi_check(wglDeleteContext(_rc));
			}
		protected:
			const _gl_funcs &_get_gl_funcs() override {
				return _gl;
			}
			void _init_new_window(window_base &wnd) override {
				window *cw = static_cast<window*>(&wnd);
				winapi_check(SetPixelFormat(cw->_dc, _pformat, &_pfd));
				bool initgl = false;
				if (!_rc) {
					winapi_check(_rc = wglCreateContext(cw->_dc));
					initgl = true;
				}
				winapi_check(wglMakeCurrent(cw->_dc, _rc));
				if (initgl) {
					_init_gl();
				}
			}
			std::function<void()> _get_begin_window_func(const window_base &wnd) override {
				const window *cw = static_cast<const window*>(&wnd);
				return std::bind(static_cast<void(*)(BOOL)>(winapi_check), std::bind(wglMakeCurrent, cw->_dc, _rc));
			}
			std::function<void()> _get_end_window_func(const window_base &wnd) override {
				const window *cw = static_cast<const window*>(&wnd);
				return std::bind(static_cast<void(*)(BOOL)>(winapi_check), std::bind(SwapBuffers, cw->_dc));
			}

			template <typename T> inline static void _get_func(T &f, LPCSTR name) {
				winapi_check(f = reinterpret_cast<T>(wglGetProcAddress(name)));
			}
			void _init_gl() {
				_get_func(_gl.GenFramebuffers, "glGenFramebuffers");
				_get_func(_gl.BindFramebuffer, "glBindFramebuffer");
				_get_func(_gl.FramebufferTexture2D, "glFramebufferTexture2D");
				_get_func(_gl.CheckFramebufferStatus, "glCheckFramebufferStatus");
				_get_func(_gl.DeleteFramebuffers, "glDeleteFramebuffers");
				_get_func(_gl.GenerateMipmap, "glGenerateMipmap");
			}

			HGLRC _rc = nullptr;
			PIXELFORMATDESCRIPTOR _pfd;
			int _pformat;
			_gl_funcs _gl;
		};
	}
}
