#pragma once

#include "../utilities/misc.h"

namespace codepad {
	namespace os {
		class window_base;
		class renderer_base;

		typedef size_t texture_id;
		typedef size_t framebuffer_id;
		struct framebuffer {
			friend class renderer_base;
			friend class opengl_renderer;
			friend class software_renderer;
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

			virtual framebuffer new_framebuffer(size_t, size_t) = 0;
			virtual void delete_framebuffer(framebuffer&) = 0;
			virtual void begin_framebuffer(const framebuffer&) = 0;
			virtual void continue_framebuffer(const framebuffer&) = 0;

			virtual void push_matrix(const matd3x3&) = 0;
			virtual void push_matrix_mult(const matd3x3&) = 0;
			virtual matd3x3 top_matrix() const = 0;
			virtual void pop_matrix() = 0;

			inline static renderer_base &get() {
				assert_true_usage(_rend.rend, "renderer not yet created");
				return *_rend.rend;
			}
			template <typename T, typename ...Args> inline static void create_default(Args &&...args) {
				_rend.create<T, Args...>(std::forward<Args>(args)...);
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
			static _default_renderer _rend;
		};

		inline framebuffer::~framebuffer() {
			if (_tid != 0) {
				renderer_base::get().delete_framebuffer(*this);
			}
		}
	}
}
