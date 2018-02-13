#pragma once

#include <cstring>

#include "misc.h"
#include <gl/GL.h>
#include <gl/glext.h>

#include "../../ui/element.h"
#include "window.h"

namespace codepad {
	namespace os {
		class software_renderer : public renderer_base {
		public:
			software_renderer() : renderer_base() {
				_txs.push_back(_tex_rec());
			}

			void begin(const window_base &wnd) override {
				const window *cwnd = static_cast<const window*>(&wnd);
				_wnd_rec *crec = &_wnds.find(cwnd)->second;
				_begin_render_target(_render_target_stackframe(
					crec->width, crec->height, crec->buffer, [this, cwnd, crec]() {
						DWORD *to = crec->bmp.arr;
						colord *from = _tarbuf;
						for (size_t i = crec->width * crec->height; i > 0; --i, ++from, ++to) {
							*to = _conv_to_dword(from->convert<unsigned char>());
						}
						winapi_check(BitBlt(
							cwnd->_dc, 0, 0, static_cast<int>(crec->width), static_cast<int>(crec->height),
							crec->dc, 0, 0, SRCCOPY
						));
					}
				));
				_clear_texture(_tarbuf, crec->width, crec->height);
			}
			void draw_character_custom(const char_texture &tex, rectd r, colord c) override {
				rectd t(0.0, 1.0, 0.0, 1.0);
				vec2d vs[6]{
					r.xmin_ymin(), r.xmax_ymin(), r.xmin_ymax(),
					r.xmax_ymin(), r.xmax_ymax(), r.xmin_ymax()
				}, uvs[6]{
					t.xmin_ymin(), t.xmax_ymin(), t.xmin_ymax(),
					t.xmax_ymin(), t.xmax_ymax(), t.xmin_ymax()
				};
				colord cs[6] = {c, c, c, c, c, c};
				_draw_triangles_impl(_get_id(tex), vs, uvs, cs, 6);
			}
			void draw_triangles(
				const texture &tid, const vec2d *poss, const vec2d *uvs, const colord *cs, size_t sz
			) override {
				_draw_triangles_impl(_get_id(tid), poss, uvs, cs, sz);
			}
			void draw_lines(const vec2d *vs, const colord *cs, size_t sz) override {
				for (size_t i = 0; i < sz; i += 2) {
					_draw_line(
						_matstk.back().transform(vs[i]),
						_matstk.back().transform(vs[i + 1]),
						cs[i]
					); // TODO get the color right
				}
			}
			void end() override {
				_end_render_target();
			}

			texture new_texture(size_t w, size_t h, const void *rgba) override {
				texture::id_t nid = _alloc_id();
				_txs[nid].set_rgba(w, h, static_cast<const unsigned char*>(rgba));
				return _make_texture(nid, w, h);
			}
			void delete_texture(texture &id) override {
				_delete_texture_impl(id);
			}

			char_texture new_character_texture(size_t w, size_t h, const void *gs) override {
				char_texture::id_t nid = _alloc_id();
				_txs[nid].set_rgba(w, h, static_cast<const unsigned char*>(gs));
				return _make_texture<false>(nid, w, h);
			}
			void delete_character_texture(char_texture &id) override {
				_delete_texture_impl(id);
			}
			void push_clip(recti r) override {
				_rtfstk.back().push_clip(r);
			}
			void pop_clip() override {
				_rtfstk.back().pop_clip();
			}

			framebuffer new_framebuffer(size_t w, size_t h) override {
				texture::id_t nid = _alloc_id();
				_txs[nid].set(w, h);
				return _make_framebuffer(nid, _make_texture(nid, w, h));
			}
			void delete_framebuffer(framebuffer &fb) override {
				_delete_texture_by_id(_get_id(fb.get_texture()));
				_erase_framebuffer(fb);
			}
			void begin_framebuffer(const framebuffer &fb) override {
				continue_framebuffer(fb);
				_clear_texture(_tarbuf, _rtfstk.back().width, _rtfstk.back().height);
			}
			void continue_framebuffer(const framebuffer &fb) override {
				_tex_rec &tr = _txs[_get_id(fb.get_texture())];
				_begin_render_target(_render_target_stackframe(tr.w, tr.h, tr.data));
			}

