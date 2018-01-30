#pragma once

#include <functional>
#include <vector>
#include <cstring>

// gl.h on windows depends on Windows.h
#ifdef CP_PLATFORM_WINDOWS
#	include <Windows.h>
#	include <windowsx.h>
#endif

#include <GL/gl.h>
#include <GL/glext.h>

#include "../utilities/misc.h"

namespace codepad {
	namespace os {
		class window_base;
		class renderer_base;
		class opengl_renderer_base;

		enum class blend_factor {
			zero,
			one,
			source_alpha,
			one_minus_source_alpha,
			destination_alpha,
			one_minus_destination_alpha,
			source_color,
			one_minus_source_color,
			destination_color,
			one_minus_destination_color
		};
		struct blend_function {
			blend_function() = default;
			blend_function(blend_factor src, blend_factor dst) : source_factor(src), destination_factor(dst) {
			}

			blend_factor source_factor = blend_factor::source_alpha, destination_factor = blend_factor::one_minus_source_alpha;
		};

		template <bool IsNormal> struct texture_base {
			friend class renderer_base;
		public:
			constexpr static bool is_normal_texture = IsNormal;

			using id_t = size_t;

			texture_base() = default;
			texture_base(const texture_base&) = delete;
			texture_base(texture_base &&t) : _id(t._id), _rend(t._rend), _w(t._w), _h(t._h) {
				t._id = 0;
				t._rend = nullptr;
				t._w = t._h = 0;
			}
			texture_base &operator=(const texture_base&) = delete;
			texture_base &operator=(texture_base &&t) {
				std::swap(_id, t._id);
				std::swap(_rend, t._rend);
				std::swap(_w, t._w);
				std::swap(_h, t._h);
				return *this;
			}
			~texture_base();

			renderer_base *get_renderer() const {
				return _rend;
			}
			size_t get_width() const {
				return _w;
			}
			size_t get_height() const {
				return _h;
			}

			bool has_content() const {
				return _rend != nullptr;
			}
		protected:
			texture_base(id_t id, renderer_base *r, size_t w, size_t h) : _id(id), _rend(r), _w(w), _h(h) {
			}

			id_t _id = 0;
			renderer_base *_rend = nullptr;
			size_t _w = 0, _h = 0;
		};
		using texture = texture_base<true>;
		using char_texture = texture_base<false>;
		struct framebuffer {
			friend class renderer_base;
		public:
			using id_t = size_t;

			framebuffer() = default;
			framebuffer(const framebuffer&) = delete;
			framebuffer(framebuffer &&r) : _id(r._id), _tex(std::move(r._tex)) {
			}
			framebuffer &operator=(const framebuffer&) = delete;
			framebuffer &operator=(framebuffer &&r) {
				std::swap(_id, r._id);
				_tex = std::move(r._tex);
				return *this;
			}
			~framebuffer();

			const texture &get_texture() const {
				return _tex;
			}

			bool has_content() const {
				return _tex.has_content();
			}
		protected:
			framebuffer(id_t rid, texture &&t) : _id(rid), _tex(std::move(t)) {
			}

			id_t _id;
			texture _tex;
		};

		class renderer_base {
			friend class window_base;
			friend struct codepad::globals;
		public:
			virtual ~renderer_base() = default;

			virtual void begin(const window_base&) = 0;
			virtual void push_clip(recti) = 0;
			virtual void pop_clip() = 0;
			virtual void draw_character_custom(const char_texture&, rectd, colord) = 0;
			virtual void draw_character(const char_texture &t, vec2d pos, colord c) {
				draw_character_custom(t, rectd::from_xywh(
					pos.x, pos.y, static_cast<double>(t.get_width()), static_cast<double>(t.get_height())
				), c);
			}
			virtual void draw_triangles(const texture&, const vec2d*, const vec2d*, const colord*, size_t) = 0;
			virtual void draw_lines(const vec2d*, const colord*, size_t) = 0;
			virtual void draw_quad(const texture &tex, rectd r, rectd t, colord c) {
				vec2d vs[6]{
					r.xmin_ymin(), r.xmax_ymin(), r.xmin_ymax(),
					r.xmax_ymin(), r.xmax_ymax(), r.xmin_ymax()
				}, uvs[6]{
					t.xmin_ymin(), t.xmax_ymin(), t.xmin_ymax(),
					t.xmax_ymin(), t.xmax_ymax(), t.xmin_ymax()
				};
				colord cs[6] = {c, c, c, c, c, c};
				draw_triangles(tex, vs, uvs, cs, 6);
			}
			virtual void end() = 0;

