#pragma once

#include <deque>

#include "editor.h"
#include "../../ui/draw.h"

namespace codepad {
	namespace editor {
		namespace code {
			class line_number : public component {
			public:
				vec2d get_desired_size() const override {
					size_t ln = _get_editor()->get_context()->num_lines(), w = 0;
					for (; ln > 0; ++w, ln /= 10) {
					}
					return vec2d(get_padding().width() + static_cast<double>(w) * editor::get_font().maximum_width(), 0.0);
				}
			protected:
				void _render() const override {
					editor *editor = _get_editor();
					double lh = editor->get_line_height(), pos = _get_box()->get_vertical_position();
					std::pair<size_t, size_t> be = editor->get_visible_lines();
					double cury = get_client_region().ymin - pos + static_cast<double>(be.first) * lh;
					for (size_t i = be.first + 1; i <= be.second; ++i, cury += lh) {
						str_t curlbl = to_str(i);
						double w = ui::text_renderer::measure_plain_text(curlbl, *editor::get_font().normal).x;
						ui::text_renderer::render_plain_text(
							curlbl, *editor::get_font().normal,
							vec2d(get_client_region().xmax - w, cury), colord()
						);
					}
				}
			};

			class minimap : public component {
			public:
				constexpr static double scale = 0.1, page_rendering_time_redline = 30.0;
				constexpr static size_t maximum_width = 150, minimum_page_size = 500; // in pixels

				vec2d get_desired_size() const override {
					codebox *cb = _get_box();
					return vec2d(std::min<double>(
						static_cast<double>(maximum_width), cb->get_actual_size().x * scale / (1.0 + scale)
						), 0.0);
				}

				inline static void set_viewport_brush(const ui::basic_brush *b) {
					_viewport_brush = b;
				}
				inline static const ui::basic_brush *get_viewport_brush() {
					return _viewport_brush;
				}
			protected:
				void _render() const override {
					std::pair<size_t, size_t> vlines = _get_visible_lines();
					double slh = _get_editor()->get_line_height() * scale;
					rectd clnrgn = get_client_region();
					clnrgn.xmax = clnrgn.xmin + maximum_width;
					clnrgn.ymin = std::round(clnrgn.ymin - _get_y_offset());
					auto ibeg = --_pgs.upper_bound(vlines.first), iend = _pgs.lower_bound(vlines.second);
					for (auto i = ibeg; i != iend; ++i) {
						rectd crgn = clnrgn;
						crgn.ymin = std::round(crgn.ymin + slh * i->first);
						crgn.ymax = crgn.ymin + i->second.get_height();
						os::renderer_base::get().draw_quad(crgn, rectd(0.0, 1.0, 0.0, 1.0), colord(), i->second.get_texture());
					}
					if (_viewport_brush != nullptr) {
						_viewport_brush->fill_rect(_get_viewport_rect());
					}
				}

				double _get_y_offset() const {
					const codebox *cb = _get_box();
					const editor *editor = cb->get_editor();
					size_t nlines = editor->get_context()->num_lines();
					double
						lh = editor->get_line_height(),
						vh = get_client_region().height(),
						maxh = nlines * lh - vh,
						maxvh = nlines * lh * scale - vh,
						perc = _get_box()->get_vertical_position() / maxh;
					perc = clamp(perc, 0.0, 1.0);
					return std::max(0.0, perc * maxvh);
				}
				rectd _get_viewport_rect() const {
					const editor *e = _get_editor();
					rectd clnrgn = get_client_region();
					return rectd::from_xywh(
						clnrgn.xmin, clnrgn.ymin - _get_y_offset() + _get_box()->get_vertical_position() * scale,
						e->get_client_region().width() * scale, get_client_region().height() * scale
					);
				}
				std::pair<size_t, size_t> _get_visible_lines() const {
					const editor *editor = _get_editor();
					double lh = editor->get_line_height() * scale, ys = _get_y_offset();
					return std::make_pair(static_cast<size_t>(ys / lh), std::min(
						static_cast<size_t>((ys + get_client_region().height()) / lh) + 1, editor->get_context()->num_lines()
					));
				}

				void _on_added() override {
					_vc_tok = (_get_editor()->content_visual_changed += std::bind(&minimap::_on_editor_visual_changed, this));
					_vpos_tok = (_get_box()->vertical_viewport_changed += [this](value_update_info<double>&) {
						_on_viewport_changed();
					});
				}
				void _on_removing() override {
					_get_editor()->content_visual_changed -= _vc_tok;
					_get_box()->vertical_viewport_changed -= _vpos_tok;
				}

