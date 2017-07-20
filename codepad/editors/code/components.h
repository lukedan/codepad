#pragma once

#include <deque>

#include "editor.h"
#include "../../ui/textrenderer.h"

namespace codepad {
	namespace editor {
		class codebox_line_number : public codebox_component {
		public:
			vec2d get_desired_size() const override {
				size_t ln = _get_editor()->get_context()->num_lines(), w = 0;
				for (; ln > 0; ++w, ln /= 10) {
				}
				return vec2d(get_padding().width() + static_cast<double>(w) * codebox_editor::get_font().maximum_width(), 0.0);
			}
		protected:
			void _render() const override {
				codebox_editor *editor = _get_editor();
				double lh = editor->get_line_height(), pos = _get_box()->get_vertical_position();
				std::pair<size_t, size_t> be = editor->get_visible_lines();
				double cury = get_client_region().ymin - pos + static_cast<double>(be.first) * lh;
				for (size_t i = be.first + 1; i <= be.second; ++i, cury += lh) {
					str_t curlbl = to_str(i);
					double w = ui::text_renderer::measure_plain_text(curlbl, *codebox_editor::get_font().normal).x;
					ui::text_renderer::render_plain_text(
						curlbl, *codebox_editor::get_font().normal,
						vec2d(get_client_region().xmax - w, cury), colord()
					);
				}
			}
		};

		class codebox_minimap : public codebox_component {
		public:
			constexpr static double scale = 0.1;
			constexpr static size_t maximum_width = 150, minimum_page_size = 800; // in pixels

			vec2d get_desired_size() const override {
				codebox *cb = _get_box();
				return vec2d(std::min<double>(
					static_cast<double>(maximum_width), cb->get_actual_size().x * scale / (1.0 + scale)
					), 0.0);
			}
		protected:
			void _render() const override {
				rectd clnrgn = get_client_region();
				size_t nlines = _get_editor()->get_context()->num_lines();
				double
					rgnh = clnrgn.height(),
					lineh = _get_editor()->get_line_height(),
					vertp = _get_box()->get_vertical_position(),
					yoffset = std::max(_get_box()->get_vertical_position() * (scale - (rgnh - lineh * scale) / ((nlines - 1) * lineh)), 0.0);
				clnrgn.xmax = clnrgn.xmin + maximum_width;
				clnrgn.ymin -= _get_y_offset();
				clnrgn.ymin = std::round(clnrgn.ymin);
				clnrgn.ymax = clnrgn.ymin + _buf.get_height();
				os::renderer_base::get().draw_quad(clnrgn, rectd(0.0, 1.0, 0.0, 1.0), colord(), _buf.get_texture());
			}

			double _get_y_offset() const {
				const codebox *cb = _get_box();
				const codebox_editor *editor = cb->get_editor();
				size_t nlines = editor->get_context()->num_lines();
				double
					lh = editor->get_line_height(),
					vh = editor->get_client_region().height(),
					maxh = nlines * lh - vh,
					maxvh = nlines * lh * scale - vh,
					perc = _get_box()->get_vertical_position() / maxh;
				perc = clamp(perc, 0.0, 1.0);
				return perc * maxvh;
			}
			std::pair<size_t, size_t> _get_visible_lines() const {
				const codebox_editor *editor = _get_editor();
				double lh = editor->get_line_height() * scale, ys = _get_y_offset();
				return std::make_pair(static_cast<size_t>(ys / lh), std::min(
					static_cast<size_t>((ys + editor->get_client_region().height()) / lh) + 1, editor->get_context()->num_lines()
				));
			}

			void _on_added() override {
				_text_tok = (
					_get_editor()->display_changed +=
					std::bind(&codebox_minimap::_on_editor_display_changed, this)
					);
				_vpos_tok = (_get_box()->vertical_viewport_changed += [this](value_update_info<double> &v) {
					_on_viewport_changed();
				});
			}
			void _on_removing() override {
				_get_editor()->display_changed -= _text_tok;
				_get_box()->vertical_viewport_changed -= _vpos_tok;
			}

