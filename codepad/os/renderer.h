#pragma once

#include <functional>
#include <vector>
#include <cstring>

#include <gl/GL.h>
#include <gl/glext.h>

#include "../utilities/misc.h"

namespace codepad {
	namespace os {
		class window_base;
		class renderer_base;
		class opengl_renderer_base;

		typedef size_t texture_id;
		typedef size_t framebuffer_id;
		struct framebuffer {
			friend class renderer_base;
			friend class opengl_renderer_base;
		public:
			framebuffer() = default;
			framebuffer(const framebuffer&) = delete;
			framebuffer(framebuffer &&r) : _id(r._id), _tid(r._tid), _w(r._w), _h(r._h) {
				r._tid = 0;
				r._w = r._h = 0;
			}
			framebuffer &operator=(const framebuffer&) = delete;
			framebuffer &operator=(framebuffer &&r) {
				std::swap(_id, r._id);
				std::swap(_tid, r._tid);
				std::swap(_w, r._w);
				std::swap(_h, r._h);
				return *this;
			}
			~framebuffer();

			size_t get_width() const {
				return _w;
			}
			size_t get_height() const {
				return _h;
			}
			texture_id get_texture() const {
				return _tid;
			}

			bool has_content() const {
				return _tid != 0;
			}
		protected:
			framebuffer(framebuffer_id rid, texture_id tid, size_t w, size_t h) : _id(rid), _tid(tid), _w(w), _h(h) {
			}

			framebuffer_id _id;
			texture_id _tid = 0;
			size_t _w = 0, _h = 0;
		};

		class renderer_base {
			friend class window_base;
			friend struct codepad::globals;
		public:
			virtual ~renderer_base() {
			}

			virtual void begin(const window_base&) = 0;
			virtual void push_clip(recti) = 0;
			virtual void pop_clip() = 0;
			virtual void draw_character(texture_id, vec2d, colord) = 0;
			virtual void draw_triangles(const vec2d*, const vec2d*, const colord*, size_t, texture_id) = 0;
			virtual void draw_lines(const vec2d*, const colord*, size_t) = 0;
			virtual void draw_quad(rectd r, rectd t, colord c, texture_id tex) {
				vec2d vs[6]{
					r.xmin_ymin(), r.xmax_ymin(), r.xmin_ymax(),
					r.xmax_ymin(), r.xmax_ymax(), r.xmin_ymax()
				}, uvs[6]{
					t.xmin_ymin(), t.xmax_ymin(), t.xmin_ymax(),
					t.xmax_ymin(), t.xmax_ymax(), t.xmin_ymax()
				};
				colord cs[6] = {c, c, c, c, c, c};
				draw_triangles(vs, uvs, cs, 6, tex);
			}
			virtual void end() = 0;

			virtual texture_id new_character_texture(size_t, size_t, const void*) = 0;
			virtual void delete_character_texture(texture_id) = 0;
			virtual texture_id new_texture(size_t, size_t, const void*) = 0;
			virtual void delete_texture(texture_id) = 0;

			virtual framebuffer new_framebuffer(size_t, size_t) = 0;
			virtual void delete_framebuffer(framebuffer&) = 0;
			virtual void begin_framebuffer(const framebuffer&) = 0;
			virtual void continue_framebuffer(const framebuffer&) = 0;

			virtual void push_matrix(const matd3x3&) = 0;
			virtual void push_matrix_mult(const matd3x3&) = 0;
			virtual matd3x3 top_matrix() const = 0;
			virtual void pop_matrix() = 0;

			inline static renderer_base &get() {
				assert_true_usage(_get_rend().rend, "renderer not yet created");
				return *_get_rend().rend;
			}
			template <typename T, typename ...Args> inline static void create_default(Args &&...args) {
				_get_rend().create<T, Args...>(std::forward<Args>(args)...);
			}
		protected:
			virtual void _new_window(window_base&) = 0;
			virtual void _delete_window(window_base&) = 0;

			struct _default_renderer {
				template <typename T, typename ...Args> void create(Args &&...args) {
					assert_true_usage(!rend, "renderer already created");
					rend = new T(std::forward<Args>(args)...);
				}
				~_default_renderer() {
					assert_true_usage(rend, "no renderer created yet");
					delete rend;
				}
				renderer_base *rend = nullptr;
			};
			static _default_renderer &_get_rend();
		};