			virtual char_texture new_character_texture(size_t, size_t, const void*) = 0;
			virtual void delete_character_texture(char_texture&) = 0;
			virtual texture new_texture(size_t, size_t, const void*) = 0;
			virtual void delete_texture(texture&) = 0;
			void delete_texture(char_texture &tex) {
				delete_character_texture(tex);
			}

			virtual framebuffer new_framebuffer(size_t, size_t) = 0;
			virtual void delete_framebuffer(framebuffer&) = 0;
			virtual void begin_framebuffer(const framebuffer&) = 0;
			virtual void continue_framebuffer(const framebuffer&) = 0;

			virtual void push_matrix(const matd3x3&) = 0;
			virtual void push_matrix_mult(const matd3x3&) = 0;
			virtual matd3x3 top_matrix() const = 0;
			virtual void pop_matrix() = 0;

			virtual void push_blend_function(blend_function) = 0;
			virtual void pop_blend_function() = 0;
			virtual blend_function top_blend_function() const = 0;

			inline static renderer_base &get() {
				assert_true_usage(_get_rend().rend != nullptr, "renderer not yet created");
				return *_get_rend().rend;
			}
			template <typename T, typename ...Args> inline static void create_default(Args &&...args) {
				T *rend = new T(std::forward<Args>(args)...);
				_get_rend().assign(rend);
			}
		protected:
			virtual void _new_window(window_base&) = 0;
			virtual void _delete_window(window_base&) = 0;

			template <bool V> inline static typename texture_base<V>::id_t _get_id(const texture_base<V> &t) {
				return t._id;
			}
			inline static framebuffer::id_t _get_id(const framebuffer &f) {
				return f._id;
			}
			template <bool Normal = true> texture_base<Normal> _make_texture(typename texture_base<Normal>::id_t id, size_t w, size_t h) {
				return texture_base<Normal>(id, this, w, h);
			}
			framebuffer _make_framebuffer(framebuffer::id_t id, texture &&tex) {
				return framebuffer(id, std::move(tex));
			}
			template <bool Normal> inline static void _erase_texture(texture_base<Normal> &t) {
				t._id = 0;
				t._w = t._h = 0;
				t._rend = nullptr;
			}
			inline static void _erase_framebuffer(framebuffer &fb) {
				fb._id = 0;
				_erase_texture(fb._tex);
			}

			void _del_tex_univ(texture &tex) {
				delete_texture(tex);
			}
			void _del_tex_univ(char_texture &tex) {
				delete_character_texture(tex);
			}

			struct _default_renderer {
				void assign(renderer_base *r) {
					assert_true_usage(!rend, "renderer already created");
					rend = r;
				}
				~_default_renderer() {
					assert_true_usage(rend != nullptr, "no renderer created yet");
					delete rend;
				}
				renderer_base *rend = nullptr;
			};
			static _default_renderer &_get_rend();
		};

