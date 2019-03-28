// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Additional components of a \ref codepad::editors::code::editor.

#include "contents_region.h"
#include "rendering.h"
#include "../../ui/draw.h"

namespace codepad::editors::code {
	/// Displays a the line number for each line.
	class line_number_display : public ui::element {
	public:
		/// Returns the width of the longest line number.
		ui::size_allocation get_desired_width() const override {
			if (contents_region *edt = component_helper::get_contents_region(*this)) {
				size_t ln = edt->get_document()->num_lines(), w = 0;
				for (; ln > 0; ++w, ln /= 10) {
				}
				double maxw = contents_region::get_font().normal->get_max_width_charset(U"0123456789");
				return ui::size_allocation(get_padding().width() + static_cast<double>(w) * maxw, true);
			}
			return ui::size_allocation(0, true);
		}

		/// Returns the default class of elements of type \ref line_number_display.
		inline static str_view_t get_default_class() {
			return CP_STRLIT("line_number_display");
		}
	protected:
		info_event<>::token _vis_change_tok; ///< The token used to listen to \ref contents_region::editing_visual_changed.

		/// Registers \ref _vis_change_tok if a \ref contents_region can be found.
		void _register_handlers() {
			if (contents_region *edt = component_helper::get_contents_region(*this)) {
				_vis_change_tok = (edt->editing_visual_changed += [this]() {
					// when the content is modified, it is possible that the number of digits is changed,
					// so we recalculate layout here
					_on_desired_size_changed(true, false);
				});
			}
		}
		/// Registers for \ref contents_region::content_modified.
		void _on_added_to_parent() override {
			element::_on_added_to_parent();
			_register_handlers();
		}
		/// Calls \ref _register_handlers() if necessary.
		void _on_logical_parent_constructed() override {
			element::_on_logical_parent_constructed();
			if (!_vis_change_tok.valid()) {
				_register_handlers();
			}
		}

		/// Renders all visible line numbers.
		void _custom_render() override {
			if (auto&&[box, edt] = component_helper::get_core_components(*this); edt) {
				const view_formatting &fmt = edt->get_formatting();
				double
					lh = edt->get_line_height(),
					ybeg = box->get_vertical_position() - edt->get_padding().top,
					yend = ybeg + edt->get_layout().height();
				size_t
					fline = static_cast<size_t>(std::max(ybeg / lh, 0.0)),
					eline = static_cast<size_t>(yend / lh) + 1;
				rectd client = get_client_region();
				double cury = client.ymin - ybeg + static_cast<double>(fline) * lh;
				for (size_t curi = fline; curi < eline; ++curi, cury += lh) {
					size_t line = fmt.get_folding().folded_to_unfolded_line_number(curi);
					auto lineinfo = fmt.get_linebreaks().get_line_info(line);
					if (lineinfo.first.entry == edt->get_document()->get_linebreaks().end()) {
						break; // when after the end of the document
					}
					if (lineinfo.first.first_char >= lineinfo.second.prev_chars) { // ignore soft linebreaks
						str_t curlbl = std::to_string(1 + line - lineinfo.second.prev_softbreaks);
						double w = ui::text_renderer::measure_plain_text(curlbl, contents_region::get_font().normal).x;
						ui::text_renderer::render_plain_text(
							curlbl, *contents_region::get_font().normal,
							vec2d(client.xmax - w, cury), colord() // TODO customizable color
						);
					}
				}
			}
		}
	};

	/// Displays a minimap of the code, similar to that of sublime text.
	class minimap : public ui::element {
	public:
		/// The maximum amount of time allowed for rendering a page (i.e., an entry of \ref _page_cache).
		constexpr static std::chrono::duration<double> page_rendering_time_redline{0.03};
		constexpr static size_t minimum_page_size = 500; /// Maximum height of a page, in pixels.

		/// Returns the default width, which is proportional to that of the \ref contents_region.
		ui::size_allocation get_desired_width() const override {
			return ui::size_allocation(get_scale(), false);
		}