			void push_matrix(const matd3x3 &m) override {
				_matstk.push_back(m);
			}
			void push_matrix_mult(const matd3x3 &m) override {
				_matstk.push_back(_matstk.back() * m);
			}
			matd3x3 top_matrix() const override {
				return _matstk.back();
			}
			void pop_matrix() override {
				_matstk.pop_back();
			}

			void push_blend_function(blend_function f) override {
				_blendstk.push_back(f);
			}
			void pop_blend_function() override {
				_blendstk.pop_back();
			}
			blend_function top_blend_function() const override {
				return _blendstk.back();
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

			colord *_tarbuf = nullptr;

			inline static void _clear_texture(colord *arr, size_t w, size_t h) {
				for (size_t i = w * h; i > 0; --i, ++arr) {
					*arr = colord(0.0, 0.0, 0.0, 0.0);
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
					for (size_t i = w * h; i > 0; --i, ++target, ++gs) {
						target->r = target->g = target->b = 1.0;
						target->a = *gs / 255.0;
					}
				}
				void set_rgba(size_t ww, size_t hh, const unsigned char *rgba) {
					set(ww, hh);
					colord *target = data;
					for (size_t i = w * h; i > 0; --i, ++target, ++rgba) {
						target->r = *rgba / 255.0;
						target->g = *++rgba / 255.0;
						target->b = *++rgba / 255.0;
						target->a = *++rgba / 255.0;
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
				HGDIOBJ create_and_select(HDC dc, size_t w, size_t h) {
					BITMAPINFO info;
					std::memset(&info, 0, sizeof(info));
					info.bmiHeader.biSize = sizeof(info.bmiHeader);
					info.bmiHeader.biWidth = static_cast<LONG>(w);
					info.bmiHeader.biHeight = -static_cast<LONG>(h);
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
			};
			struct _wnd_rec {
				void create_buffer(HDC ndc, size_t w, size_t h) {
					width = w;
					height = h;
					winapi_check(dc = CreateCompatibleDC(ndc));
					if (width != 0 && height != 0) {
						old = bmp.create_and_select(dc, width, height);
					}
					buffer = new colord[width * height];
				}
				void resize_buffer(size_t w, size_t h) {
					width = w;
					height = h;
					if (width != 0 && height != 0) {
						_dev_bitmap newbmp;
						HGDIOBJ ov = newbmp.create_and_select(dc, width, height);
						if (old == nullptr) {
							old = ov;
						} else {
							bmp.dispose();
						}
						bmp = newbmp;
					}
					delete[] buffer;
					buffer = new colord[width * height];
				}
				void dispose_buffer() {
					if (old != nullptr) {
						bmp.unselect_and_dispose(dc, old);
					}
					winapi_check(DeleteDC(dc));
					delete[] buffer;
				}

				HGDIOBJ old = nullptr;
				HDC dc;
				_dev_bitmap bmp;
				colord *buffer = nullptr;
				size_t width, height;
			};
			struct _render_target_stackframe {
				_render_target_stackframe(size_t w, size_t h, colord *buf) :
					_render_target_stackframe(w, h, buf, nullptr) {
				}
				template <typename T> _render_target_stackframe(
					size_t w, size_t h, colord *buf, T &&e
				) :
					width(w), height(h), buffer(buf), end(std::forward<T>(e)),
					clip_stack({recti(0, static_cast<int>(width), 0, static_cast<int>(height))}) {
				}

				size_t width = 0, height = 0;
				colord *buffer = nullptr;
				std::function<void()> end;
				std::vector<recti> clip_stack;

				void push_clip(recti r) {
					r = recti::common_part(r, clip_stack.back());
					r.make_valid_max();
					clip_stack.push_back(r);
				}
				void pop_clip() {
					clip_stack.pop_back();
				}
			};

			std::vector<_tex_rec> _txs;
			std::vector<texture::id_t> _id_realloc;
			std::unordered_map<const window*, _wnd_rec> _wnds;
			std::vector<_render_target_stackframe> _rtfstk;
			std::vector<matd3x3> _matstk;
			std::vector<blend_function> _blendstk;

			void _begin_render_target(_render_target_stackframe sf) {
				_matstk.push_back(matd3x3());
				_matstk.back().set_identity();
				_blendstk.push_back(blend_function());
				_rtfstk.push_back(std::move(sf));
				_continue_last_render_target();
			}
			void _continue_last_render_target() {
				_tarbuf = _rtfstk.back().buffer;
			}
			void _end_render_target() {
				assert_true_usage(_rtfstk.back().clip_stack.size() == 1, "pushclip/popclip mismatch");
				if (_rtfstk.back().end) {
					_rtfstk.back().end();
				}
				_rtfstk.pop_back();
				_matstk.pop_back();
				_blendstk.pop_back();
				if (!_rtfstk.empty()) {
					_continue_last_render_target();
				} else {
					assert_true_usage(_matstk.size() == 0, "pushmatrix/popmatrix mismatch");
					assert_true_usage(_blendstk.size() == 0, "pushblendfunc/popblendfunc mismatch");
				}
			}

			texture::id_t _alloc_id() {
				texture::id_t nid;
				if (_id_realloc.empty()) {
					nid = _txs.size();
					_txs.push_back(_tex_rec());
				} else {
					nid = _id_realloc.back();
					_id_realloc.pop_back();
				}
				return nid;
			}
			void _delete_texture_by_id(texture::id_t id) {
				_txs[id].dispose();
				_id_realloc.push_back(id);
			}
			template <bool V> void _delete_texture_impl(texture_base<V> &tex) {
				_delete_texture_by_id(_get_id(tex));
				_erase_texture(tex);
			}
			void _draw_triangles_impl(
				texture::id_t tid, const vec2d *poss, const vec2d *uvs, const colord *cs, size_t sz
			) {
				const vec2d *cp = poss, *cuv = uvs;
				const colord *cc = cs;
				vec2d tmp[3];
				for (; sz > 2; sz -= 3, cp += 3, cuv += 3, cc += 3) {
					tmp[0] = _matstk.back().transform(cp[0]);
					tmp[1] = _matstk.back().transform(cp[1]);
					tmp[2] = _matstk.back().transform(cp[2]);
					_draw_triangle(tmp, cuv, cc, tid);
				}
			}

			inline static DWORD _conv_to_dword(colori cv) {
				return
					static_cast<DWORD>(cv.a) << 24 | static_cast<DWORD>(cv.r) << 16 |
					static_cast<DWORD>(cv.g) << 8 | cv.b;
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
				const vec2d *ps, const vec2d *uvs, const colord *cs, texture::id_t tex
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
			void _draw_triangle_half(
				double sx, double sy, double invk1, double invk2, double ymin, double ymax,
				const _tex_rec &tex, const _pq_params &params,
				const vec2d *uvs, const colord *cs
			) {
				sx += 0.5;
				sy -= 0.5;
				recti crgn = _rtfstk.back().clip_stack.back();
				size_t
					miny = static_cast<size_t>(std::max<double>(ymin + 0.5, crgn.ymin)),
					maxy = static_cast<size_t>(std::clamp<double>(ymax + 0.5, crgn.ymin, crgn.ymax)),
					scrw = _rtfstk.back().width;
				vec2d uvd = uvs[0] * params.xpi + uvs[1] * params.xqi + uvs[2] * params.xri;
				colord cd = cs[0] * params.xpi + cs[1] * params.xqi + cs[2] * params.xri;
				for (size_t y = miny; y < maxy; ++y) {
					double diff = static_cast<double>(y) - sy, left = diff * invk1 + sx, right = diff * invk2 + sx;
					size_t
						l = static_cast<size_t>(std::max<double>(left, crgn.xmin)),
						r = static_cast<size_t>(std::clamp<double>(right, crgn.xmin, crgn.xmax));
					colord *pixel = _tarbuf + y * scrw + l;
					double p, q, mpq;
					params.get_pq(l, y, p, q);
					mpq = 1.0 - p - q;
					vec2d uv = uvs[0] * p + uvs[1] * q + uvs[2] * mpq;
					colord cc = cs[0] * p + cs[1] * q + cs[2] * mpq;
					for (size_t cx = l; cx < r; ++cx, ++pixel, uv += uvd, cc += cd) {
						colord ncc = cc;
						if (tex.data) {
							ncc *= tex.sample(uv);
						}
						*pixel += (ncc - *pixel) * ncc.a; // TODO apply blend func
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
			void _draw_line(vec2d p1, vec2d p2, colord c) { // FIXME clipping is not working, rounding issues
				if (p1.x + p1.y > p2.x + p2.y) {
					std::swap(p1, p2);
				}
				vec2d diff = p2 - p1;
				recti crgn = _rtfstk.back().clip_stack.back();
				if (diff.x < 0.0 ? true : (diff.y < 0.0 ? false : (std::fabs(diff.y) > std::fabs(diff.x)))) {
					if (_clip_line_onedir(p1.x, p1.y, p2.x, p2.y, crgn.xmin + 0.5, crgn.xmax - 0.5)) {
						_draw_line_up(p1.y, p1.x, p2.y, diff.x / diff.y, c);
					}
				} else {
					if (_clip_line_onedir(p1.y, p1.x, p2.y, p2.x, crgn.ymin + 0.5, crgn.ymax - 0.5)) {
						_draw_line_right(p1.x, p1.y, p2.x, diff.y / diff.x, c);
					}
				}
			}
			void _draw_pixel_with_blend(size_t x, size_t y, colord c) {
				colord *pixel = _tarbuf + (y * _rtfstk.back().width + x);
				*pixel += (c - *pixel) * c.a;
			}
			void _draw_line_right(double fx, double fy, double tx, double k, colord c) {
				size_t t = static_cast<size_t>(std::clamp(tx, 0.0, static_cast<double>(_rtfstk.back().width - 1)));
				tx -= fx;
				fx -= 0.5;
				for (size_t cx = static_cast<size_t>(std::max(fx + 0.5, 0.0)); cx < t; ++cx) {
					_draw_pixel_with_blend(cx, static_cast<size_t>(fy + k * std::clamp(static_cast<double>(cx) - fx, 0.0, tx)), c);
				}
			}
			void _draw_line_up(double by, double bx, double ty, double invk, colord c) {
				size_t t = static_cast<size_t>(std::clamp(ty, 0.0, static_cast<double>(_rtfstk.back().height - 1)));
				ty -= by;
				by -= 0.5;
				for (size_t cy = static_cast<size_t>(std::max(by + 0.5, 0.0)); cy < t; ++cy) {
					_draw_pixel_with_blend(static_cast<size_t>(bx + invk * std::clamp(static_cast<double>(cy) - by, 0.0, ty)), cy, c);
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
				_dispose_gl_rsrc();
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
				return[this, dc = cw->_dc]() {
					winapi_check(wglMakeCurrent(dc, _rc));
				};
			}
			std::function<void()> _get_end_window_func(const window_base &wnd) override {
				const window *cw = static_cast<const window*>(&wnd);
				return[dc = cw->_dc]() {
					winapi_check(SwapBuffers(dc));
				};
			}

			template <typename T> inline static void _get_func(T &f, LPCSTR name) {
				winapi_check(f = reinterpret_cast<T>(wglGetProcAddress(name)));
			}
			void _init_gl() {
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

			HGLRC _rc = nullptr;
			PIXELFORMATDESCRIPTOR _pfd;
			int _pformat;
			_gl_funcs _gl;
		};
	}
}