			void _render_page(size_t s, size_t pe, bool cont) const {
				auto start_time = std::chrono::high_resolution_clock::now();
				const codebox_editor *ce = _get_editor();
				double lh = ce->get_line_height(), ybeg = lh * s;

				if (cont) {
					os::renderer_base::get().continue_framebuffer(_buf);
				} else {
					os::renderer_base::get().begin_framebuffer(_buf);
				}
				os::renderer_base::get().push_matrix(matd3x3::scale(vec2d(), vec2d(scale, scale)));

				ui::font_family::baseline_info bi = codebox_editor::get_font().get_baseline_info();
				for (codebox_editor::character_rendering_iterator it = ce->start_rendering(s, pe); it.next_char(); ) {
					const ui::line_character_iterator &ci = it.character_info();
					const text_theme_data::char_iterator &ti = it.theme_info();
					if (is_graphical_char(ci.current_char())) {
						os::renderer_base::get().draw_quad(
							ci.current_char_entry().placement.translated(
								vec2d(ci.char_left(), ybeg + it.y_offset() + bi.get(ti.current_theme.style))
							),
							rectd(), ti.current_theme.color, 0
						);
						//os::renderer_base::get().draw_character(
						//	ci.current_char_entry().texture,
						//	vec2d(ci.char_left(), ybeg + it.y_offset() + bi.get(ti.current_theme.style)) +
						//	ci.current_char_entry().placement.xmin_ymin(),
						//	ti.current_theme.color
						//);
					}
				}

				os::renderer_base::get().pop_matrix();
				os::renderer_base::get().end_framebuffer();

				auto dur = std::chrono::high_resolution_clock::now() - start_time;
				CP_INFO("minimap rendering cost ", std::chrono::duration<double, std::milli>(dur).count(), "ms");
			}

			void _restart_cache() {
				codebox_editor *editor = _get_editor();
				std::pair<size_t, size_t> be = _get_visible_lines();
				double slh = editor->get_line_height() * scale;
				size_t
					numlines = editor->get_context()->num_lines(),
					pgsize = std::max(be.second - be.first, static_cast<size_t>(minimum_page_size / slh) + 1);
				_buf = os::renderer_base::get().new_framebuffer(
					maximum_width, static_cast<size_t>(std::ceil(slh * numlines))
				);
				_page_beg = 0;
				_page_end = numlines;
				if (pgsize < numlines) {
					if (be.first + be.second < pgsize) {
						_page_end = pgsize;
					} else if (be.first + be.second + pgsize > numlines * 2) {
						_page_beg = numlines - pgsize;
					} else {
						_page_beg = (be.first + be.second - pgsize) / 2;
						_page_end = _page_beg + pgsize;
					}
				}
				_render_page(_page_beg, _page_end, false);
			}
			void _on_viewport_changed() {
				if (!_buf.has_content()) {
					_restart_cache();
				} else {
					codebox *box = _get_box();
					codebox_editor *editor = box->get_editor();
					std::pair<size_t, size_t> be = _get_visible_lines();
					if (be.first >= _page_beg && be.second <= _page_end) {
						return;
					}
					size_t page_lines = std::max(
						be.second - be.first, static_cast<size_t>(minimum_page_size / (editor->get_line_height() * scale)) + 1
					);
					if (be.first + page_lines < _page_beg || _page_end + page_lines < be.second) {
						_restart_cache();
					} else {
						if (be.first < _page_beg) {
							size_t frontline = 0;
							if (page_lines < _page_beg) {
								frontline = _page_beg - page_lines;
							}
							_render_page(frontline, _page_beg, true);
							_page_beg = frontline;
						}
						if (be.second > _page_end) {
							size_t backline = std::min(editor->get_context()->num_lines(), _page_end + page_lines);
							_render_page(_page_end, backline, true);
							_page_end = backline;
						}
					}
				}
			}
			void _on_editor_display_changed() {
				_restart_cache();
			}

			size_t _page_beg, _page_end;
			os::framebuffer _buf;
			event<void>::token _text_tok;
			event<value_update_info<double>>::token _vpos_tok;
		};
	}
}
