#pragma once

#include <unordered_map>
#include <vector>
#include <cstring>

#include <Windows.h>
#include <windowsx.h>
#undef min
#undef max
#include <gl/GL.h>
#include <gl/glext.h>

#include "window.h"
#include "renderer.h"
#include "input.h"
#include "../utilities/textconfig.h"
#include "../utilities/textproc.h"

namespace codepad {
	namespace os {
		template <typename T> inline void winapi_check(T v) {
			if (!v) {
				logger::get().log_error(CP_HERE, "WinAPI error code ", GetLastError());
				assert_true_sys(false, "WinAPI error");
			}
		}

		namespace input {
			extern const int _key_id_mapping[total_num_keys];
			inline bool is_key_down(key k) {
				return (GetAsyncKeyState(_key_id_mapping[static_cast<int>(k)]) & ~1) != 0;
			}
			inline bool is_mouse_button_swapped() {
				return GetSystemMetrics(SM_SWAPBUTTON) != 0;
			}

			inline vec2i get_mouse_position() {
				POINT p;
				winapi_check(GetCursorPos(&p));
				return vec2i(p.x, p.y);
			}
			inline void set_mouse_position(vec2i p) {
				winapi_check(SetCursorPos(p.x, p.y));
			}
		}

		class window : public window_base {
			friend LRESULT CALLBACK _wndproc(HWND, UINT, WPARAM, LPARAM);
			friend class software_renderer;
			friend class opengl_renderer;
			friend class ui::element;
		public:
			void set_caption(const str_t &cap) override {
				auto u16str = utf32_to_utf16(cap);
				winapi_check(SetWindowText(_hwnd, reinterpret_cast<LPCWSTR>(u16str.c_str())));
			}
			vec2i get_position() const override {
				POINT tl;
				winapi_check(ClientToScreen(_hwnd, &tl));
				return vec2i(tl.x, tl.y);
			}
			void set_position(vec2i pos) override {
				RECT r;
				POINT tl;
				tl.x = tl.y = 0;
				winapi_check(GetWindowRect(_hwnd, &r));
				winapi_check(ClientToScreen(_hwnd, &tl));
				tl.x -= r.left;
				tl.y -= r.top;
				winapi_check(SetWindowPos(_hwnd, nullptr, pos.x - tl.x, pos.y - tl.y, 0, 0, SWP_NOSIZE));
			}
			vec2i get_client_size() const override {
				RECT r;
				winapi_check(GetClientRect(_hwnd, &r));
				return vec2i(r.right, r.bottom);
			}
			void set_client_size(vec2i sz) override {
				RECT wndrgn, cln;
				winapi_check(GetWindowRect(_hwnd, &wndrgn));
				winapi_check(GetClientRect(_hwnd, &cln));
				winapi_check(SetWindowPos(
					_hwnd, nullptr, 0, 0,
					wndrgn.right - wndrgn.left - cln.right + sz.x,
					wndrgn.bottom - wndrgn.top - cln.bottom + sz.y,
					SWP_NOMOVE
				));
			}

			void activate() override {
				winapi_check(SetWindowPos(_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE));
			}
			void prompt_ready() override {
				FLASHWINFO fwi;
				std::memset(&fwi, 0, sizeof(fwi));
				fwi.cbSize = sizeof(fwi);
				fwi.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
				fwi.dwTimeout = 0;
				fwi.hwnd = _hwnd;
				fwi.uCount = 0;
				FlashWindowEx(&fwi);
			}

			void set_display_maximize_button(bool disp) override {
				_set_window_style_bit(disp, WS_MAXIMIZE);
			}
			void set_display_minimize_button(bool disp) override {
				_set_window_style_bit(disp, WS_MINIMIZE);
			}
			void set_display_caption_bar(bool disp) override {
				_set_window_style_bit(disp, WS_CAPTION ^ WS_BORDER);
			}
			void set_display_border(bool disp) override {
				_set_window_style_bit(disp, WS_BORDER);
			}
			void set_sizable(bool size) override {
				_set_window_style_bit(size, WS_THICKFRAME);
			}

			bool hit_test_full_client(vec2i v) const override {
				RECT r;
				winapi_check(GetWindowRect(_hwnd, &r));
				return r.left <= v.x && r.right > v.x && r.top <= v.y && r.bottom > v.y;
			}

