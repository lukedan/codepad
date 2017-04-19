#pragma once

#include "../ui/element.h"
#include "../utilities/textconfig.h"

namespace codepad {
	namespace platform {
		struct size_changed_info {
			size_changed_info(vec2i v) : new_size(v) {
			}
			const vec2i new_size;
		};
		class window_base : public ui::element {
		public:
			virtual void set_caption(const str_t&) = 0;

			virtual bool idle() = 0;

			virtual vec2i screen_to_client(vec2i) const = 0;
			virtual vec2i client_to_screen(vec2i) const = 0;

			event<void_info> close_request;
			event<size_changed_info> size_changed;
		protected:
			virtual void _on_close_request(void_info &p) {
				close_request(p);
			}
			virtual void _on_size_changed(size_changed_info &p) {
				size_changed(p);
			}
		};

		typedef size_t texture_id;
		class renderer_base {
		public:
			virtual ~renderer_base() {
			}

			virtual bool support_partial_redraw() const = 0;

			virtual void new_window(window_base&) = 0;
			virtual void delete_window(window_base&) = 0;

			virtual void begin(window_base&, recti) = 0;
			virtual void draw_triangles(const vec2d*, const vec2d*, const colord*, size_t, texture_id) = 0;
			virtual void end() = 0;

			virtual texture_id new_texture_grayscale(size_t, size_t, const void*) = 0;
			virtual void delete_texture(texture_id) = 0;
		};
	}
}