				os::framebuffer _render_page(size_t s, size_t pe) const {
					auto start_time = std::chrono::high_resolution_clock::now();
					const editor *ce = _get_editor();
					double lh = ce->get_line_height(), buftop = s * lh * scale, rbtop = std::round(buftop);
					os::framebuffer buf = os::renderer_base::get().new_framebuffer(
						maximum_width, static_cast<size_t>(std::ceil(lh * scale * (pe - s)))
					);

					os::renderer_base::get().begin_framebuffer(buf);
					os::renderer_base::get().push_matrix(matd3x3::scale(vec2d(), vec2d(scale, scale)));

					ui::font_family::baseline_info bi = editor::get_font().get_baseline_info();
					// TODO improve drawing method
					ui::render_batch rb;
					std::vector<vec2d> poss;
					std::vector<colord> colors;
					character_rendering_iterator it(*ce, s, pe);
					for (it.begin(); !it.ended(); it.next_char()) {
						if (!it.is_line_break()) {
							const ui::line_character_iterator &ci = it.character_info();
							const text_theme_data::char_iterator &ti = it.theme_info();
							if (is_graphical_char(ci.current_char())) {
								rectd crec = ci.current_char_entry().placement.translated(vec2d(
									ci.char_left(),
									(std::round((it.y_offset() + bi.get(ti.current_theme.style)) * scale + buftop) - rbtop) / scale
								));
								rb.add_quad(crec, rectd(), ti.current_theme.color);

								//os::renderer_base::get().draw_character(
								//	ci.current_char_entry().texture,
								//	vec2d(ci.char_left(), ybeg + it.y_offset() + bi.get(ti.current_theme.style)) +
								//	ci.current_char_entry().placement.xmin_ymin(),
								//	ti.current_theme.color
								//);
							}
						}
					}
					rb.draw(0);
					os::renderer_base::get().pop_matrix();
					os::renderer_base::get().end_framebuffer();

					double dur = std::chrono::duration<double, std::milli>(
						std::chrono::high_resolution_clock::now() - start_time
						).count();
					if (dur > page_rendering_time_redline) {
						logger::get().log_info(CP_HERE, "minimap rendering [", s, ", ", pe, ") cost ", dur, "ms");
					}
					return buf;
				}

				void _restart_cache() {
					_pgs.clear();
					editor *editor = _get_editor();
					std::pair<size_t, size_t> be = _get_visible_lines();
					double slh = editor->get_line_height() * scale;
					size_t
						numlines = editor->get_context()->num_lines(),
						pgsize = std::max(be.second - be.first, static_cast<size_t>(minimum_page_size / slh) + 1),
						page_beg = 0;
					_page_end = numlines;
					if (pgsize < numlines) {
						if (be.first + be.second < pgsize) {
							_page_end = pgsize;
						} else if (be.first + be.second + pgsize > numlines * 2) {
							page_beg = numlines - pgsize;
						} else {
							page_beg = (be.first + be.second - pgsize) / 2;
							_page_end = page_beg + pgsize;
						}
					}
					logger::get().log_info(CP_HERE, "minimap cache cleared");
					_pgs.insert(std::make_pair(page_beg, _render_page(page_beg, _page_end)));
					invalidate_visual();
				}
				void _on_viewport_changed() {
					if (_pgs.empty()) {
						_restart_cache();
					} else {
						codebox *box = _get_box();
						editor *editor = box->get_editor();
						std::pair<size_t, size_t> be = _get_visible_lines();
						size_t page_beg = _pgs.begin()->first;
						if (be.first >= page_beg && be.second <= _page_end) {
							return;
						}
						size_t page_lines = std::max(
							be.second - be.first, static_cast<size_t>(minimum_page_size / (editor->get_line_height() * scale)) + 1
						);
						if (be.first + page_lines < page_beg || _page_end + page_lines < be.second) {
							_restart_cache();
						} else {
							if (be.first < page_beg) {
								size_t frontline = 0;
								if (page_lines < page_beg) {
									frontline = page_beg - page_lines;
								}
								_pgs.insert(std::make_pair(frontline, _render_page(frontline, page_beg)));
							}
							if (be.second > _page_end) {
								size_t backline = std::min(editor->get_context()->num_lines(), _page_end + page_lines);
								_pgs.insert(std::make_pair(_page_end, _render_page(_page_end, backline)));
								_page_end = backline;
							}
						}
					}
				}
				void _on_editor_visual_changed() {
					_restart_cache();
				}

				void _on_mouse_down(ui::mouse_button_info &info) override {
					if (info.button == os::input::mouse_button::left) {
						rectd rv = _get_viewport_rect();
						if (rv.contains(info.position)) {
							_dragoffset = rv.ymin - info.position.y;
							get_window()->set_mouse_capture(*this);
							_dragging = true;
						} else {
							double ch = get_client_region().height();
							const editor *edt = _get_editor();
							_get_box()->set_vertical_position(std::min(
								(info.position.y - get_client_region().ymin + _get_y_offset()) / scale - 0.5 * ch,
								edt->get_context()->num_lines() * edt->get_line_height() - ch
							));
						}
					}
				}
				void _on_mouse_lbutton_up() {
					if (_dragging) {
						_dragging = false;
						get_window()->release_mouse_capture();
					}
				}
				void _on_mouse_up(ui::mouse_button_info &info) override {
					if (info.button == os::input::mouse_button::left) {
						_on_mouse_lbutton_up();
					}
				}
				void _on_mouse_move(ui::mouse_move_info &info) override {
					if (_dragging) {
						const editor *edt = _get_editor();
						const text_context *ctx = edt->get_context();
						double
							yp = info.new_pos.y + _dragoffset - get_client_region().ymin,
							toth = ctx->num_lines() * edt->get_line_height() - get_client_region().height(),
							totch = std::min(get_client_region().height() * (1.0 - scale), toth * scale);
						_get_box()->set_vertical_position(toth * yp / totch);
					}
				}
				void _on_capture_lost() override {
					_on_mouse_lbutton_up();
				}

				size_t _page_end;
				std::map<size_t, os::framebuffer> _pgs;
				bool _dragging = false;
				double _dragoffset;
				event<void>::token _vc_tok;
				event<value_update_info<double>>::token _vpos_tok;

				static const ui::basic_brush *_viewport_brush;
			};
		}
	}
}