			ui::cursor get_current_display_cursor() const override {
				if (_capture) {
					return _capture->get_current_display_cursor();
				}
				return window_base::get_current_display_cursor();
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

			void set_mouse_capture(ui::element &elem) override {
				window_base::set_mouse_capture(elem);
				SetCapture(_hwnd);
			}
			void release_mouse_capture() {
				window_base::release_mouse_capture();
				winapi_check(ReleaseCapture());
			}
		protected:
			window() : window(U"Codepad") {
			}
			explicit window(const str_t&);

			HWND _hwnd;
			HDC _dc;

			struct _wndclass {
				_wndclass();
				~_wndclass() {
					UnregisterClass(reinterpret_cast<LPCTSTR>(atom), GetModuleHandle(nullptr));
				}

				ATOM atom;
			};
			static _wndclass _class;

			void _set_window_style_bit(bool v, LONG bit) {
				LONG old = GetWindowLong(_hwnd, GWL_STYLE);
				SetWindowLong(_hwnd, GWL_STYLE, v ? old | bit : old & ~bit);
				winapi_check(SetWindowPos(
					_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED
				));
			}

			void _setup_mouse_tracking() {
				TRACKMOUSEEVENT tme;
				std::memset(&tme, 0, sizeof(tme));
				tme.cbSize = sizeof(tme);
				tme.dwFlags = TME_HOVER | TME_LEAVE;
				tme.dwHoverTime = HOVER_DEFAULT;
				tme.hwndTrack = _hwnd;
				winapi_check(TrackMouseEvent(&tme));
			}

			void _on_resize() {
				RECT cln;
				winapi_check(GetClientRect(_hwnd, &cln));
				_layout = rectd::from_xywh(0.0, 0.0, cln.right, cln.bottom);
				size_changed_info p(vec2i(cln.right, cln.bottom));
				if (p.new_size.x > 0 && p.new_size.y > 0) {
					_on_size_changed(p);
					ui::manager::get().update_layout_and_visual();
				}
			}

			bool _idle();
			void _on_update() override {
				_idle();
				_update_drag();
				ui::manager::get().schedule_update(*this);
			}

			void _initialize() override {
				window_base::_initialize();
				SetWindowLongPtr(_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
				ShowWindow(_hwnd, SW_SHOW);
				ui::manager::get().schedule_update(*this);
			}
			void _dispose() override {
				winapi_check(DestroyWindow(_hwnd));
				window_base::_dispose();
			}
		};

		class software_renderer : public renderer_base {
		public:
			software_renderer() : renderer_base() {
				_txs.push_back(_tex_rec());
			}
			~software_renderer() {
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

		class opengl_renderer : public renderer_base {
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
			~opengl_renderer() {
				_atl.dispose();
				winapi_check(wglMakeCurrent(nullptr, nullptr));
				winapi_check(wglDeleteContext(_rc));
			}

			void begin(const window_base &wnd) override {
				const window *cw = static_cast<const window*>(&wnd);
				_curdc = cw->_dc;
				_invert_y = true;
				vec2i sz = cw->get_actual_size().convert<int>();
				winapi_check(wglMakeCurrent(_curdc, _rc));
				_begin_viewport_size(sz.x, sz.y);
				glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
				glClear(GL_COLOR_BUFFER_BIT);
				_gl_verify();
			}
			void push_clip(recti r) override {
				_flush_text_buffer();
				if (_invert_y) {
					int ymin = r.ymin;
					r.ymin = _curheight - r.ymax;
					r.ymax = _curheight - ymin;
				}
				if (_clpstk.size() > 0) {
					r = recti::common_part(r, _clpstk.back());
				}
				r.make_valid_max();
				_clpstk.push_back(r);
				glEnable(GL_SCISSOR_TEST);
				glScissor(r.xmin, r.ymin, r.width(), r.height());
			}
			void pop_clip() override {
				_flush_text_buffer();
				_clpstk.pop_back();
				if (_clpstk.size() > 0) {
					const recti &r = _clpstk.back();
					glScissor(r.xmin, r.ymin, r.width(), r.height());
				} else {
					glDisable(GL_SCISSOR_TEST);
				}
			}
			void draw_character(texture_id id, vec2d pos, colord color) override {
				const _text_atlas::char_data &cd = _atl.get_char_data(id);
				if (_lstpg != cd.page) {
					_flush_text_buffer();
					_lstpg = cd.page;
				}
				_textbuf.append(
					pos, pos + vec2d(static_cast<double>(cd.w), static_cast<double>(cd.h)),
					cd.layout, color, _matstk.size() > 0 ? &_matstk.back() : nullptr
				);
			}
			void draw_triangles(const vec2d *ps, const vec2d *us, const colord *cs, size_t n, texture_id t) override {
				_flush_text_buffer();
				glVertexPointer(2, GL_DOUBLE, sizeof(vec2d), ps);
				glTexCoordPointer(2, GL_DOUBLE, sizeof(vec2d), us);
				glColorPointer(4, GL_DOUBLE, sizeof(colord), cs);
				glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(t));
				glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(n));
			}
			void draw_lines(const vec2d *ps, const colord *cs, size_t n) override {
				_flush_text_buffer();
				glVertexPointer(2, GL_DOUBLE, sizeof(vec2d), ps);
				glColorPointer(4, GL_DOUBLE, sizeof(colord), cs);
				glBindTexture(GL_TEXTURE_2D, 0);
				glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(n));
			}
			void end() override {
				_flush_text_buffer();
				winapi_check(SwapBuffers(_curdc));
				_gl_verify();
			}

			texture_id new_character_texture(size_t w, size_t h, const void *data) override {
				assert_true_usage(_rc, "texture allocation requested before establishing any context");
				return _atl.new_char(w, h, data);
			}
			void delete_character_texture(texture_id id) override {
				_atl.delete_char(id);
			}

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
				_gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tid, 0);
				assert_true_sys(
					_gl.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
					"OpenGL error: unable to create framebuffer"
				);
				return framebuffer(fbid, tid, w, h);
			}
			void delete_framebuffer(framebuffer &fb) override {
				GLuint id = static_cast<GLuint>(fb._id), tid = static_cast<GLuint>(fb._tid);
				_gl.DeleteFramebuffers(1, &id);
				glDeleteTextures(1, &tid);
				fb._tid = 0;
			}
			void begin_framebuffer(const framebuffer &fb) override {
				continue_framebuffer(fb);
				glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
				glClear(GL_COLOR_BUFFER_BIT);
			}
			void continue_framebuffer(const framebuffer &fb) override {
				assert_true_usage(fb.has_content(), "cannot draw to an empty frame buffer");
				_invert_y = false;
				_gl.BindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(fb._id));
				_begin_viewport_size(fb._w, fb._h);
				_gl_verify();
			}
			void end_framebuffer() override {
				_flush_text_buffer();
				_gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
				_gl_verify();
			}

