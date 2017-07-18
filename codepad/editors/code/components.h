#pragma once

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
				vec2d res = get_padding().size();
				res.x += static_cast<double>(w) * codebox_editor::get_font().maximum_width();
				return res;
			}
		protected:
			void _render() const override {
				codebox_editor *editor = _get_editor();
				double lh = editor->get_line_height(), pos = _get_box()->get_vertical_position();
				size_t
					line_beg = static_cast<size_t>(std::max(0.0, pos - get_padding().top) / lh),
					line_end = std::min(
						static_cast<size_t>((pos + get_client_region().height() + get_padding().bottom) / lh) + 1,
						editor->get_context()->num_lines()
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
		class codebox_minimap : public codebox_component {
		public:
			constexpr static double maximum_width = 150, scale = 0.1;
			constexpr static size_t page_size = 200;

			vec2d get_desired_size() const override {
				codebox *cb = _get_box();
				return vec2d(std::min(maximum_width, cb->get_actual_size().x * scale / (1.0 + scale)), 0.0);
			}
		protected:
			void _render() const override {
				rectd finrgn = get_client_region();
				finrgn.xmax = finrgn.xmin + maximum_width;
				finrgn.ymin -= _get_box()->get_vertical_position() * scale;
				finrgn.ymax = finrgn.ymin + codebox_editor::get_font().maximum_height() * scale * _get_editor()->get_context()->num_lines();
				os::renderer_base::get().draw_quad(finrgn, rectd(0.0, 1.0, 0.0, 1.0), colord(), _cache.get_texture());
			}

			void _on_editor_display_changed() {
				auto start_time = std::chrono::high_resolution_clock::now();
				codebox_editor *ce = _get_editor();
				text_context *ctx = ce->get_context();
				if (ctx) {
					const ui::font_family &fnt = codebox_editor::get_font();
					double lh = fnt.maximum_height();
					_cache = os::renderer_base::get().new_framebuffer(
						static_cast<size_t>(maximum_width),
						static_cast<size_t>(ctx->num_lines() * lh * scale)
					);

					os::renderer_base::get().begin_framebuffer(_cache);
					os::renderer_base::get().push_matrix(matd3x3::scale(vec2d(), vec2d(scale, scale)));
					auto lit = ctx->begin();
					double cury = 0.0;
					text_theme_data::char_iterator tit = ctx->get_text_theme().get_iter_at(caret_position());
					ui::font_family::baseline_info bi = fnt.get_baseline_info();
					for (
						size_t i = 0;
						lit != ctx->end();
						ctx->get_text_theme().incr_iter(tit, caret_position(++i, 0)), ++lit, cury += lh
						) {
						int sy = static_cast<int>(std::round(cury));
						size_t col = 0;
						for (
							ui::line_character_iterator it(lit->content, fnt, ce->get_tab_width());
							it.next(tit.current_theme.style);
							ctx->get_text_theme().incr_iter(tit, caret_position(i, ++col))
							) {
							if (is_graphical_char(it.current_char())) {
								os::renderer_base::get().draw_quad(
									it.current_char_entry().placement.translated(vec2d(it.char_left(), sy + bi.get(tit.current_theme.style))),
									rectd(), tit.current_theme.color, 0
								);
								//os::renderer_base::get().draw_character(
								//	it.current_char_entry().texture,
								//	vec2d(it.char_left(), sy + bi.get(tit.current_theme.style)) +
								//	it.current_char_entry().placement.xmin_ymin(),
								//	tit.current_theme.color
								//);
							}
						}
					}
					os::renderer_base::get().pop_matrix();
					os::renderer_base::get().end_framebuffer();
				} else {
					os::renderer_base::get().delete_framebuffer(_cache);
				}
				auto dur = std::chrono::high_resolution_clock::now() - start_time;
				CP_INFO("minimap rendering cost ", std::chrono::duration<double, std::milli>(dur).count(), "ms");
			}

			void _on_added() override {
				_tok = (
					_get_editor()->display_changed +=
					std::bind(&codebox_minimap::_on_editor_display_changed, this)
					);
			}
			void _on_removing() override {
				_get_editor()->display_changed -= _tok;
			}

			os::framebuffer _cache;
			event<void>::token _tok;
		};
	}
}