		/// Returns the scale of the text based on \ref _target_height.
		inline static double get_scale() {
			return _target_height / contents_region::get_font().maximum_height();
		}

		/// Sets the desired font height of minimaps. Note that font height is different from line height.
		inline static void set_target_font_height(double h) {
			_target_height = h;
		}
		/// Returns the current font height of minimaps.
		inline static double get_target_font_height() {
			return _target_height;
		}

		/// Returns the default class of elements of type \ref minimap.
		inline static str_t get_default_class() {
			return CP_STRLIT("minimap");
		}
		/// Returns the default class of the minimap's viewport.
		inline static str_view_t get_viewport_class() {
			return CP_STRLIT("minimap_viewport");
		}
	protected:
		/// Caches rendered pages so it won't be necessary to render large pages of text frequently.
		struct _page_cache {
			constexpr static size_t minimum_width = 50; ///< The minimum width of a page.
			constexpr static double
				/// Factor used to enlarge the width of pages when the actual width exceeds the page width.
				enlarge_factor = 1.5,
				/// If the actual width is less than this times page width, then page width is shrunk to fit the
				/// actual width.
				shirnk_threshold = 0.5;

			/// Constructor. Sets the associated \ref minimap of this cache.
			explicit _page_cache(const minimap &p) : _parent(&p) {
			}

			/// Clears all cached pages, and re-renders the currently visible page immediately. To render this page
			/// on demand, simply clear \ref pages and call \ref invalidate().
			void restart() {
				pages.clear();
				if (contents_region *edt = component_helper::get_contents_region(*_parent)) {
					std::pair<size_t, size_t> be = _parent->_get_visible_visual_lines();
					double slh = edt->get_line_height() * get_scale();
					size_t
						numlines = edt->get_num_visual_lines(),
						pgsize = std::max(be.second - be.first, static_cast<size_t>(minimum_page_size / slh) + 1),
						page_beg = 0;
					_page_end = numlines;
					if (pgsize < numlines) { // the viewport is smaller than one page
						if (be.first + be.second < pgsize) { // at the top
							_page_end = pgsize;
						} else if (be.first + be.second + pgsize > numlines * 2) { // at the bottom
							page_beg = numlines - pgsize;
						} else { // middle
							page_beg = (be.first + be.second - pgsize) / 2;
							_page_end = page_beg + pgsize;
						}
					}
					_render_page(page_beg, _page_end); // render the visible page
				}
			}
			/// Ensures that all visible pages have been rendered. If \ref pages is empty, calls \ref restart;
			/// otherwise checks if new pages need to be rendered.
			void prepare() {
				if (_ready) {
					return;
				}
				if (pages.empty()) {
					restart();
				} else {
					if (contents_region *edt = component_helper::get_contents_region(*_parent)) {
						std::pair<size_t, size_t> be = _parent->_get_visible_visual_lines();
						size_t page_beg = pages.begin()->first;
						if (be.first >= page_beg && be.second <= _page_end) { // all are visible
							return;
						}
						size_t min_page_lines = static_cast<size_t>(
							minimum_page_size / (edt->get_line_height() * get_scale())
							) + 1,
							// the number of lines in the page about to be rendered
							page_lines = std::max(be.second - be.first, min_page_lines);
						if (be.first + page_lines < page_beg || _page_end + page_lines < be.second) {
							// too far away from already rendered region, reset cache
							restart();
						} else {
							if (be.first < page_beg) { // render one page before the first one
								// if the page before is not large enough, make it as large as min_page_lines
								size_t frontline = std::max(page_beg, min_page_lines) - min_page_lines;
								// at least the first visible line is rendered
								_render_page(std::min(be.first, frontline), page_beg);
							}
							if (be.second > _page_end) { // render one page after the last one
								// if not large enough, make it as large as min_page_lines
								size_t backline = std::min(edt->get_num_visual_lines(), _page_end + min_page_lines);
								// at least the last visible line is rendered
								backline = std::max(be.second, backline);
								_render_page(_page_end, backline);
								_page_end = backline; // set _page_end
							}
						}
					}
				}
				_ready = true;
			}
			/// Marks this cache as not ready so that it'll be updated next time \ref prepare() is called.
			void invalidate() {
				_ready = false;
			}