			void push_matrix(const matd3x3 &m) override {
				_matstk.push_back(m);
				glMatrixMode(GL_MODELVIEW);
				glPushMatrix();
				double d[16];
				_set_gl_matrix(m, d);
				glLoadMatrixd(d);
			}
			void push_matrix_mult(const matd3x3 &m) override {
				glMatrixMode(GL_MODELVIEW);
				glPushMatrix();
				double d[16];
				_set_gl_matrix(m, d);
				glMultMatrixd(d);
			}
			matd3x3 top_matrix() const override {
				double d[16];
				glGetDoublev(GL_MODELVIEW_MATRIX, d);
				return _get_gl_matrix(d);
			}
			void pop_matrix() override {
				_matstk.pop_back();
				glMatrixMode(GL_MODELVIEW);
				glPopMatrix();
			}
		protected:
			void _new_window(window_base &wnd) override {
				window *cw = static_cast<window*>(&wnd);
				winapi_check(SetPixelFormat(cw->_dc, _pformat, &_pfd));
				bool initgl = false;
				if (!_rc) {
					winapi_check(_rc = wglCreateContext(cw->_dc));
					initgl = true;
				}
				winapi_check(wglMakeCurrent(cw->_dc, _rc));
				if (initgl) {
					_gl.init();
				}
				glEnable(GL_TEXTURE_2D);
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glEnableClientState(GL_VERTEX_ARRAY);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				glEnableClientState(GL_COLOR_ARRAY);
			}
			void _delete_window(window_base&) override {
			}