		texture load_image(renderer_base&, const std::filesystem::path&);
		inline texture load_image(const std::filesystem::path &filename) {
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
			}
			void pop_clip() override {
				_flush_text_buffer();
				_rtfstk.back().pop_clip();
			}
			void draw_character_custom(const char_texture &id, rectd r, colord color) override {
				const _text_atlas::char_data &cd = _atl.get_char_data(_get_id(id));
				if (_lstpg != cd.page) {
					_flush_text_buffer();
					_lstpg = cd.page;
				}
				if (!_textbuf.initialized()) {
					_textbuf.initialize(*this);
				}
				_textbuf.append(r.xmin_ymin(), r.xmax_ymax(), cd.layout, color);
			}
			void draw_triangles(const texture &t, const vec2d *ps, const vec2d *us, const colord *cs, size_t n) override {
				_flush_text_buffer();
				glVertexPointer(2, GL_DOUBLE, sizeof(vec2d), ps);
				glTexCoordPointer(2, GL_DOUBLE, sizeof(vec2d), us);
				glColorPointer(4, GL_DOUBLE, sizeof(colord), cs);
				glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(_get_id(t)));
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
				_end_render_target();
				_gl_verify();
			}

			char_texture new_character_texture(size_t w, size_t h, const void *data) override {
				return _atl.new_char(*this, w, h, data);
			}
			void delete_character_texture(char_texture &id) override {
				_atl.delete_char(id);
			}
			texture new_texture(size_t w, size_t h, const void *data) override {
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
				return _make_texture(static_cast<texture::id_t>(texid), w, h);
			}
			void delete_texture(texture &tex) override {
				GLuint t = static_cast<GLuint>(_get_id(tex));
				glDeleteTextures(1, &t);
				_erase_texture(tex);
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
				return _make_framebuffer(fbid, _make_texture(static_cast<texture::id_t>(tid), w, h));
			}
			void delete_framebuffer(framebuffer &fb) override {
				GLuint id = static_cast<GLuint>(_get_id(fb)), tid = static_cast<GLuint>(_get_id(fb.get_texture()));
				_get_gl_funcs().DeleteFramebuffers(1, &id);
				glDeleteTextures(1, &tid);
				_erase_framebuffer(fb);
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
					false, fb.get_texture().get_width(), fb.get_texture().get_height(),
					std::bind(gl.BindFramebuffer, GL_FRAMEBUFFER, static_cast<GLuint>(_get_id(fb))),
					[this, tex = static_cast<GLuint>(_get_id(fb.get_texture())), gl]() {
					gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
					glBindTexture(GL_TEXTURE_2D, tex);
					gl.GenerateMipmap(GL_TEXTURE_2D);
				}
				));
				_gl_verify();
			}

			void push_matrix(const matd3x3 &m) override {
				_flush_text_buffer();
				glMatrixMode(GL_MODELVIEW);
				glPushMatrix();
				double d[16];
				_set_gl_matrix(m, d);
				glLoadMatrixd(d);
			}
			void push_matrix_mult(const matd3x3 &m) override {
				_flush_text_buffer();
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
				_flush_text_buffer();
				glMatrixMode(GL_MODELVIEW);
				glPopMatrix();
			}

			void push_blend_function(blend_function f) override {
				_flush_text_buffer();
				_rtfstk.back().push_blend_func(f);
			}
			void pop_blend_function() override {
				_flush_text_buffer();
				_rtfstk.back().pop_blend_func();
			}
			blend_function top_blend_function() const override {
				if (_rtfstk.back().blend_func_stack.empty()) {
					return blend_function();
				}
				return _rtfstk.back().blend_func_stack.back();
			}
		protected:
			static GLenum _blend_func_mapping[10];

			struct _gl_funcs {
				PFNGLGENBUFFERSPROC GenBuffers = nullptr;
				PFNGLDELETEBUFFERSPROC DeleteBuffers = nullptr;
				PFNGLBINDBUFFERPROC BindBuffer = nullptr;
				PFNGLBUFFERDATAPROC BufferData = nullptr;
				PFNGLMAPBUFFERPROC MapBuffer = nullptr;
				PFNGLUNMAPBUFFERPROC UnmapBuffer = nullptr;
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

			struct _gl_vertex_buffer {
				friend class opengl_renderer_base;
			public:
				_gl_vertex_buffer() = default;
				_gl_vertex_buffer(const _gl_vertex_buffer&) = delete;
				_gl_vertex_buffer(_gl_vertex_buffer &&r) : _rend(r._rend), _id(r._id) {
					r._rend = nullptr;
					r._id = 0;
				}
				_gl_vertex_buffer &operator=(const _gl_vertex_buffer&) = delete;
				_gl_vertex_buffer &operator=(_gl_vertex_buffer &&r) {
					std::swap(_rend, r._rend);
					std::swap(_id, r._id);
					return *this;
				}
				~_gl_vertex_buffer() {
					if (_rend && _id != 0) {
						_rend->_delete_vertex_buffer(*this);
					}
				}

				opengl_renderer_base *get_context() const {
					return _rend;
				}
			protected:
				opengl_renderer_base * _rend = nullptr;
				GLuint _id = 0;
			};
			_gl_vertex_buffer _new_vertex_buffer() {
				_gl_vertex_buffer res;
				res._rend = this;
				_get_gl_funcs().GenBuffers(1, &res._id);
				return res;
			}
			void _bind_vertex_buffer(GLenum target, const _gl_vertex_buffer &buf) {
				_get_gl_funcs().BindBuffer(target, buf._id);
			}
			void _unbind_vertex_buffer(GLenum target) {
				_get_gl_funcs().BindBuffer(target, 0);
			}
			void _resize_vertex_buffer_stream_draw(GLenum target, const _gl_vertex_buffer &buf, size_t size) {
				_bind_vertex_buffer(target, buf);
				_get_gl_funcs().BufferData(target, size, nullptr, GL_DYNAMIC_DRAW);
			}
			void *_map_vertex_buffer(GLenum target, const _gl_vertex_buffer &buf) {
				_bind_vertex_buffer(target, buf);
				return _get_gl_funcs().MapBuffer(target, GL_READ_WRITE);
			}
			void _unmap_vertex_buffer(GLenum target, const _gl_vertex_buffer &buf) {
				_bind_vertex_buffer(target, buf);
				_get_gl_funcs().UnmapBuffer(target);
			}
			void _delete_vertex_buffer(_gl_vertex_buffer &buf) {
				_get_gl_funcs().DeleteBuffers(1, &buf._id);
				buf._rend = nullptr;
				buf._id = 0;
			}
			template <typename T, GLenum Type> struct _automatic_gl_vertex_buffer {
			public:
				constexpr static size_t minimum_size = 20;

				void initialize(opengl_renderer_base &rend) {
					_buf = rend._new_vertex_buffer();
					_cap = minimum_size;
					rend._resize_vertex_buffer_stream_draw(Type, _buf, sizeof(T) * minimum_size);
					_ptr = static_cast<T*>(rend._map_vertex_buffer(Type, _buf));
				}
				bool initialized() const {
					return _buf.get_context() != nullptr;
				}
				void dispose() {
					if (initialized()) {
						_buf.get_context()->_unmap_vertex_buffer(Type, _buf);
						_buf.get_context()->_delete_vertex_buffer(_buf);
					}
				}

				void push_back(const T &obj) {
					if (_cnt == _cap) {
						_set_capacity(_cap * 2);
					}
					_ptr[_cnt++] = obj;
				}
				void reserve(size_t cnt) {
					if (cnt > _cap) {
						_set_capacity(cnt);
					}
				}

				void bind() {
					_buf.get_context()->_unmap_vertex_buffer(Type, _buf);
					_buf.get_context()->_bind_vertex_buffer(Type, _buf);
				}
				void unbind() {
					_ptr = static_cast<T*>(_buf.get_context()->_map_vertex_buffer(Type, _buf));
					_buf.get_context()->_unbind_vertex_buffer(Type);
				}

				const _gl_vertex_buffer &get_raw() const {
					return _buf;
				}

				size_t size() const {
					return _cnt;
				}
				size_t capacity() const {
					return _cap;
				}
				void clear() {
					_cnt = 0;
				}
			protected:
				_gl_vertex_buffer _buf;
				T *_ptr = nullptr;
				size_t _cnt = 0, _cap = 0;

				void _set_capacity(size_t cp) {
					_cap = cp;
					opengl_renderer_base *rend = _buf.get_context();
					_gl_vertex_buffer newbuf = rend->_new_vertex_buffer();
					rend->_resize_vertex_buffer_stream_draw(Type, newbuf, sizeof(T) * _cap);
					T *nptr = static_cast<T*>(rend->_map_vertex_buffer(Type, newbuf));
					std::memcpy(nptr, _ptr, sizeof(T) * _cnt);
					rend->_unmap_vertex_buffer(Type, _buf);
					_ptr = nptr;
					_buf = std::move(newbuf);
				}
			};

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
				std::vector<char_texture::id_t> _cd_alloc;
				bool _lpdirty = false;

				void _new_page() {
					page np;
					np.create(page_width, page_height);
					for (size_t y = 0; y < np.height; ++y) {
						unsigned char *cur = np.data + y * np.width * 4;
						for (size_t x = 0; x < np.width; ++x) {
							*(cur++) = 255;
							*(cur++) = 255;
							*(cur++) = 255;
							*(cur++) = 0;
						}
					}
					_ps.push_back(np);
				}
				char_texture::id_t _alloc_id() {
					texture::id_t res;
					if (!_cd_alloc.empty()) {
						res = _cd_alloc.back();
						_cd_alloc.pop_back();
					} else {
						res = _cd_slots.size();
						_cd_slots.emplace_back();
					}
					return res;
				}
			public:
				char_texture new_char(opengl_renderer_base &r, size_t w, size_t h, const void *data) {
					if (_ps.empty()) {
						_new_page();
					}
					texture::id_t id = _alloc_id();
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
								curp.flush(r._get_gl_funcs());
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
							for (size_t x = 0; x < w; ++x, src += 4, cur += 4) {
								cur[0] = src[0];
								cur[1] = src[1];
								cur[2] = src[2];
								cur[3] = src[3];
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
					return r._make_texture<false>(id, w, h);
				}
				void delete_char(char_texture &id) {
					_cd_alloc.push_back(_get_id(id));
					_erase_texture(id);
				}
			};
			struct _text_buffer {
				struct vertex {
					vertex(vec2d p, vec2d u, colord color) : v(p), uv(u), c(color) {
					}
					vec2d v, uv;
					colord c;
				};
				_automatic_gl_vertex_buffer<vertex, GL_ARRAY_BUFFER> vertex_buffer;
				_automatic_gl_vertex_buffer<unsigned int, GL_ELEMENT_ARRAY_BUFFER> id_buffer;
				size_t indexed_vertex_count = 0, used_index_count = 0;

				void initialize(opengl_renderer_base &rend) {
					vertex_buffer.initialize(rend);
					id_buffer.initialize(rend);
				}
				bool initialized() const {
					return vertex_buffer.initialized();
				}
				void dispose() {
					vertex_buffer.dispose();
					id_buffer.dispose();
				}

				void append(vec2d tl, vec2d br, rectd layout, colord c) {
					used_index_count += 6;
					if (indexed_vertex_count == vertex_buffer.size()) {
						unsigned int id = static_cast<unsigned int>(vertex_buffer.size());
						id_buffer.push_back(id);
						id_buffer.push_back(id + 1);
						id_buffer.push_back(id + 2);
						id_buffer.push_back(id + 1);
						id_buffer.push_back(id + 3);
						id_buffer.push_back(id + 2);
						indexed_vertex_count += 4;
					}
					vertex_buffer.push_back(vertex(tl, layout.xmin_ymin(), c));
					vertex_buffer.push_back(vertex(vec2d(br.x, tl.y), layout.xmax_ymin(), c));
					vertex_buffer.push_back(vertex(vec2d(tl.x, br.y), layout.xmin_ymax(), c));
					vertex_buffer.push_back(vertex(br, layout.xmax_ymax(), c));
				}
				void flush_nocheck(GLuint tex) {
					vertex_buffer.bind();
					id_buffer.bind();
					glVertexPointer(2, GL_DOUBLE, sizeof(vertex), reinterpret_cast<const GLvoid*>(offsetof(vertex, v)));
					glTexCoordPointer(2, GL_DOUBLE, sizeof(vertex), reinterpret_cast<const GLvoid*>(offsetof(vertex, uv)));
					glColorPointer(4, GL_DOUBLE, sizeof(vertex), reinterpret_cast<const GLvoid*>(offsetof(vertex, c)));
					glBindTexture(GL_TEXTURE_2D, tex);
					glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(used_index_count), GL_UNSIGNED_INT, nullptr);
					vertex_buffer.clear();
					used_index_count = 0;
					vertex_buffer.unbind();
					id_buffer.unbind();
				}
			};

			struct _render_target_stackframe {
				template <typename T, typename U> _render_target_stackframe(
					bool invy, size_t w, size_t h, T &&fbeg, U &&fend
				) : invert_y(invy), width(w), height(h), begin(std::forward<T>(fbeg)), end(std::forward<U>(fend)) {
				}

				bool invert_y = false;
				size_t width = 0, height = 0;
				std::function<void()> begin, end;
				std::vector<recti> clip_stack;
				std::vector<blend_function> blend_func_stack;

				void push_clip(recti r) {
					if (invert_y) {
						int ymin = r.ymin, ch = static_cast<int>(height);
						r.ymin = ch - r.ymax;
						r.ymax = ch - ymin;
					}
					if (!clip_stack.empty()) {
						r = recti::common_part(r, clip_stack.back());
					}
					r.make_valid_max();
					clip_stack.push_back(r);
					apply_clip();
				}
				void pop_clip() {
					clip_stack.pop_back();
					apply_clip();
				}
				void apply_clip() {
					if (clip_stack.empty()) {
						glDisable(GL_SCISSOR_TEST);
					} else {
						glEnable(GL_SCISSOR_TEST);
						const recti &r = clip_stack.back();
						glScissor(r.xmin, r.ymin, r.width(), r.height());
					}
				}

				void push_blend_func(blend_function bf) {
					blend_func_stack.push_back(bf);
					apply_blend_func();
				}
				void pop_blend_func() {
					blend_func_stack.pop_back();
					apply_blend_func();
				}
				void apply_blend_func() {
					if (blend_func_stack.empty()) {
						glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					} else {
						glBlendFunc(
							_blend_func_mapping[static_cast<int>(blend_func_stack.back().source_factor)],
							_blend_func_mapping[static_cast<int>(blend_func_stack.back().destination_factor)]
						);
					}
				}

				void apply_config() {
					apply_clip();
					apply_blend_func();
				}
			};

			std::vector<_render_target_stackframe> _rtfstk;

			_text_atlas _atl;
			_text_buffer _textbuf;
			size_t _lstpg;

			void _dispose_gl_rsrc() {
				_atl.dispose();
				_textbuf.dispose();
			}

			void _begin_render_target(_render_target_stackframe rtf) {
				if (!_rtfstk.empty()) {
					_flush_text_buffer();
				}
				_rtfstk.push_back(std::move(rtf));
				_continue_last_render_target();
				glMatrixMode(GL_MODELVIEW);
				glPushMatrix();
				glLoadIdentity();
			}
			void _continue_last_render_target() {
				_render_target_stackframe &rtf = _rtfstk.back();
				rtf.begin();
				rtf.apply_config();
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
				_flush_text_buffer();
				glMatrixMode(GL_MODELVIEW);
				glPopMatrix();
				_rtfstk.back().end();
				assert_true_usage(_rtfstk.back().clip_stack.empty(), "pushclip/popclip mismatch");
				_rtfstk.pop_back();
				if (!_rtfstk.empty()) {
					_continue_last_render_target();
				}
			}

			void _flush_text_buffer() {
				if (_textbuf.used_index_count > 0) {
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

		template <bool Normal> inline texture_base<Normal>::~texture_base() {
			if (has_content()) {
				_rend->delete_texture(*this);
			}
		}
		inline framebuffer::~framebuffer() {
			if (has_content()) {
				_tex.get_renderer()->delete_framebuffer(*this);
			}
		}
	}
}