			/// Called when the width of the \ref minimap has changed to update \ref _width.
			void on_width_changed(double w) {
				w += 1.0; // add 1 to avoid rounding issues
				if (w > _width) {
					do {
						_width = static_cast<size_t>(_width * enlarge_factor);
					} while (w > _width);
					logger::get().log_verbose(CP_HERE, "minimap width extended to ", _width);
					pages.clear();
					invalidate();
				} else if (_width > minimum_width && w < shirnk_threshold * _width) {
					_width = std::max(minimum_width, static_cast<size_t>(std::ceil(w)));
					logger::get().log_verbose(CP_HERE, "minimap width shrunk to ", _width);
				}
			}

			/// The cached pages. The keys are the indices of each page's first line, and the values are
			/// corresponding \ref os::frame_buffer "framebuffers".
			std::map<size_t, ui::frame_buffer> pages;
		protected:
			/// The index past the end of the range of lines that has been rendered and stored in \ref pages.
			size_t
				_page_end = 0,
				_width = minimum_width; ///< The width of all pages, in pixels.
			const minimap *_parent = nullptr; ///< The associated \ref minimap.
			/// Marks whether this cache is ready for rendering the currently visible portion of the document.
			bool _ready = false;

			/// Renders characters in a specific way so that they are more visible in the minimap.
			struct _char_renderer {
			public:
				/// Initializes the batch renderer and \ref _scale.
				_char_renderer(ui::atlas &atl, double scale) : _renderer(atl), _scale(scale) {
				}

				/// Adds the given character to the \ref ui::atlas::batch_renderer.
				void add_character(const ui::font::entry &entry, vec2d position, colord color) {
					rectd place = entry.placement.translated(
						position
					).coordinates_scaled(_scale).fit_grid_enlarge<double>();
					place.xmin = std::max(place.xmin, _last_xmax);
					_renderer.add_sprite(entry.texture, place, color);
					_last_xmax = place.xmax;
				}
				/// Resets this \ref _char_renderer to start from the beginning of the line.
				void reset() {
					_last_xmax = 0.0;
				}

