#pragma once

#include "../utilities/misc.h"

namespace codepad {
	namespace platform {
		typedef size_t texture_id;
		class window_base;
		class renderer_base {
		public:
			virtual ~renderer_base() {
			}

			virtual void new_window(window_base&) = 0;
			virtual void delete_window(window_base&) = 0;

			virtual void begin(const window_base&) = 0;
			virtual void push_clip(recti) = 0;
			virtual void pop_clip() = 0;
			virtual void draw_character(texture_id, vec2d, colord) = 0;
			virtual void draw_triangles(const vec2d*, const vec2d*, const colord*, size_t, texture_id) = 0;
			virtual void draw_lines(const vec2d*, const colord*, size_t) = 0;
			virtual void end() = 0;

			virtual texture_id new_character_texture(size_t, size_t, const void*) = 0;
			virtual void delete_character_texture(texture_id) = 0;
		};
	}
}
