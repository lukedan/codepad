#pragma once

#include "codebox.h"
#include "../ui/textrenderer.h"

namespace codepad {
	namespace editor {
		class codebox_line_number : public codebox_component {
		public:
			vec2d get_desired_size() const override {
				size_t ln = _get_box()->get_editor()->get_context()->num_lines(), w = 0;
				for (; ln > 0; ++w, ln /= 10) {
				}
				vec2d res = get_padding().size();
				res.x += w * codebox_editor::get_font().maximum_width();
				return res;
			}
		protected:
			void _render() const override {
				codebox_editor *editor = _get_box()->get_editor();
				double lh = editor->get_line_height(), pos = _get_box()->get_vertical_position();
				size_t
					line_beg = static_cast<size_t>(std::max(0.0, pos - get_padding().top) / lh),
					line_end = std::min(
						static_cast<size_t>((pos + get_client_region().height() + get_padding().bottom) / lh) + 1,
						_get_box()->get_editor()->get_context()->num_lines()
					);
				double cury = get_client_region().ymin - pos + static_cast<double>(line_beg) * lh;
				for (size_t i = line_beg + 1; i <= line_end; ++i, cury += lh) {
					str_t curlbl = to_str(i);
					double w = ui::text_renderer::measure_plain_text(curlbl, *codebox_editor::get_font().normal).x;
					ui::text_renderer::render_plain_text(
						curlbl, *codebox_editor::get_font().normal,
						vec2d(get_client_region().xmax - w, cury), colord()
					);
				}
			}
		};
	}
}
