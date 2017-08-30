#pragma once

#include <deque>

#include "editor.h"
#include "../../ui/draw.h"

namespace codepad {
	namespace editor {
		namespace code {
			class line_number : public component { // TODO width measurement inaccurate
			public:
				vec2d get_desired_size() const override {
					size_t ln = _get_editor()->get_context()->num_lines(), w = 0;
					for (; ln > 0; ++w, ln /= 10) {
					}
					return vec2d(get_padding().width() + static_cast<double>(w) * editor::get_font().normal->max_width(), 0.0);
				}
			protected:
				void _render() const override {
					editor *editor = _get_editor();
					double lh = editor->get_line_height();
					std::pair<size_t, size_t> be = editor->get_visible_lines();
					editor::folding_info::const_iterator it = editor->get_folding_info().lower_bound(
						editor::fold_region(caret_position(be.first, 0), caret_position(be.first, 0))
					);
					double cury =
						get_client_region().ymin - _get_box()->get_vertical_position() +
						static_cast<double>(editor->unfolded_to_folded_linebeg(be.first)) * lh;
					for (size_t curi = be.first; curi < be.second; ++curi, cury += lh) {
						str_t curlbl = to_str(curi + 1);
						double w = ui::text_renderer::measure_plain_text(curlbl, editor::get_font().normal.get()).x;
						ui::text_renderer::render_plain_text(
							curlbl, editor::get_font().normal.get(),
							vec2d(get_client_region().xmax - w, cury), colord()
						);
						while (it != editor->get_folding_info().end() && it->first.line == curi) {
							curi = it->second.line;
							++it;
						}
					}
				}
			};

			class minimap : public component {
			public:
				constexpr static double target_font_height = 2.0, page_rendering_time_redline = 30.0;
				constexpr static size_t
					maximum_width = 150, minimum_page_size = 500, // in pixels
					expected_triangles_per_line = 200;

				vec2d get_desired_size() const override {
					codebox *cb = _get_box();
					double totw = cb->get_client_region().width() - cb->get_editor()->get_margin().width();
					for (auto i = cb->get_components_left().begin(); i != cb->get_components_left().end(); ++i) {
						if (*i != this) {
							totw -= (*i)->get_desired_size().x + (*i)->get_margin().width();
						}
					}
					return vec2d(std::min<double>(
						static_cast<double>(maximum_width), totw * get_scale() / (1.0 + get_scale())
						), 0.0);
				}

				inline static void set_viewport_brush(const ui::basic_brush *b) {
					_viewport_brush = b;
				}
				inline static const ui::basic_brush *get_viewport_brush() {
					return _viewport_brush;
				}

				inline static double get_scale() {
					return target_font_height / editor::get_font().maximum_height();
				}
			protected:
				struct _page_cache {
					explicit _page_cache(const minimap *p) : parent(p) {
					}

					size_t page_end = 0;
					std::map<size_t, os::framebuffer> pages;
					const minimap *parent;

					struct line_top_cache {
						explicit line_top_cache(double top) : page_top(top), rounded_page_top(std::round(page_top)) {
						}

						double get_render_line_top(double y, double baseline) const {
							return (std::round((y + baseline) * get_scale() + page_top) - rounded_page_top) / get_scale();
						}
						void refresh(double y) {
							normal = get_render_line_top(y, baselines.get(font_style::normal));
							bold = get_render_line_top(y, baselines.get(font_style::bold));
							italic = get_render_line_top(y, baselines.get(font_style::italic));
							bold_italic = get_render_line_top(y, baselines.get(font_style::bold_italic));
						}
						double get(font_style fs) {
							switch (fs) {
							case font_style::normal:
								return normal;
							case font_style::bold:
								return bold;
							case font_style::italic:
								return italic;
							case font_style::bold_italic:
								return bold_italic;
							}
							assert_true_logical(false, "invalid font style");
							return 0.0;
						}