			inline static void _set_default_texture_params() {
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}
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

			struct _text_atlas {
				void dispose() {
					for (auto i = _ps.begin(); i != _ps.end(); ++i) {
						i->dispose();
					}
				}

				struct page {
					void create(size_t w, size_t h) {
						width = w;
						height = h;
						size_t sz = width * height * 4;
						data = static_cast<unsigned char*>(std::malloc(sz));
						std::memset(data, 0, sz);
						glGenTextures(1, &tex_id);
						glBindTexture(GL_TEXTURE_2D, tex_id);
						_set_default_texture_params();
					}
					void flush() {
						glBindTexture(GL_TEXTURE_2D, tex_id);
						glTexImage2D(
							GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
							0, GL_RGBA, GL_UNSIGNED_BYTE, data
						);
					}
					void dispose() {
						std::free(data);
						glDeleteTextures(1, &tex_id);
					}

					size_t width, height;
					unsigned char *data;
					GLuint tex_id;
				};
				struct char_data {
					size_t w, h, page;
					rectd layout;
				};

				const char_data &get_char_data(size_t id) const {
					return _cd_slots[id];
				}
				const page &get_page(size_t page) {
					if (_lpdirty && page + 1 == _ps.size()) {
						_ps.back().flush();
					}
					return _ps[page];
				}

				size_t page_width = 600, page_height = 300, border = 1;
			protected:
				size_t _cx = 0, _cy = 0, _my = 0;
				std::vector<page> _ps;
				std::vector<char_data> _cd_slots;
				std::vector<texture_id> _cd_alloc;
				bool _lpdirty = false;

				void _new_page() {
					page np;
					np.create(page_width, page_height);
					_ps.push_back(np);
				}
				texture_id _alloc_id() {
					texture_id res;
					if (_cd_alloc.size() > 0) {
						res = _cd_alloc.back();
						_cd_alloc.pop_back();
					} else {
						res = _cd_slots.size();
						_cd_slots.push_back(char_data());
					}
					return res;
				}
			public:
				texture_id new_char(size_t w, size_t h, const void *data) {
					if (_ps.size() == 0) {
						_new_page();
					}
					texture_id id = _alloc_id();
					char_data &cd = _cd_slots[id];
					cd.w = w;
					cd.h = h;
					if (w == 0 || h == 0) {
						cd.layout = rectd(0.0, 0.0, 0.0, 0.0);
						cd.page = _ps.size() - 1;
					} else {
						page curp = _ps.back();
						if (_cx + w + border > curp.width) { // next row
							_cx = 0;
							_cy += _my;
							_my = 0;
						}
						size_t t, l;
						if (_cy + h + border > curp.height) { // next page
							if (_lpdirty) {
								curp.flush();
							}
							_new_page();
							curp = _ps.back();
							l = t = border;
							_cy = 0;
							_my = h + border;
						} else {
							l = _cx + border;
							t = _cy + border;
							_my = std::max(_my, h + border);
						}
						const unsigned char *src = static_cast<const unsigned char*>(data);
						for (size_t y = 0; y < h; ++y) {
							unsigned char *cur = curp.data + ((y + t) * curp.width + l) * 4;
							for (size_t x = 0; x < w; ++x) {
								*(cur++) = 255;
								*(cur++) = 255;
								*(cur++) = 255;
								*(cur++) = *(src++);
							}
						}
						_cx = l + w;
						cd.layout = rectd(
							static_cast<double>(l) / static_cast<double>(curp.width),
							static_cast<double>(l + w) / static_cast<double>(curp.width),
							static_cast<double>(t) / static_cast<double>(curp.height),
							static_cast<double>(t + h) / static_cast<double>(curp.height)
						);
						cd.page = _ps.size() - 1;
						_lpdirty = true;
					}
					return id;
				}
				void delete_char(texture_id id) {
					_cd_alloc.push_back(id);
				}
			};
			struct _text_buffer {
				struct vertex {
					vertex(vec2d p, vec2d u, colord color) : v(p), uv(u), c(color) {
					}
					vec2d v, uv;
					colord c;
				};
				std::vector<vertex> vxs;
				std::vector<unsigned int> ids;
				size_t vertcount = 0, count = 0;

