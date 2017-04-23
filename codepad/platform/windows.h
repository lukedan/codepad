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

#ifndef NDEBUG
#	include <iostream>
#endif

namespace codepad {
	namespace platform {
		template <typename T> inline void winapi_check(T v) {
#ifndef NDEBUG
			if (!v) {
				std::cout << "ERROR: " << GetLastError() << "\n";
			}
#endif
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
			HDC _dc;

			struct _wndclass {
				_wndclass() {
					WNDCLASSEX wcex;
					std::memset(&wcex, 0, sizeof(wcex));
					wcex.style = CS_OWNDC;
					wcex.hInstance = GetModuleHandle(nullptr);
					winapi_check(wcex.hCursor = LoadCursor(nullptr, IDC_ARROW));
					wcex.cbSize = sizeof(wcex);
					wcex.lpfnWndProc = _wndproc;
					wcex.lpszClassName = CPTEXT("Codepad");
					winapi_check(atom = RegisterClassEx(&wcex));
				}
				~_wndclass() {
					UnregisterClass(reinterpret_cast<LPCTSTR>(atom), GetModuleHandle(nullptr));
				}

				ATOM atom;
			};
			static _wndclass _class;

			void _recalc_layout() {
				RECT cln;
				winapi_check(GetClientRect(_hwnd, &cln));
				_layout = rectd::from_xywh(0.0, 0.0, cln.right, cln.bottom);
			}
		};

		class software_renderer : public renderer_base {
		public:
			software_renderer() : renderer_base() {
				_txs.push_back(_tex_rec());
			}

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
			void draw_character(texture_id tex, vec2d, colord) override; // TODO
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

			texture_id new_character_texture(size_t w, size_t h, const void *gs) override {
				texture_id nid = _alloc_id();
				_txs[nid].set_grayscale(w, h, static_cast<const unsigned char*>(gs));
				return nid;
			}
			void delete_character_texture(texture_id id) {
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
					if (!data) {
						return colord();
					}
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

			bool support_partial_redraw() const override {
				return false;
			}

			void new_window(window_base &wnd) override {
				window *cw = static_cast<window*>(&wnd);
				winapi_check(SetPixelFormat(cw->_dc, _pformat, &_pfd));
				if (!_rc) {
					winapi_check(_rc = wglCreateContext(cw->_dc));
				}
				_on_window_size_changed(cw, static_cast<GLsizei>(cw->_layout.width()), static_cast<GLsizei>(cw->_layout.height()));
				cw->size_changed += [this, cw](size_changed_info &info) {
					_on_window_size_changed(cw, info.new_size.x, info.new_size.y);
				};
				glEnable(GL_TEXTURE_2D);
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glEnableClientState(GL_VERTEX_ARRAY);
				glEnableClientState(GL_NORMAL_ARRAY);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				glEnableClientState(GL_COLOR_ARRAY);
			}
			void delete_window(window_base&) override {
			}

			void begin(window_base &wnd, recti) override {
				window *cw = static_cast<window*>(&wnd);
				_curdc = cw->_dc;
				winapi_check(wglMakeCurrent(_curdc, _rc));
				glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
				glClear(GL_COLOR_BUFFER_BIT);
			}
			void draw_character(texture_id id, vec2d pos, colord color) override {
				const text_atlas::char_data &cd = _atl.get_char_data(id);
				if (_lstpg != cd.page) {
					_flush_text_buffer();
					_lstpg = cd.page;
				}
				unsigned int beg = static_cast<unsigned int>(_text_cache.size());
				_text_cache.push_back(_vertex(pos, cd.layout.xmin_ymin(), color));
				_text_cache.push_back(_vertex(pos + vec2d(cd.w, 0.0), cd.layout.xmax_ymin(), color));
				_text_cache.push_back(_vertex(pos + vec2d(0.0, cd.h), cd.layout.xmin_ymax(), color));
				_text_cache.push_back(_vertex(pos + vec2d(cd.w, cd.h), cd.layout.xmax_ymax(), color));
				_text_cache_ids.push_back(beg);
				_text_cache_ids.push_back(beg + 1);
				_text_cache_ids.push_back(beg + 2);
				_text_cache_ids.push_back(beg + 1);
				_text_cache_ids.push_back(beg + 3);
				_text_cache_ids.push_back(beg + 2);
			}
			void draw_triangles(const vec2d *ps, const vec2d *us, const colord *cs, size_t n, texture_id t) override {
				_flush_text_buffer();
				glVertexPointer(2, GL_DOUBLE, sizeof(vec2d), ps);
				glTexCoordPointer(2, GL_DOUBLE, sizeof(vec2d), us);
				glColorPointer(4, GL_DOUBLE, sizeof(colord), cs);
				glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(t));
				glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(n));
			}
			void end() override {
				_flush_text_buffer();
				winapi_check(SwapBuffers(_curdc));
				_gl_verify();
			}

			texture_id new_character_texture(size_t w, size_t h, const void *data) override {
				return _atl.new_char(w, h, data);
			}
			void delete_character_texture(texture_id id) override {
				_atl.delete_char(id);
			}
		protected:
			struct text_atlas {
				void dispose() {
					for (auto i = _ps.begin(); i != _ps.end(); ++i) {
						i->dispose();
					}
				}

				struct page {
					void create(size_t w, size_t h) {
						width = w;
						height = h;
						data = static_cast<unsigned char*>(std::malloc(width * height * 4));
						glGenTextures(1, &tex_id);
						glBindTexture(GL_TEXTURE_2D, tex_id);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					}
					void flush() {
						glBindTexture(GL_TEXTURE_2D, tex_id);
						glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
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
							l / static_cast<double>(curp.width), (l + w) / static_cast<double>(curp.width),
							t / static_cast<double>(curp.height), (t + h) / static_cast<double>(curp.height)
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

			struct _vertex {
				_vertex(vec2d p, vec2d u, colord c) : pos(p), uv(u), color(c) {
				}

				vec2d pos, uv;
				colord color;
			};

			HGLRC _rc = nullptr;
			PIXELFORMATDESCRIPTOR _pfd;
			HDC _curdc;
			int _pformat;

			text_atlas _atl;
			std::vector<_vertex> _text_cache;
			std::vector<unsigned int> _text_cache_ids;
			size_t _lstpg;

			void _flush_text_buffer() {
				if (_text_cache.size() > 0) {
					glVertexPointer(2, GL_DOUBLE, sizeof(_vertex), &_text_cache[0].pos);
					glTexCoordPointer(2, GL_DOUBLE, sizeof(_vertex), &_text_cache[0].uv);
					glColorPointer(4, GL_DOUBLE, sizeof(_vertex), &_text_cache[0].color);
					glBindTexture(GL_TEXTURE_2D, _atl.get_page(_lstpg).tex_id);
					glDrawElements(GL_TRIANGLES, _text_cache_ids.size(), GL_UNSIGNED_INT, _text_cache_ids.data());
					_text_cache.clear();
					_text_cache_ids.clear();
				}
			}

			void _gl_verify() {
				GLenum error = glGetError();
#ifndef NDEBUG
				if (error != GL_NO_ERROR) {
					std::cout << "OpenGL error: " << error << "\n";
				}
#endif
				assert(error == GL_NO_ERROR);
			}
			void _on_window_size_changed(window *wnd, GLsizei w, GLsizei h) {
				winapi_check(wglMakeCurrent(wnd->_dc, _rc));
				glViewport(0, 0, w, h);
				glMatrixMode(GL_PROJECTION);
				glLoadIdentity();
				glOrtho(0.0, w, h, 0.0, 0.0, -1.0);
			}
		};
	}
}