		texture_id load_image(renderer_base&, const str_t&); // not recommended
		inline texture_id load_image(const str_t &filename) {
			return load_image(renderer_base::get(), filename);
		}

		class opengl_renderer_base : public renderer_base {
		public:
			~opengl_renderer_base() override {
#ifdef CP_DETECT_LOGICAL_ERRORS
				assert_true_logical(_atl.disposed, "derived classes must manually dispose _atl");
#endif
			}

			void begin(const window_base&) override;
			void push_clip(recti r) override {
				_flush_text_buffer();
				_rtfstk.back().push_clip(r);
				_rtfstk.back().apply_clip();
			}
			void pop_clip() override {
				_flush_text_buffer();
				_rtfstk.back().pop_clip();
				_rtfstk.back().apply_clip();
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
				_end_render_target();
				_gl_verify();
			}

			texture_id new_character_texture(size_t w, size_t h, const void *data) override {
				return _atl.new_char(_get_gl_funcs(), w, h, data);
			}
			void delete_character_texture(texture_id id) override {
				_atl.delete_char(id);
			}
			texture_id new_texture(size_t w, size_t h, const void *data) override {
				GLuint texid;
				glGenTextures(1, &texid);
				glBindTexture(GL_TEXTURE_2D, texid);
				_set_default_texture_params();
				glTexImage2D(
					GL_TEXTURE_2D, 0, GL_RGBA8,
					static_cast<GLsizei>(w), static_cast<GLsizei>(h),
					0, GL_RGBA, GL_UNSIGNED_BYTE, data
				);
				_get_gl_funcs().GenerateMipmap(GL_TEXTURE_2D);
				return static_cast<texture_id>(texid);
			}
			void delete_texture(texture_id tex) override {
				GLuint t = static_cast<GLuint>(tex);
				glDeleteTextures(1, &t);
			}

