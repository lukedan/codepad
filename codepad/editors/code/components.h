#pragma once

/// \file
/// Additional components of a \ref codepad::editor::code::codebox.

#include "editor.h"
#include "rendering.h"
#include "../../ui/draw.h"

namespace codepad::editor::code {
	/// Displays a the line number for each line.
	class line_number_display : public component {
	public:
		/// Returns the width of the longest line number.
		std::pair<double, bool> get_desired_width() const override {
			size_t ln = _get_editor()->get_document()->num_lines(), w = 0;
			for (; ln > 0; ++w, ln /= 10) {
			}
			double maxw = editor::get_font().normal->get_max_width_charset(U"0123456789");
			return {get_padding().width() + static_cast<double>(w) * maxw, true};
		}

		/// Returns the default class of elements of type \ref line_number_display.
		inline static str_t get_default_class() {
			return CP_STRLIT("line_number_display");
		}
	protected:
		event<void>::token _resizetk; ///< The token used to listen to \ref editor::content_modified.

		/// Register for \ref editor::content_modified.
		void _on_added() override {
			component::_on_added();
			_resizetk = (_get_box()->get_editor()->content_modified += [this]() {
				invalidate_layout();
				});
		}
		/// Unregisters from \ref editor::content_modified.
		void _on_removing() override {
			_get_box()->get_editor()->content_modified -= _resizetk;
			component::_on_removing();
		}

		/// Renders all visible line numbers.
		void _custom_render() override {
			editor *edt = _get_editor();
			const view_formatting &fmt = edt->get_formatting();
			double
				lh = edt->get_line_height(),
				ybeg = _get_box()->get_vertical_position() - edt->get_padding().top,
				yend = ybeg + edt->get_layout().height();
			size_t fline = static_cast<size_t>(std::max(ybeg / lh, 0.0)), eline = static_cast<size_t>(yend / lh) + 1;
			rectd client = get_client_region();
			double cury = client.ymin - ybeg + static_cast<double>(fline) * lh;
			for (size_t curi = fline; curi < eline; ++curi, cury += lh) {
				size_t line = fmt.get_folding().folded_to_unfolded_line_number(curi);
				auto lineinfo = fmt.get_linebreaks().get_line_info(line);
				if (lineinfo.first.entry == edt->get_document()->get_linebreak_registry().end()) {
					break; // when after the end of the document
				}
				if (lineinfo.first.first_char >= lineinfo.second.prev_chars) { // ignore soft linebreaks
					str_t curlbl = to_str(1 + line - lineinfo.second.prev_softbreaks);
					double w = ui::text_renderer::measure_plain_text(curlbl, editor::get_font().normal).x;
					ui::text_renderer::render_plain_text(
						curlbl, editor::get_font().normal,
						vec2d(client.xmax - w, cury), colord() // TODO customizable color
					);
				}
			}
		}
	};

	/// Displays a minimap of the code, similar to that of sublime text.
	class minimap : public component {
	public:
		/// The maximum amount of time allowed for rendering a page (i.e., an entry of \ref _page_cache).
		constexpr static double page_rendering_time_redline = 0.03;
		constexpr static size_t
			maximum_width = 150, /// Maximum width of the element, in pixels.
			minimum_page_size = 500; /// Maximum height of a page, in pixels.

		/// Returns the default width, which is proportional to that of the \ref editor.
		std::pair<double, bool> get_desired_width() const override {
			return {get_scale(), false};
		}