				void append(vec2d tl, vec2d br, rectd layout, colord c, const matd3x3 *m) {
					vec2d v[4]{tl, vec2d(br.x, tl.y), vec2d(tl.x, br.y), br};
					if (m) {
						for (size_t i = 0; i < 4; ++i) {
							v[i] = m->transform(v[i]);
						}
					}
					if (vertcount == vxs.size()) {
						unsigned int id = static_cast<unsigned int>(vxs.size());
						ids.push_back(id);
						ids.push_back(id + 1);
						ids.push_back(id + 2);
						ids.push_back(id + 1);
						ids.push_back(id + 3);
						ids.push_back(id + 2);
						vxs.push_back(vertex(v[0], layout.xmin_ymin(), c));
						vxs.push_back(vertex(v[1], layout.xmax_ymin(), c));
						vxs.push_back(vertex(v[2], layout.xmin_ymax(), c));
						vxs.push_back(vertex(v[3], layout.xmax_ymax(), c));
						vertcount = vxs.size();
						count = ids.size();
					} else {
						vxs[vertcount++] = vertex(v[0], layout.xmin_ymin(), c);
						vxs[vertcount++] = vertex(v[1], layout.xmax_ymin(), c);
						vxs[vertcount++] = vertex(v[2], layout.xmin_ymax(), c);
						vxs[vertcount++] = vertex(v[3], layout.xmax_ymax(), c);
						count += 6;
					}
				}
				void flush_nocheck(GLuint tex) {
					glMatrixMode(GL_MODELVIEW);
					glPushMatrix();
					glLoadIdentity();
					glVertexPointer(2, GL_DOUBLE, sizeof(vertex), &vxs[0].v);
					glTexCoordPointer(2, GL_DOUBLE, sizeof(vertex), &vxs[0].uv);
					glColorPointer(4, GL_DOUBLE, sizeof(vertex), &vxs[0].c);
					glBindTexture(GL_TEXTURE_2D, tex);
					glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(count), GL_UNSIGNED_INT, ids.data());
					glPopMatrix();
					count = vertcount = 0;
				}
			};

			struct _wgl_funcs {
			public:
				void init() {
					_get_func(GenFramebuffers, "glGenFramebuffers");
					_get_func(BindFramebuffer, "glBindFramebuffer");
					_get_func(FramebufferTexture2D, "glFramebufferTexture2D");
					_get_func(CheckFramebufferStatus, "glCheckFramebufferStatus");
					_get_func(DeleteFramebuffers, "glDeleteFramebuffers");
				}
				PFNGLGENFRAMEBUFFERSPROC GenFramebuffers;
				PFNGLBINDFRAMEBUFFERPROC BindFramebuffer;
				PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D;
				PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus;
				PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers;
			protected:
				template <typename T> inline static void _get_func(T &f, LPCSTR name) {
					winapi_check(f = reinterpret_cast<T>(wglGetProcAddress(name)));
				}
			};

			HGLRC _rc = nullptr;
			PIXELFORMATDESCRIPTOR _pfd;
			HDC _curdc;
			_wgl_funcs _gl;
			int _pformat;

			int _curheight;
			std::vector<recti> _clpstk;
			std::vector<matd3x3> _matstk;
			bool _invert_y = true;

			_text_atlas _atl;
			_text_buffer _textbuf;
			size_t _lstpg;

			void _begin_viewport_size(size_t w, size_t h) {
				_curheight = static_cast<int>(h);
				glViewport(0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h));
				glMatrixMode(GL_PROJECTION);
				glLoadIdentity();
				if (_invert_y) {
					glOrtho(0.0, static_cast<GLdouble>(w), static_cast<GLdouble>(h), 0.0, 0.0, -1.0);
				} else {
					glOrtho(0.0, static_cast<GLdouble>(w), 0.0, static_cast<GLdouble>(h), 0.0, -1.0);
				}
			}
			void _flush_text_buffer() {
				if (_textbuf.vertcount > 0) {
					_textbuf.flush_nocheck(_atl.get_page(_lstpg).tex_id);
				}
			}
			void _gl_verify() {
				GLenum errorcode = glGetError();
				if (errorcode != GL_NO_ERROR) {
					logger::get().log_error(CP_HERE, "OpenGL error code ", errorcode);
					assert_true_sys(false, "OpenGL error");
				}
			}
		};
	}
}