			framebuffer new_framebuffer(size_t w, size_t h) override {
				GLuint fbid, tid;
				const _gl_funcs &gl = _get_gl_funcs();
				gl.GenFramebuffers(1, &fbid);
				glGenTextures(1, &tid);
				gl.BindFramebuffer(GL_FRAMEBUFFER, fbid);
				glBindTexture(GL_TEXTURE_2D, tid);
				_set_default_texture_params();
				glTexImage2D(
					GL_TEXTURE_2D, 0, GL_RGBA8,
					static_cast<GLsizei>(w), static_cast<GLsizei>(h),
					0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr
				);
				gl.GenerateMipmap(GL_TEXTURE_2D);
				gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tid, 0);
				assert_true_sys(
					gl.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
					"OpenGL error: unable to create framebuffer"
				);
				return framebuffer(fbid, tid, w, h);
			}
			void delete_framebuffer(framebuffer &fb) override {
				GLuint id = static_cast<GLuint>(fb._id), tid = static_cast<GLuint>(fb._tid);
				_get_gl_funcs().DeleteFramebuffers(1, &id);
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
				const _gl_funcs &gl = _get_gl_funcs();
				_begin_render_target(_render_target_stackframe(
					false, fb._w, fb._h,
					std::bind(gl.BindFramebuffer, GL_FRAMEBUFFER, static_cast<GLuint>(fb._id)),
					[this, tex = static_cast<GLuint>(fb._tid), gl]() {
					gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
					glBindTexture(GL_TEXTURE_2D, tex);
					gl.GenerateMipmap(GL_TEXTURE_2D);
				}
				));
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
			struct _gl_funcs {
				PFNGLGENFRAMEBUFFERSPROC GenFramebuffers = nullptr;
				PFNGLBINDFRAMEBUFFERPROC BindFramebuffer = nullptr;
				PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D = nullptr;
				PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus = nullptr;
				PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers = nullptr;
				PFNGLGENERATEMIPMAPPROC GenerateMipmap = nullptr;
			};

			// derived (platform-specific) classes must override these four functions
			virtual const _gl_funcs &_get_gl_funcs() = 0;
			virtual void _init_new_window(window_base&) = 0;
			virtual std::function<void()> _get_begin_window_func(const window_base&) = 0;
			virtual std::function<void()> _get_end_window_func(const window_base&) = 0;

			void _new_window(window_base &wnd) override {
				_init_new_window(wnd);
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
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
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
#ifdef CP_DETECT_LOGICAL_ERRORS
					assert_true_logical(!disposed, "text atlas already disposed");
					disposed = true;
#endif
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
					void flush(const _gl_funcs &gl) {
						glBindTexture(GL_TEXTURE_2D, tex_id);
						glTexImage2D(
							GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
							0, GL_RGBA, GL_UNSIGNED_BYTE, data
						);
						gl.GenerateMipmap(GL_TEXTURE_2D);
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
				const page &get_page(const _gl_funcs &gl, size_t page) {
					if (_lpdirty && page + 1 == _ps.size()) {
						_ps.back().flush(gl);
						_lpdirty = false;
					}
					return _ps[page];
				}

				size_t page_width = 600, page_height = 300, border = 1;
#ifdef CP_DETECT_LOGICAL_ERRORS
				bool disposed = false;
#endif
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
				texture_id new_char(const _gl_funcs &gl, size_t w, size_t h, const void *data) {
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
								curp.flush(gl);
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

			struct _render_target_stackframe {
				_render_target_stackframe() = default;
				template <typename T, typename U> _render_target_stackframe(bool invy, size_t w, size_t h, T fbeg, U fend) :
					invert_y(invy), width(w), height(h), begin(std::move(fbeg)), end(std::move(fend)) {
				}

				bool invert_y = false;
				size_t width = 0, height = 0;
				std::function<void()> begin, end;
				std::vector<recti> clip_stack;

				void push_clip(recti r) {
					if (invert_y) {
						int ymin = r.ymin, ch = static_cast<int>(height);
						r.ymin = ch - r.ymax;
						r.ymax = ch - ymin;
					}
					if (clip_stack.size() > 0) {
						r = recti::common_part(r, clip_stack.back());
					}
					r.make_valid_max();
					clip_stack.push_back(r);
				}
				void pop_clip() {
					clip_stack.pop_back();
				}
				void apply_clip() {
					if (clip_stack.size() > 0) {
						glEnable(GL_SCISSOR_TEST);
						const recti &r = clip_stack.back();
						glScissor(r.xmin, r.ymin, r.width(), r.height());
					} else {
						glDisable(GL_SCISSOR_TEST);
					}
				}
			};

			std::vector<matd3x3> _matstk;
			std::vector<_render_target_stackframe> _rtfstk;

			_text_atlas _atl;
			_text_buffer _textbuf;
			size_t _lstpg;

			void _begin_render_target(_render_target_stackframe rtf) {
				_rtfstk.push_back(std::move(rtf));
				_continue_last_render_target();
				glMatrixMode(GL_MODELVIEW);
				glPushMatrix();
				glLoadIdentity();
			}
			void _continue_last_render_target() {
				_render_target_stackframe &rtf = _rtfstk.back();
				rtf.begin();
				rtf.apply_clip();
				glViewport(0, 0, static_cast<GLsizei>(rtf.width), static_cast<GLsizei>(rtf.height));
				glMatrixMode(GL_PROJECTION);
				glLoadIdentity();
				if (rtf.invert_y) {
					glOrtho(0.0, static_cast<GLdouble>(rtf.width), static_cast<GLdouble>(rtf.height), 0.0, 0.0, -1.0);
				} else {
					glOrtho(0.0, static_cast<GLdouble>(rtf.width), 0.0, static_cast<GLdouble>(rtf.height), 0.0, -1.0);
				}
				_gl_verify();
			}
			void _end_render_target() {
				glMatrixMode(GL_MODELVIEW);
				glPopMatrix();
				_rtfstk.back().end();
				assert_true_usage(_rtfstk.back().clip_stack.empty(), "pushclip/popclip mismatch");
				_rtfstk.pop_back();
				if (!_rtfstk.empty()) {
					_continue_last_render_target();
				} else {
					assert_true_usage(_matstk.size() == 0, "pushmatrix/popmatrix mismatch");
				}
			}

			void _flush_text_buffer() {
				if (_textbuf.vertcount > 0) {
					_textbuf.flush_nocheck(_atl.get_page(_get_gl_funcs(), _lstpg).tex_id);
				}
			}
			void _gl_verify() {
#ifdef CP_DETECT_SYSTEM_ERRORS
				GLenum errorcode = glGetError();
				if (errorcode != GL_NO_ERROR) {
					logger::get().log_error(CP_HERE, "OpenGL error code ", errorcode);
					assert_true_sys(false, "OpenGL error");
				}
#endif
			}
	};

		inline framebuffer::~framebuffer() {
			if (_tid != 0) {
				renderer_base::get().delete_framebuffer(*this);
			}
		}
}
	}