				/// Returns the position of the right boundary of the last renderered character, relative to the
				/// left of the first character.
				double get_xmax() const {
					return _last_xmax;
				}
				/// Returns a reference to \ref _renderer.
				ui::atlas::batch_renderer &get_renderer() {
					return _renderer;
				}
				/// \overload
				const ui::atlas::batch_renderer &get_renderer() const {
					return _renderer;
				}
			protected:
				ui::atlas::batch_renderer _renderer; ///< The \ref ui::atlas::batch_renderer.
				double
					_scale = 0.0, ///< The scale of the characters.
					_last_xmax = 0.0; ///< The position of the right boundary of the last character.
			};
			/// Renders the page specified by the range of lines, and inserts the result into \ref pages. Note that
			/// this function does not automatically set \ref _page_end.
			///
			/// \param s Index of the first visual line of the page.
			/// \param pe Index past the last visual line of the page.
			///
			/// \todo Correct Y offset of characters of different fonts.
			void _render_page(size_t s, size_t pe) {
				performance_monitor mon(CP_STRLIT("render_minimap_page"), page_rendering_time_redline);
				if (contents_region *edt = component_helper::get_contents_region(*_parent)) {
					double lh = edt->get_line_height(), scale = get_scale();

					ui::renderer_base &r = _parent->get_manager().get_renderer();
					ui::frame_buffer buf = r.new_frame_buffer(
						// add 1 because the starting position was floored instead of rounded
						_width, static_cast<size_t>(std::ceil(lh * scale * static_cast<double>(pe - s))) + 1
					);
					r.begin_frame_buffer(buf);
					{ // this scope ensures that batch_renderer::flush() is called
						const view_formatting &fmt = edt->get_formatting();
						size_t
							curvisline = s,
							firstchar = fmt.get_linebreaks().get_beginning_char_of_visual_line(
								fmt.get_folding().folded_to_unfolded_line_number(s)
							).first,
							plastchar = fmt.get_linebreaks().get_beginning_char_of_visual_line(
								fmt.get_folding().folded_to_unfolded_line_number(pe)
							).first;
						rendering_token_iterator<soft_linebreak_inserter, folded_region_skipper> it(
							std::make_tuple(std::cref(fmt.get_linebreaks()), firstchar),
							std::make_tuple(std::cref(fmt.get_folding()), firstchar),
							std::make_tuple(std::cref(*edt->get_document()), firstchar)
						);
						text_metrics_accumulator metrics(
							contents_region::get_font(), lh, edt->get_formatting().get_tab_width()
						);
						_char_renderer crend(contents_region::get_font().normal->get_manager().get_atlas(), scale);
						// reserve for the maximum possible number of quads
						crend.get_renderer().get_batch().reserve_quads((pe - s) * _width);
						while (it.get_position() < plastchar) {
							token_generation_result tok = it.generate();
							metrics.next<token_measurement_flags::defer_text_gizmo_measurement>(tok.result);
							if (std::holds_alternative<character_token>(tok.result)) {
								auto &chartok = std::get<character_token>(tok.result);
								if (is_graphical_char(chartok.value)) { // render one character
									crend.add_character(
										metrics.get_character().current_char_entry(),
										vec2d(metrics.get_character().char_left(), metrics.get_y()),
										chartok.color
									);
								}
							} else if (std::holds_alternative<linebreak_token>(tok.result)) {
								++curvisline;
								crend.reset();
							} else if (std::holds_alternative<text_gizmo_token>(tok.result)) {
								auto &texttok = std::get<text_gizmo_token>(tok.result);
								auto tokit = texttok.contents.begin();
								codepoint last = 0;
								vec2d pos(metrics.get_character().char_right(), metrics.get_y());
								double xdiff = 0.0;
								while (tokit != texttok.contents.end()) {
									codepoint cp;
									if (encodings::utf8::next_codepoint(tokit, texttok.contents.end(), cp)) {
										std::shared_ptr<const ui::font>
											fnt = texttok.font ? texttok.font : contents_region::get_font().normal;
										if (last != 0) {
											xdiff += fnt->get_kerning(last, cp).x;
										}
										auto &entry = fnt->get_char_entry(cp);
										crend.add_character(entry, vec2d(pos.x + xdiff, pos.y), texttok.color);
										xdiff += entry.advance;
										last = cp;
									} else {
										last = 0;
										logger::get().log_warning(CP_HERE, "invalid codepoint in text gizmo");
									}
								}
								metrics.get_modify_character().next_gizmo(xdiff);
							} else if (std::holds_alternative<image_gizmo_token>(tok.result)) {
								// TODO
							}
							if (crend.get_xmax() < _width) {
								it.update(tok.steps);
							} else { // skip right to the next line
								++curvisline;
								size_t
									pos = fmt.get_linebreaks().get_beginning_char_of_visual_line(
										fmt.get_folding().folded_to_unfolded_line_number(curvisline)
									).first;
								it.reposition(pos);
								metrics.next_line();
								crend.reset();
							}
						}
					}
					r.end();
					pages.insert(std::make_pair(s, std::move(buf)));
				}
			}
		};

		/// Updates \ref _viewport_cfg.
		void _on_update_visual_configurations(ui::animation_update_info &info) override {
			element::_on_update_visual_configurations(info);
			info.update_configuration(_viewport_cfg);
		}