						ui::font_family::baseline_info baselines{editor::get_font().get_baseline_info()};
						double page_top, rounded_page_top, normal, bold, italic, bold_italic;
					};
					// accepts folded line numbers
					void render_page(size_t s, size_t pe) {
						auto start_time = std::chrono::high_resolution_clock::now();
						const editor *ce = parent->_get_editor();
						double lh = ce->get_line_height();
						line_top_cache lct(static_cast<double>(s) * lh * get_scale());
						bool needrefresh = true;

						// TODO improve drawing method
						os::framebuffer buf = os::renderer_base::get().new_framebuffer(
							maximum_width, static_cast<size_t>(std::ceil(lh * get_scale() * static_cast<double>(pe - s)))
						);
						os::renderer_base::get().begin_framebuffer(buf);
						os::renderer_base::get().push_matrix(matd3x3::scale(vec2d(), get_scale()));
						ui::render_batch rb;
						rb.reserve(expected_triangles_per_line * (pe - s));
						rendering_iterator<true> it(
							*ce,
							parent->_get_editor()->folded_to_unfolded_linebeg(s),
							parent->_get_editor()->folded_to_unfolded_linebeg(pe)
						);
						for (it.begin(); !it.ended(); it.next()) {
							if (!it.char_iter().is_line_break()) {
								if (needrefresh) {
									lct.refresh(it.char_iter().y_offset());
								}
								const ui::line_character_iterator &ci = it.char_iter().character_info();
								const text_theme_data::char_iterator &ti = it.char_iter().theme_info();
								if (is_graphical_char(ci.current_char())) {
									rectd crec = ci.current_char_entry().placement.translated(vec2d(
										ci.char_left(), lct.get(ti.current_theme.style)
									));
									rb.add_quad(crec, rectd(), ti.current_theme.color);

									//os::renderer_base::get().draw_character(
									//	ci.current_char_entry().texture,
									//	vec2d(
									//		ci.char_left(),
									//		(std::round(
									//		(it.char_iter().y_offset() + bi.get(ti.current_theme.style)) * get_scale() + buftop
									//		) - rbtop) / get_scale()
									//	) + ci.current_char_entry().placement.xmin_ymin(),
									//	ti.current_theme.color
									//);
								}
								needrefresh = false;
							} else {
								needrefresh = true;
							}
						}
						rb.draw(0);
						os::renderer_base::get().pop_matrix();
						os::renderer_base::get().end();

						double dur = std::chrono::duration<double, std::milli>(
							std::chrono::high_resolution_clock::now() - start_time
							).count();
						if (dur > page_rendering_time_redline) {
							logger::get().log_warning(CP_HERE, "minimap rendering [", s, ", ", pe, ") cost ", dur, "ms");
						}
						pages.insert(std::make_pair(s, std::move(buf)));
					}
					void restart() {
						pages.clear();
						editor *editor = parent->_get_editor();
						std::pair<size_t, size_t> be = parent->_get_visible_lines_folded();
						double slh = editor->get_line_height() * get_scale();
						size_t
							numlines = editor->get_num_lines_with_folding(),
							pgsize = std::max(be.second - be.first, static_cast<size_t>(minimum_page_size / slh) + 1),
							page_beg = 0;
						page_end = numlines;
						if (pgsize < numlines) {
							if (be.first + be.second < pgsize) {
								page_end = pgsize;
							} else if (be.first + be.second + pgsize > numlines * 2) {
								page_beg = numlines - pgsize;
							} else {
								page_beg = (be.first + be.second - pgsize) / 2;
								page_end = page_beg + pgsize;
							}
						}
						render_page(page_beg, page_end);
					}
					void make_valid() {
						if (pages.empty()) {
							restart();
						} else {
							codebox *box = parent->_get_box();
							editor *editor = box->get_editor();
							std::pair<size_t, size_t> be = parent->_get_visible_lines_folded();
							size_t page_beg = pages.begin()->first;
							if (be.first >= page_beg && be.second <= page_end) {
								return;
							}
							size_t page_lines = std::max(
								be.second - be.first, static_cast<size_t>(minimum_page_size / (editor->get_line_height() * get_scale())) + 1
							);
							if (be.first + page_lines < page_beg || page_end + page_lines < be.second) {
								restart();
							} else {
								if (be.first < page_beg) {
									size_t frontline = 0;
									if (page_lines < page_beg) {
										frontline = page_beg - page_lines;
									}
									render_page(frontline, page_beg);
								}
								if (be.second > page_end) {
									size_t backline = std::min(editor->get_context()->num_lines(), page_end + page_lines);
									render_page(page_end, backline);
									page_end = backline;
								}
							}
						}
					}
				};

