#include "editor.h"

using namespace std;
using namespace codepad::ui;
using namespace codepad::os;

namespace codepad {
	namespace editor {
		namespace code {
			void editor::set_context(shared_ptr<text_context> nctx) {
				if (_ctx) {
					_ctx->modified -= _mod_tok;
					_ctx->visual_changed -= _tc_tok;
				}
				_ctx = move(nctx);
				_cset.reset();
				if (_ctx) {
					_mod_tok = (_ctx->modified += [this](modification_info &info) {
						_on_content_modified(info);
						});
					_tc_tok = (_ctx->visual_changed += [this]() {
						_on_content_visual_changed();
						});
					_fmt = _ctx->create_view_formatting();
				} else {
					_fmt = view_formatting();
				}
				_on_context_switch();
			}

			void editor::_custom_render() {
				performance_monitor mon(CP_HERE);
				rectd client = get_client_region();
				if (client.height() < 0.0) {
					return;
				}
				double lh = get_line_height(), layoutw = get_layout().width();
				pair<size_t, size_t> be = get_visible_lines();
				caret_set::container ncon;
				const caret_set::container *used = &_cset.carets;
				if (_selecting) {
					ncon = *used;
					used = &ncon;
					caret_set::add_caret(
						ncon, {{_mouse_cache.first, _sel_end}, caret_data(0.0, _mouse_cache.second)}
					);
				}
				font_family::baseline_info bi = get_font().get_baseline_info();

				renderer_base::get().push_matrix(matd3x3::translate(vec2d(
					round(client.xmin),
					round(
						client.ymin - _get_box()->get_vertical_position() +
						static_cast<double>(_fmt.get_folding().unfolded_to_folded_line_number(be.first)) * lh
					)
				)));
				size_t
					firstchar = _fmt.get_linebreaks().get_beginning_char_of_visual_line(be.first).first,
					plastchar = _fmt.get_linebreaks().get_beginning_char_of_visual_line(be.second).first;
				rendering_iterator<view_formatter, caret_renderer> it(
					make_tuple(cref(_fmt), firstchar),
					make_tuple(cref(*used), firstchar, plastchar),
					make_tuple(cref(*this), firstchar, plastchar)
				);
				for (it.begin(); !it.ended(); it.next()) {
					const character_rendering_iterator &cri = it.char_iter();
					if (!cri.is_hard_line_break()) {
						const character_metrics_accumulator &lci = cri.character_info();
						const text_theme_data::char_iterator &ti = cri.theme_info();
						if (is_graphical_char(lci.current_char())) {
							vec2d xpos =
								vec2d(lci.char_left(), round(cri.rounded_y_offset() + bi.get(ti.current_theme.style))) +
								lci.current_char_entry().placement.xmin_ymin();
							if (xpos.x < layoutw) {
								renderer_base::get().draw_character(
									lci.current_char_entry().texture, xpos, ti.current_theme.color
								);
							}
						}
					}
				}
				if (_get_appearance().folding_gizmo.texture) {
					render_batch batch;
					for (const rectd &r : it.get_addon<view_formatter>().get_fold_region_skipper().get_gizmos()) {
						batch.add_quad(r, rectd(0.0, 1.0, 0.0, 1.0), colord());
					}
					batch.draw(*_get_appearance().folding_gizmo.texture);
				}
				_selst.update();
				for (const auto &selrgn : it.get_addon<caret_renderer>().get_selection_rects()) {
					for (const auto &rgn : selrgn) {
						_selst.render(rgn);
					}
				}
				if (!_selst.stationary()) {
					invalidate_visual();
				}
				if (_caretst.update_and_render_multiple(it.get_addon<caret_renderer>().get_caret_rects())) {
					invalidate_visual();
				}
				renderer_base::get().pop_matrix();
			}

			vector<size_t> editor::_recalculate_wrapping_region(size_t beg, size_t end) const {
				rendering_iterator<fold_region_skipper> it(
					make_tuple(cref(_fmt.get_folding()), beg),
					make_tuple(cref(*this), beg, _ctx->num_chars())
				);
				vector<size_t> poss;
				bool lb = true;
				it.char_iter().switching_char += [this, &it, &lb, &poss](switch_char_info &info) {
					if (info.is_jump) {
						if (
							it.char_iter().character_info().prev_char_right() + fold_region_skipper::gizmo_size >
							_view_width && !lb
							) {
							poss.push_back(it.char_iter().current_position());
						}
						lb = false;
					}
				};
				for (
					it.begin();
					!it.ended() && (
						it.char_iter().current_position() <= end ||
						!it.char_iter().is_hard_line_break()
						);
					it.next()
					) {
					if (
						!lb && !it.char_iter().is_hard_line_break() &&
						it.char_iter().character_info().char_right() > _view_width
						) {
						it.char_iter().insert_soft_linebreak();
						poss.push_back(it.char_iter().current_position());
						lb = false;
					} else {
						lb = it.char_iter().is_hard_line_break();
					}
				}
				return poss;
			}

			double editor::_get_caret_pos_x_unfolded_linebeg(size_t line, size_t position) const {
				size_t begc = _fmt.get_linebreaks().get_beginning_char_of_visual_line(line).first;
				rendering_iterator<fold_region_skipper> iter(
					make_tuple(cref(_fmt.get_folding()), begc),
					make_tuple(cref(*this), begc, _ctx->num_chars())
				);
				for (iter.begin(); iter.char_iter().current_position() < position; iter.next()) {
				}
				return iter.char_iter().character_info().prev_char_right();
			}