		/// Checks and validates \ref _pgcache by calling \ref _page_cache::prepare.
		void _on_prerender() override {
			element::_on_prerender();
			_pgcache.prepare();
		}
		/// Renders all visible pages.
		void _custom_render() override {
			if (contents_region *edt = component_helper::get_contents_region(*this)) {
				std::pair<size_t, size_t> vlines = _get_visible_visual_lines();
				double slh = edt->get_line_height() * get_scale();
				rectd pagergn = get_client_region();
				pagergn.ymin = std::round(pagergn.ymin - _get_y_offset());
				auto
					ibeg = _pgcache.pages.upper_bound(vlines.first),
					iend = _pgcache.pages.lower_bound(vlines.second);
				if (ibeg != _pgcache.pages.begin()) {
					--ibeg;
				} else {
					logger::get().log_error(CP_HERE, "agnomaly in page range selection");
				}
				ui::renderer_base &r = get_manager().get_renderer();
				r.push_blend_function(ui::blend_function(
					ui::blend_factor::one, ui::blend_factor::one_minus_source_alpha
				));
				for (auto i = ibeg; i != iend; ++i) {
					rectd crgn = pagergn;
					crgn.xmax = crgn.xmin + i->second.get_texture().get_width();
					crgn.ymin = std::floor(crgn.ymin + slh * static_cast<double>(i->first));
					crgn.ymax = crgn.ymin + static_cast<double>(i->second.get_texture().get_height());
					ui::render_batch rb(r);
					rb.add_quad(crgn, rectd(0.0, 1.0, 0.0, 1.0), colord());
					rb.draw_and_discard(i->second.get_texture());
				}
				r.pop_blend_function();
				// render visible region indicator
				_viewport_cfg.render(r, _get_clamped_viewport_rect());
			}
		}

		/// Calculates and returns the vertical offset of all pages according to \ref editor::get_vertical_position.
		double _get_y_offset() const {
			if (auto &&[box, edt] = component_helper::get_core_components(*this); edt) {
				size_t nlines = edt->get_num_visual_lines();
				double
					lh = edt->get_line_height(),
					vh = get_client_region().height(),
					maxh = static_cast<double>(nlines) * lh - vh,
					maxvh = static_cast<double>(nlines) * lh * get_scale() - vh,
					perc = (box->get_vertical_position() - edt->get_padding().top) / maxh;
				perc = std::clamp(perc, 0.0, 1.0);
				return std::max(0.0, perc * maxvh);
			}
			return 0.0;
		}
		/// Returns the rectangle marking the \ref contents_region's visible region.
		rectd _get_viewport_rect() const {
			if (contents_region *edt = component_helper::get_contents_region(*this)) {
				rectd clnrgn = get_client_region();
				return rectd::from_xywh(
					clnrgn.xmin - edt->get_padding().left * get_scale(),
					clnrgn.ymin - _get_y_offset() + (
						editor::get_encapsulating(*this)->get_vertical_position() - edt->get_padding().top
						) * get_scale(),
					edt->get_layout().width() * get_scale(), clnrgn.height() * get_scale()
				);
			}
			return rectd();
		}
		/// Clamps the result of \ref _get_viewport_rect. The region is clamped so that its right border won't
		/// overflow when the \ref contents_region's width is large.
		rectd _get_clamped_viewport_rect() const {
			rectd r = _get_viewport_rect();
			r.xmin = std::max(r.xmin, get_client_region().xmin);
			r.xmax = std::min(r.xmax, get_client_region().xmax);
			return r;
		}
		/// Returns the range of lines that are visible in the \ref minimap.
		std::pair<size_t, size_t> _get_visible_visual_lines() const {
			if (contents_region *edt = component_helper::get_contents_region(*this)) {
				double scale = get_scale(), ys = _get_y_offset();
				return edt->get_visible_visual_lines(ys / scale, (ys + get_client_region().height()) / scale);
			}
			return {0, 0};
		}

		/// Changes the state of the visible region indicator.
		void _on_state_changed(value_update_info<ui::element_state_id> &info) override {
			_viewport_cfg.on_state_changed(get_state());
			element::_on_state_changed(info);
		}
		/// Notifies and invalidates \ref _pgcache.
		void _on_layout_changed() override {
			_pgcache.on_width_changed(get_layout().width());
			_pgcache.invalidate(); // invalidate no matter what since the height may have also changed
			element::_on_layout_changed();
		}