				void _on_prerender() override {
					if (!_cachevalid) {
						_pgcache.make_valid();
						_cachevalid = true;
					}
					component::_on_prerender();
				}
				void _render() const override {
					std::pair<size_t, size_t> vlines = _get_visible_lines_folded();
					double slh = _get_editor()->get_line_height() * get_scale();
					rectd clnrgn = get_client_region();
					clnrgn.xmax = clnrgn.xmin + maximum_width;
					clnrgn.ymin = std::round(clnrgn.ymin - _get_y_offset());
					auto ibeg = --_pgcache.pages.upper_bound(vlines.first), iend = _pgcache.pages.lower_bound(vlines.second);
					for (auto i = ibeg; i != iend; ++i) {
						rectd crgn = clnrgn;
						crgn.ymin = std::round(crgn.ymin + slh * static_cast<double>(i->first));
						crgn.ymax = crgn.ymin + static_cast<double>(i->second.get_height());
						os::renderer_base::get().draw_quad(crgn, rectd(0.0, 1.0, 0.0, 1.0), colord(), i->second.get_texture());
					}
					if (_viewport_brush != nullptr) {
						_viewport_brush->fill_rect(_get_viewport_rect());
					}
				}

				double _get_y_offset() const {
					const codebox *cb = _get_box();
					const editor *editor = cb->get_editor();
					size_t nlines = editor->get_num_lines_with_folding();
					double
						lh = editor->get_line_height(),
						vh = get_client_region().height(),
						maxh = static_cast<double>(nlines) * lh - vh,
						maxvh = static_cast<double>(nlines) * lh * get_scale() - vh,
						perc = cb->get_vertical_position() / maxh;
					perc = clamp(perc, 0.0, 1.0);
					return std::max(0.0, perc * maxvh);
				}
				rectd _get_viewport_rect() const {
					const editor *e = _get_editor();
					rectd clnrgn = get_client_region();
					return rectd::from_xywh(
						clnrgn.xmin, clnrgn.ymin - _get_y_offset() + _get_box()->get_vertical_position() * get_scale(),
						e->get_client_region().width() * get_scale(), get_client_region().height() * get_scale()
					);
				}
				std::pair<size_t, size_t> _get_visible_lines_folded() const {
					const editor *editor = _get_editor();
					double lh = editor->get_line_height() * get_scale(), ys = _get_y_offset();
					return std::make_pair(static_cast<size_t>(ys / lh), std::min(
						static_cast<size_t>((ys + get_client_region().height()) / lh) + 1, editor->get_num_lines_with_folding()
					));
				}

				void _on_added() override {
					component::_on_added();
					_vc_tok = (_get_editor()->content_visual_changed += std::bind(&minimap::_on_editor_visual_changed, this));
					_fold_tok = (_get_editor()->folding_changed += std::bind(&minimap::_on_editor_visual_changed, this));
					_vpos_tok = (_get_box()->vertical_viewport_changed += [this](value_update_info<double>&) {
						_on_viewport_changed();
					});
				}
				void _on_removing() override {
					_get_editor()->content_visual_changed -= _vc_tok;
					_get_editor()->folding_changed -= _fold_tok;
					_get_box()->vertical_viewport_changed -= _vpos_tok;
					component::_on_removing();
				}

				void _on_viewport_changed() {
					_cachevalid = false;
				}
				void _on_editor_visual_changed() {
					_pgcache.pages.clear();
					_cachevalid = false;
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
								(info.position.y - get_client_region().ymin + _get_y_offset()) / get_scale() - 0.5 * ch,
								static_cast<double>(edt->get_num_lines_with_folding()) * edt->get_line_height() - ch
							));
						}
					}
					component::_on_mouse_down(info);
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
					component::_on_mouse_up(info);
				}
				void _on_mouse_move(ui::mouse_move_info &info) override {
					if (_dragging) {
						const editor *edt = _get_editor();
						double
							yp = info.new_pos.y + _dragoffset - get_client_region().ymin,
							toth =
							static_cast<double>(edt->get_num_lines_with_folding()) * edt->get_line_height() -
							get_client_region().height(),
							totch = std::min(get_client_region().height() * (1.0 - get_scale()), toth * get_scale());
						_get_box()->set_vertical_position(toth * yp / totch);
					}
					component::_on_mouse_move(info);
				}
				void _on_capture_lost() override {
					_on_mouse_lbutton_up();
					component::_on_capture_lost();
				}

				_page_cache _pgcache{this};
				bool _dragging = false, _cachevalid = true;
				double _dragoffset = 0.0;
				event<void>::token _vc_tok, _fold_tok;
				event<value_update_info<double>>::token _vpos_tok;

				static const ui::basic_brush *_viewport_brush;
			};
		}
	}
}