		/// Returns the scale of the text based on \ref _target_height.
		inline static double get_scale() {
			return _target_height / editor::get_font().maximum_height();
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
		inline static str_t get_viewport_class() {
			return CP_STRLIT("minimap_viewport");
		}
	protected:
		/// Caches rendered pages so it won't be necessary to render large pages of text frequently.
		struct _page_cache {
			/// Constructor. Sets the associated \ref minimap of this cache.
			explicit _page_cache(const minimap &p) : parent(&p) {
			}

			/// The index past the end of the range of lines that has been rendered and stored in \ref pages.
			size_t page_end = 0;
			/// The cached pages. The keys are the indices of each page's first line, and the values are
			/// corresponding \ref os::framebuffer "framebuffers".
			std::map<size_t, os::framebuffer> pages;
			const minimap *parent; ///< The associated \ref minimap.

			/// Caches the vertical offsets corresponding to all four \ref font_style "font styles" for a specific
			/// line. The offsets are calculated in a way so that the result are consistent wherever the pages are
			/// split.
			struct line_top_cache {
				/// Initializes all fields of the struct.
				///
				/// \param top The position of the page's upper boundary.
				explicit line_top_cache(double top) : page_top(top), floored_page_top(std::floor(page_top)) {
				}

				/// Calculates the vertical position given the position of the top of the line and the baseline
				/// corresponding to a \ref font_style.
				double get_render_line_top(double y, double baseline) const {
					return (std::round((y + baseline) * get_scale() + page_top) - floored_page_top) / get_scale();
				}
				/// Calls \ref get_render_line_top to recalculate the positions for all
				/// \ref font_style "font styles".
				void refresh(double y) {
					normal = get_render_line_top(y, baselines.get(font_style::normal));
					bold = get_render_line_top(y, baselines.get(font_style::bold));
					italic = get_render_line_top(y, baselines.get(font_style::italic));
					bold_italic = get_render_line_top(y, baselines.get(font_style::bold_italic));
				}
				/// Returns the position corresponding to the given \ref font_style.
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

				/// Stores the baselines of all fonts in a \ref ui::font_family.
				ui::font_family::baseline_info baselines{editor::get_font().get_baseline_info()};
				double
					page_top, ///< The position of the page's upper boundary.
					floored_page_top, ///< Floored position of the page's upper boundary.
					normal = 0.0, ///< The position corresponding to \ref font_style::normal.
					bold = 0.0, ///< The position corresponding to \ref font_style::bold.
					italic = 0.0, ///< The position corresponding to \ref font_style::italic.
					bold_italic = 0.0; ///< The position corresponding to \ref font_style::bold_italic.
			};
			/// Renders the page specified by the range of lines, and inserts the result into \ref pages. Note that
			/// this function does not automatically set \ref page_end.
			///
			/// \param s Index of the first line of the page, with word wrapping and folding enabled.
			/// \param pe Index past the last line of the page, with word wrapping and folding enabled.
			void render_page(size_t s, size_t pe) {
				performance_monitor mon(CP_HERE, page_rendering_time_redline);

				const editor *ce = parent->_get_editor();
				double lh = ce->get_line_height(), scale = get_scale();
				line_top_cache lct(static_cast<double>(s) * lh * scale);
				bool needrefresh = true; // indicates whether lct needs to be refreshed

				os::framebuffer buf = os::renderer_base::get().new_framebuffer(
					// add one for good luck
					maximum_width, static_cast<size_t>(std::ceil(lh * scale * static_cast<double>(pe - s))) + 1
					// actually, adding 1 is necessary because the start position was floored instead of rounded
				);
				os::renderer_base::get().begin_framebuffer(buf);
				const view_formatting &fmt = ce->get_formatting();
				size_t
					firstchar = fmt.get_linebreaks().get_beginning_char_of_visual_line(
						fmt.get_folding().folded_to_unfolded_line_number(s)
					).first,
					plastchar = fmt.get_linebreaks().get_beginning_char_of_visual_line(
						fmt.get_folding().folded_to_unfolded_line_number(pe)
					).first;
				rendering_iterator<view_formatter> it(
					std::make_tuple(std::cref(fmt), firstchar),
					std::make_tuple(std::cref(*ce), firstchar, plastchar)
				);
				// records the position of the right boundary of the last character that has just been rendered
				double lastx = 0.0;
				for (it.begin(); !it.ended(); it.next()) { // iterate over the range of chars
					if (it.get_addon<view_formatter>().get_soft_linebreak_inserter().is_linebreak(it.char_iter())) {
						needrefresh = true;
						lastx = 0.0;
					}
					if (!it.char_iter().is_hard_line_break()) {
						if (needrefresh) {
							// only refresh lct at a character, so that the y position is always the latest one
							lct.refresh(it.char_iter().y_offset());
							needrefresh = false;
						}
						const ui::character_metrics_accumulator &ci = it.char_iter().character_info();
						const text_theme_data::char_iterator &ti = it.char_iter().theme_info();
						if (is_graphical_char(ci.current_char())) {
							rectd crec = ci.current_char_entry().placement.translated(
								vec2d(ci.char_left(), lct.get(ti.current_theme.style))
							).coordinates_scaled(scale).fit_grid_enlarge<double>(); // snap to pixel borders
							crec.xmin = std::max(crec.xmin, lastx); // do not overlap with last character
							lastx = crec.xmax;
							if (crec.xmin < maximum_width) { // draw only visible characters
								os::renderer_base::get().draw_character_custom(
									ci.current_char_entry().texture,
									crec,
									ti.current_theme.color
								);
							}
						}
					}
				}
				os::renderer_base::get().end();
				pages.insert(std::make_pair(s, std::move(buf)));
			}
			/// Clears all cached pages, and re-renders the currently visible page.
			void restart() {
				pages.clear();
				editor *editor = parent->_get_editor();
				std::pair<size_t, size_t> be = parent->_get_visible_lines_folded();
				double slh = editor->get_line_height() * get_scale();
				size_t
					numlines = editor->get_num_visual_lines(),
					pgsize = std::max(be.second - be.first, static_cast<size_t>(minimum_page_size / slh) + 1),
					page_beg = 0;
				page_end = numlines;
				if (pgsize < numlines) { // the viewport is smaller than one page
					if (be.first + be.second < pgsize) { // at the top
						page_end = pgsize;
					} else if (be.first + be.second + pgsize > numlines * 2) { // at the bottom
						page_beg = numlines - pgsize;
					} else { // middle
						page_beg = (be.first + be.second - pgsize) / 2;
						page_end = page_beg + pgsize;
					}
				}
				render_page(page_beg, page_end); // render the visible page
			}
			/// Ensures that all visible pages have been rendered. If \ref pages is empty, calls \ref restart;
			/// otherwise checks if new pages needs to be rendered.
			void make_valid() {
				if (pages.empty()) {
					restart();
				} else {
					codebox *box = parent->_get_box();
					editor *editor = box->get_editor();
					std::pair<size_t, size_t> be = parent->_get_visible_lines_folded();
					size_t page_beg = pages.begin()->first;
					if (be.first >= page_beg && be.second <= page_end) { // all are visible
						return;
					}
					size_t min_page_lines = static_cast<size_t>(
						minimum_page_size / (editor->get_line_height() * get_scale())
						) + 1,
						// the number of lines in the page about to be rendered
						page_lines = std::max(be.second - be.first, min_page_lines);
					if (be.first + page_lines < page_beg || page_end + page_lines < be.second) {
						// too far away from already rendered region, reset cache
						restart();
					} else {
						if (be.first < page_beg) { // render one page before the first one
							// if the page before is not large enough, make it as large as min_page_lines
							size_t frontline = std::max(page_beg, min_page_lines) - min_page_lines;
							// at least the first visible line is rendered
							render_page(std::min(be.first, frontline), page_beg);
						}
						if (be.second > page_end) { // render one page after the last one
							// if not large enough, make it as large as min_page_lines
							size_t backline = std::min(editor->get_num_visual_lines(), page_end + min_page_lines);
							// at least the last visible line is rendered
							backline = std::max(be.second, backline);
							render_page(page_end, backline);
							page_end = backline; // set page_end
						}
					}
				}
			}
		};

		/// Checks and validates \ref _pgcache by calling \ref _page_cache::make_valid.
		void _on_prerender() override {
			if (!_cachevalid) {
				_pgcache.make_valid();
				_cachevalid = true;
			}
			component::_on_prerender();
		}
		/// Renders all visible pages.
		void _custom_render() override {
			std::pair<size_t, size_t> vlines = _get_visible_lines_folded();
			double slh = _get_editor()->get_line_height() * get_scale();
			rectd clnrgn = get_client_region();
			clnrgn.xmax = clnrgn.xmin + maximum_width;
			clnrgn.ymin = std::round(clnrgn.ymin - _get_y_offset());
			auto
				ibeg = --_pgcache.pages.upper_bound(vlines.first),
				iend = _pgcache.pages.lower_bound(vlines.second);
			os::renderer_base::get().push_blend_function(os::blend_function(
				os::blend_factor::one, os::blend_factor::one_minus_source_alpha
			));
			for (auto i = ibeg; i != iend; ++i) {
				rectd crgn = clnrgn;
				crgn.ymin = std::floor(crgn.ymin + slh * static_cast<double>(i->first));
				crgn.ymax = crgn.ymin + static_cast<double>(i->second.get_texture().get_height());
				os::renderer_base::get().draw_quad(
					i->second.get_texture(), crgn, rectd(0.0, 1.0, 0.0, 1.0), colord()
				);
			}
			os::renderer_base::get().pop_blend_function();
			// render visible region indicator
			if (_vrgnst.update_and_render(_get_clamped_viewport_rect())) {
				invalidate_visual();
			}
		}

		/// Calculates and returns the vertical offset of all pages according to \ref codebox::get_vertical_position.
		double _get_y_offset() const {
			const codebox *cb = _get_box();
			const editor *editor = cb->get_editor();
			size_t nlines = editor->get_num_visual_lines();
			double
				lh = editor->get_line_height(),
				vh = get_client_region().height(),
				maxh = static_cast<double>(nlines) * lh - vh,
				maxvh = static_cast<double>(nlines) * lh * get_scale() - vh,
				perc = (cb->get_vertical_position() - editor->get_padding().top) / maxh;
			perc = std::clamp(perc, 0.0, 1.0);
			return std::max(0.0, perc * maxvh);
		}
		/// Returns the rectangle marking the \ref editor's visible region.
		rectd _get_viewport_rect() const {
			const editor *e = _get_editor();
			rectd clnrgn = get_client_region();
			return rectd::from_xywh(
				clnrgn.xmin - e->get_padding().left * get_scale(),
				clnrgn.ymin - _get_y_offset() +
				(_get_box()->get_vertical_position() - e->get_padding().top) * get_scale(),
				e->get_layout().width() * get_scale(), clnrgn.height() * get_scale()
			);
		}
		/// Clamps the result of \ref _get_viewport_rect. The region is clamped so that its right border won't
		/// overflow when the \ref editor's width is large.
		rectd _get_clamped_viewport_rect() const {
			rectd r = _get_viewport_rect();
			r.xmin = std::max(r.xmin, get_client_region().xmin);
			r.xmax = std::min(r.xmax, get_client_region().xmax);
			return r;
		}
		/// Returns the range of lines that are visible in the \ref minimap, with word wrapping and folding enabled.
		std::pair<size_t, size_t> _get_visible_lines_folded() const {
			const editor *editor = _get_editor();
			double lh = editor->get_line_height() * get_scale(), ys = _get_y_offset();
			return {
				static_cast<size_t>(ys / lh), std::min(static_cast<size_t>(
					std::max(ys + get_client_region().height(), 0.0) / lh
					) + 1, editor->get_num_visual_lines())
			};
		}

		/// Changes the visual state of the visible region indicator.
		void _on_state_changed(value_update_info<ui::element_state_id> &info) override {
			_vrgnst.set_state(_rst.get_state());
			component::_on_state_changed(info);
		}

		/// Registers event handlers to update the minimap and viewport indicator automatically.
		void _on_added() override {
			component::_on_added();
			_vis_tok = (_get_editor()->editing_visual_changed += [this]() {
				_on_editor_visual_changed();
				});
			_vpos_tok = (_get_box()->vertical_viewport_changed += [this](value_update_info<double>&) {
				_on_viewport_changed();
				});
		}
		/// Unregisters all previously registered event handlers.
		void _on_removing() override {
			_get_editor()->editing_visual_changed -= _vis_tok;
			_get_box()->vertical_viewport_changed -= _vpos_tok;
			component::_on_removing();
		}

		/// Marks \ref _pgcache for update when the viewport has changed, to determine if more pages need to
		/// be rendered when \ref _on_prerender is called.
		void _on_viewport_changed() {
			_cachevalid = false;
		}
		/// Clears \ref _pgcache.
		void _on_editor_visual_changed() {
			_pgcache.pages.clear();
			_cachevalid = false;
		}

		/// If the user presses ahd holds the primary mouse button on the viewport, starts dragging it; otherwise,
		/// if the user presses the left mouse button, jumps to the corresponding position.
		void _on_mouse_down(ui::mouse_button_info &info) override {
			if (info.button == os::input::mouse_button::primary) {
				rectd rv = _get_viewport_rect();
				if (rv.contains(info.position)) {
					_dragoffset = rv.ymin - info.position.y;
					get_window()->set_mouse_capture(*this);
					_dragging = true;
				} else {
					rectd client = get_client_region();
					double ch = client.height();
					const editor *edt = _get_editor();
					_get_box()->set_vertical_position(std::min(
						(info.position.y - client.ymin + _get_y_offset()) / get_scale() - 0.5 * ch,
						static_cast<double>(edt->get_num_visual_lines()) * edt->get_line_height() - ch
					) + edt->get_padding().top);
				}
			}
			component::_on_mouse_down(info);
		}
		/// Called to stop dragging the viewport.
		void _stop_dragging() {
			if (_dragging) {
				_dragging = false;
				get_window()->release_mouse_capture();
			}
		}
		/// Calls \ref _stop_dragging if necessary.
		void _on_mouse_up(ui::mouse_button_info &info) override {
			if (info.button == os::input::mouse_button::primary) {
				_stop_dragging();
			}
			component::_on_mouse_up(info);
		}
		/// If dragging, updates the position of the viewport.
		///
		/// \todo Small glitch when starting to drag the region when it's partially out of the minimap area.
		void _on_mouse_move(ui::mouse_move_info &info) override {
			if (_dragging) {
				const editor *edt = _get_editor();
				rectd client = get_client_region();
				double
					scale = get_scale(),
					yp = info.new_pos.y + _dragoffset - client.ymin,
					toth =
					static_cast<double>(edt->get_num_visual_lines()) * edt->get_line_height() - client.height(),
					totch = std::min(client.height() * (1.0 - scale), toth * scale);
				_get_box()->set_vertical_position(toth * yp / totch + edt->get_padding().top);
			}
			component::_on_mouse_move(info);
		}
		/// Calls \ref _stop_dragging.
		void _on_capture_lost() override {
			_stop_dragging();
			component::_on_capture_lost();
		}

		/// Sets the class of \ref _vrgnst.
		void _initialize() override {
			component::_initialize();
			_vrgnst.set_class(get_viewport_class());
		}

		_page_cache _pgcache{*this}; ///< Caches rendered pages.
		event<void>::token _vis_tok; ///< Used to listen to \ref editor::editing_visual_changed.
		/// Used to listen to \ref codebox::vertical_viewport_changed.
		event<value_update_info<double>>::token _vpos_tok;
		ui::visual::render_state _vrgnst; ///< Used to render the visible region indicator.
		/// The offset of the mouse relative to the top border of the visible region indicator.
		double _dragoffset = 0.0;
		bool
			_dragging = false, ///< Indicates whether the visible region indicator is being dragged.
			_cachevalid = true; ///< Indicates whether \ref _page_cache::make_valid needs to be called.

		static double _target_height; ///< The desired font height of minimaps.
	};
}