		/// Registers event handlers to update the minimap and viewport indicator automatically.
		void _register_handlers() {
			if (auto&&[box, edt] = component_helper::get_core_components(*this); edt) {
				_vis_tok = (edt->editing_visual_changed += [this]() {
					_on_editor_visual_changed();
				});
				box->vertical_viewport_changed += [this]() {
					_on_viewport_changed();
				};
			}
		}
		/// Calls \ref _register_handlers().
		void _on_added_to_parent() override {
			element::_on_added_to_parent();
		}
		/// Calls \ref _register_handlers() if necessary.
		void _on_logical_parent_constructed() override {
			if (!_vis_tok.valid()) {
				_register_handlers();
			}
		}

		/// Marks \ref _pgcache for update when the viewport has changed, to determine if more pages need to
		/// be rendered when \ref _on_prerender is called.
		void _on_viewport_changed() {
			_pgcache.invalidate();
		}
		/// Clears \ref _pgcache.
		void _on_editor_visual_changed() {
			_pgcache.pages.clear();
			_pgcache.invalidate();
		}

		/// If the user presses ahd holds the primary mouse button on the viewport, starts dragging it; otherwise,
		/// if the user presses the left mouse button, jumps to the corresponding position.
		void _on_mouse_down(ui::mouse_button_info &info) override {
			element::_on_mouse_down(info);
			if (info.button == ui::mouse_button::primary) {
				if (auto&&[box, edt] = component_helper::get_core_components(*this); edt) {
					rectd rv = _get_viewport_rect();
					if (rv.contains(info.position)) {
						_dragoffset = rv.ymin - info.position.y;
						get_window()->set_mouse_capture(*this);
						_dragging = true;
					} else {
						rectd client = get_client_region();
						double ch = client.height();
						box->set_vertical_position(std::min(
							(info.position.y - client.ymin + _get_y_offset()) / get_scale() - 0.5 * ch,
							static_cast<double>(edt->get_num_visual_lines()) * edt->get_line_height() - ch
						) + edt->get_padding().top);
					}
				}
			}
		}
		/// Stops dragging.
		void _on_mouse_up(ui::mouse_button_info &info) override {
			element::_on_mouse_up(info);
			if (_dragging && info.button == ui::mouse_button::primary) {
				_dragging = false;
				get_window()->release_mouse_capture();
			}
		}
		/// If dragging, updates the position of the viewport.
		///
		/// \todo Small glitch when starting to drag the region when it's partially out of the minimap area.
		void _on_mouse_move(ui::mouse_move_info &info) override {
			element::_on_mouse_move(info);
			if (_dragging) {
				if (auto&&[box, edt] = component_helper::get_core_components(*this); edt) {
					rectd client = get_client_region();
					double
						scale = get_scale(),
						yp = info.new_position.y + _dragoffset - client.ymin,
						toth =
						static_cast<double>(edt->get_num_visual_lines()) * edt->get_line_height() - client.height(),
						totch = std::min(client.height() * (1.0 - scale), toth * scale);
					box->set_vertical_position(toth * yp / totch + edt->get_padding().top);
				}
			}
		}
		/// Stops dragging.
		void _on_capture_lost() override {
			element::_on_capture_lost();
			_dragging = false;
		}

		/// Sets the class of \ref _viewport_cfg.
		void _initialize(str_view_t cls, const ui::element_metrics &metrics) override {
			element::_initialize(cls, metrics);
			_can_focus = false;
			_viewport_cfg = ui::visual_configuration(
				get_manager().get_class_visuals().get_or_default(get_viewport_class())
			);
		}

		_page_cache _pgcache{*this}; ///< Caches rendered pages.
		info_event<>::token _vis_tok; ///< Used to listen to \ref contents_region::editing_visual_changed.
		ui::visual_configuration _viewport_cfg; ///< Used to render the visible region indicator.
		/// The offset of the mouse relative to the top border of the visible region indicator.
		double _dragoffset = 0.0;
		bool _dragging = false; ///< Indicates whether the visible region indicator is being dragged.

		static double _target_height; ///< The desired font height of minimaps.
	};
}