			std::pair<size_t, bool> editor::_hit_test_unfolded_linebeg(size_t line, double x) const {
				size_t begc = _fmt.get_linebreaks().get_beginning_char_of_visual_line(line).first;
				rendering_iterator<view_formatter> iter(
					make_tuple(cref(_fmt), begc),
					make_tuple(cref(*this), begc, _ctx->num_chars())
				);
				size_t lastpos = _fmt.get_linebreaks().get_beginning_char_of_visual_line(line).first;
				bool stop = false, lastjmp = false, lastslb = false;
				iter.char_iter().switching_char += [&](switch_char_info &pi) {
					if (!stop) {
						if (lastjmp) {
							double midv = (
								iter.char_iter().character_info().prev_char_right() +
								iter.char_iter().character_info().char_left()
								) * 0.5;
							if (x < midv) {
								lastjmp = false;
								stop = true;
								return;
							}
							lastpos = iter.char_iter().current_position();
						}
						lastjmp = pi.is_jump;
						if (!pi.is_jump) {
							double midv = (
								iter.char_iter().character_info().char_left() +
								iter.char_iter().character_info().char_right()
								) * 0.5;
							if (x < midv) {
								stop = true;
								return;
							}
							lastpos = pi.next_position;
						} else {
							lastpos = iter.char_iter().current_position();
						}
					}
				};
				iter.char_iter().switching_line += [&](switch_line_info&) {
					if (!stop) {
						lastslb = stop = true;
					}
				};
				for (iter.begin(); !stop && !iter.char_iter().is_hard_line_break(); iter.next()) {
				}
				if (lastjmp) {
					double midv = (
						iter.char_iter().character_info().prev_char_right() +
						iter.char_iter().character_info().char_left()
						) * 0.5;
					if (x > midv) {
						lastpos = iter.char_iter().current_position();
					}
				}
				return {lastpos, !lastslb};
			}

			void editor::_on_content_modified(modification_info &mi) {
				_gizmos.fixup(mi.caret_fixup);

				// fixup view
				// TODO improve performance
				_fmt.fixup_folding_positions(mi.caret_fixup);
				_fmt.set_softbreaks(_recalculate_wrapping_region(0, _ctx->num_chars()));

				if (mi.source != this) { // fixup carets
					caret_fixup_info::context cctx(mi.caret_fixup);
					caret_set nset;
					for (auto i = _cset.carets.begin(); i != _cset.carets.end(); ++i) {
						bool cfront = i->first.first < i->first.second;
						caret_selection cs(i->first.first, i->first.second);
						if (!cfront) {
							swap(cs.first, cs.second);
						}
						cs.first = mi.caret_fixup.fixup_caret_max(cs.first, cctx);
						cs.second = mi.caret_fixup.fixup_caret_min(cs.second, cctx);
						if (cs.first > cs.second) {
							cs.second = cs.first;
						}
						if (!cfront) {
							swap(cs.first, cs.second);
						}
						nset.add(caret_set::entry(cs, caret_data(0.0, i->second.softbreak_next_line)));
					}
					set_carets(nset);
				}

				content_modified.invoke();
				_on_content_visual_changed();
			}


			void editor::caret_renderer::_check_draw_caret(const character_rendering_iterator &it, bool softbreak) {
				double cx = it.character_info().char_left(), cy = it.y_offset();
				while (_curcidx != _caretposs.end() && it.current_position() == _curcidx->first) {
					if ((softbreak && _curcidx->second) || (_last_slb && !_curcidx->second)) {
						break;
					}
					if (it.is_hard_line_break()) {
						_caretrects.emplace_back(rectd::from_xywh(
							cx, cy, get_font().normal->get_char_entry(' ').advance, it.line_height()
						));
					} else {
						_caretrects.emplace_back(cx, it.character_info().char_right(), cy, cy + it.line_height());
					}
					++_curcidx;
				}
			}

			void editor::caret_renderer::_on_switching_char(const character_rendering_iterator &it, bool softbreak) {
				double cx = it.character_info().char_left(), cy = it.y_offset();
				if (_insel) {
					if (_minmax.second <= it.current_position()) {
						_selrgns.back().emplace_back(_lastl, cx, cy, cy + it.line_height());
						_insel = false;
						++_cur;
						if (_cur != _end) {
							_minmax = std::minmax({_cur->first.first, _cur->first.second});
						}
					}
				}
				_check_draw_caret(it, softbreak);
				if (!_insel) {
					_check_next(cx, it.current_position());
				}
				_last_slb = softbreak;
			}

			void editor::caret_renderer::switching_char(const character_rendering_iterator &it, switch_char_info&) {
				_on_switching_char(it, false);
			}

			void editor::caret_renderer::switching_line(
				const character_rendering_iterator &it, switch_line_info &info
			) {
				_on_switching_char(it, info.type == line_ending::none);
				if (_insel) {
					double finx = it.character_info().char_left();
					if (info.type != line_ending::none) {
						finx += it.character_info().get_font_family().get_by_style(
							it.theme_info().current_theme.style
						)->get_char_entry(' ').advance;
					}
					_selrgns.back().push_back(rectd(_lastl, finx, it.y_offset(), it.y_offset() + it.line_height()));
					_lastl = 0.0;
				}
			}


			void editor::gizmo_renderer::switching_char(const character_rendering_iterator &it, switch_char_info&) {
				// TODO
			}

			void editor::gizmo_renderer::switching_line(const character_rendering_iterator &it) {
				// TODO
			}
		}
	}
}
