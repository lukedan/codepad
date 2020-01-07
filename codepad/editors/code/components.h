// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Additional components of a \ref codepad::editors::code::editor.

#include "contents_region.h"
#include "rendering.h"

namespace codepad::editors::code {
	/// Displays a the line number for each line.
	class line_number_display : public ui::element {
	public:
		/// Returns the width of the longest line number.
		ui::size_allocation get_desired_width() const override {
			if (contents_region *edt = component_helper::get_contents_region(*this)) {
				std::size_t ln = edt->get_document()->num_lines(), w = 0;
				for (; ln > 0; ++w, ln /= 10) {
				}
				// TODO customizable font parameters
				auto font = edt->get_font_families()[0]->get_matching_font(
					ui::font_style::normal, ui::font_weight::normal, ui::font_stretch::normal
				);
				double maxw = edt->get_font_size() * font->get_maximum_character_width_em(
					reinterpret_cast<const codepoint*>(U"0123456789")
				);
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
		void _custom_render() const override {
			element::_custom_render();

			if (auto &&[box, edt] = component_helper::get_core_components(*this); edt) {
				const view_formatting &fmt = edt->get_formatting();
				double
					lh = edt->get_line_height(),
					ybeg = box->get_vertical_position() - edt->get_padding().top,
					yend = ybeg + edt->get_layout().height();
				std::size_t
					fline = static_cast<std::size_t>(std::max(ybeg / lh, 0.0)),
					eline = static_cast<std::size_t>(yend / lh) + 1;
				rectd client = get_client_region();
				double cury = static_cast<double>(fline) * lh - ybeg, width = client.width() + get_padding().left;

				auto &renderer = get_manager().get_renderer();
				auto font = edt->get_font_families()[0]->get_matching_font(
					ui::font_style::normal, ui::font_weight::normal, ui::font_stretch::normal
				);
				double baseline_correction = edt->get_baseline() - font->get_ascent_em() * edt->get_font_size();

				{
					ui::pixel_snapped_render_target buffer(
						renderer,
						rectd::from_corners(vec2d(), get_layout().size()),
						get_window()->get_scaling_factor()
					);

					for (std::size_t curi = fline; curi < eline; ++curi, cury += lh) {
						std::size_t line = fmt.get_folding().folded_to_unfolded_line_number(curi);
						auto lineinfo = fmt.get_linebreaks().get_line_info(line);
						if (lineinfo.first.entry == edt->get_document()->get_linebreaks().end()) {
							break; // when after the end of the document
						}
						if (lineinfo.first.first_char >= lineinfo.second.prev_chars) { // ignore soft linebreaks
							str_t curlbl = std::to_string(1 + line - lineinfo.second.prev_softbreaks);
							auto text = renderer.create_plain_text(curlbl, *font, edt->get_font_size());
							double w = text->get_width();
							// TODO customizable color
							renderer.draw_plain_text(*text, vec2d(width - w, cury + baseline_correction), colord());
						}
					}
				}
			}
		}
	};

	/// Displays a minimap of the code, similar to that of sublime text.
	class minimap : public ui::element {
	public:
		/// The maximum amount of time allowed for rendering a page (i.e., an entry of \ref _page_cache).
		constexpr static std::chrono::duration<double> page_rendering_time_redline{ 0.03 };
		constexpr static std::size_t minimum_page_size = 500; /// Maximum height of a page, in pixels.

		/// Returns the default width, which is proportional to that of the \ref contents_region.
		ui::size_allocation get_desired_width() const override {
			return ui::size_allocation(get_scale(), false);
		}

		/// Returns the scale of the text based on \ref _target_height.
		double get_scale() const {
			if (contents_region *con = component_helper::get_contents_region(*this)) {
				return get_target_line_height() / con->get_line_height();
			}
			return 1.0;
		}

		/// Sets the desired line height of minimaps.
		inline static void set_target_line_height(double h) {
			_target_height = h;
		}
		/// Returns the current line height of minimaps.
		inline static double get_target_line_height() {
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
			constexpr static double
				minimum_width = 50, ///< The minimum width of a page.
				/// Factor used to enlarge the width of pages when the actual width exceeds the page width.
				enlarge_factor = 1.5,
				/// If the actual width is less than this times page width, then page width is shrunk to fit the
				/// actual width.
				shirnk_threshold = 0.5;

			/// Constructor. Sets the associated \ref minimap of this cache.
			explicit _page_cache(minimap &p) : _parent(&p) {
			}

			/// Clears all cached pages, and re-renders the currently visible page immediately. To render this page
			/// on demand, simply clear \ref pages and call \ref invalidate().
			void restart() {
				pages.clear();
				if (contents_region *edt = component_helper::get_contents_region(*_parent)) {
					std::pair<std::size_t, std::size_t> be = _parent->_get_visible_visual_lines();
					double slh = edt->get_line_height() * _parent->get_scale();
					std::size_t
						numlines = edt->get_num_visual_lines(),
						pgsize = std::max(
							be.second - be.first, static_cast<std::size_t>(minimum_page_size / slh) + 1
						),
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
						std::pair<std::size_t, std::size_t> be = _parent->_get_visible_visual_lines();
						std::size_t page_beg = pages.begin()->first;
						if (be.first >= page_beg && be.second <= _page_end) { // all are visible
							return;
						}
						std::size_t min_page_lines = static_cast<std::size_t>(
							minimum_page_size / (edt->get_line_height() * _parent->get_scale())
							) + 1,
							// the number of lines in the page about to be rendered
							page_lines = std::max(be.second - be.first, min_page_lines);
						if (be.first + page_lines < page_beg || _page_end + page_lines < be.second) {
							// too far away from already rendered region, reset cache
							restart();
						} else {
							if (be.first < page_beg) { // render one page before the first one
								// if the page before is not large enough, make it as large as min_page_lines
								std::size_t frontline = std::max(page_beg, min_page_lines) - min_page_lines;
								// at least the first visible line is rendered
								_render_page(std::min(be.first, frontline), page_beg);
							}
							if (be.second > _page_end) { // render one page after the last one
								// if not large enough, make it as large as min_page_lines
								std::size_t backline = std::min(edt->get_num_visual_lines(), _page_end + min_page_lines);
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
				if (w > _width) {
					do {
						_width = _width * enlarge_factor;
					} while (w > _width);
					logger::get().log_debug(CP_HERE) << "minimap width extended to " << _width;
					pages.clear();
					invalidate();
				} else if (_width > minimum_width && w < shirnk_threshold * _width) {
					_width = std::max(minimum_width, w);
					logger::get().log_debug(CP_HERE) << "minimap width shrunk to " << _width;
				}
			}

			/// The cached pages. The keys are the indices of each page's first line, and the values are
			/// corresponding \ref os::frame_buffer "framebuffers".
			std::map<std::size_t, ui::render_target_data> pages;
		protected:
			/// The index past the end of the range of lines that has been rendered and stored in \ref pages.
			std::size_t _page_end = 0;
			double _width = minimum_width; ///< The width of all pages.
			minimap *_parent = nullptr; ///< The associated \ref minimap.
			/// Marks whether this cache is ready for rendering the currently visible portion of the document.
			bool _ready = false;

			/// Renders the page specified by the range of lines, and inserts the result into \ref pages. Note that
			/// this function does not automatically set \ref _page_end.
			///
			/// \param s Index of the first visual line of the page.
			/// \param pe Index past the last visual line of the page.
			void _render_page(std::size_t s, std::size_t pe) {
				ui::window_base *wnd = _parent->get_window();
				if (wnd == nullptr) { // we need the scale factor from the window
					return;
				}

				performance_monitor mon(CP_STRLIT("render_minimap_page"), page_rendering_time_redline);
				if (contents_region *edt = component_helper::get_contents_region(*_parent)) {
					double lh = edt->get_line_height(), scale = _parent->get_scale();

					ui::renderer_base &r = _parent->get_manager().get_renderer();
					ui::render_target_data rt = r.create_render_target(
						vec2d( // add 1 because the starting position was floored instead of rounded
							_width, std::ceil(lh * scale * static_cast<double>(pe - s)) + 1
						),
						wnd->get_scaling_factor()
					);

					const view_formatting &fmt = edt->get_formatting();
					std::size_t
						curvisline = s,
						firstchar = fmt.get_linebreaks().get_beginning_char_of_visual_line(
							fmt.get_folding().folded_to_unfolded_line_number(s)
						).first,
						plastchar = fmt.get_linebreaks().get_beginning_char_of_visual_line(
							fmt.get_folding().folded_to_unfolded_line_number(pe)
						).first;

					fragment_generator<fragment_generator_component_hub<
						soft_linebreak_inserter, folded_region_skipper
						>> gen(
							*edt->get_document(), edt->get_font_families(), firstchar,
							soft_linebreak_inserter(fmt.get_linebreaks(), firstchar),
							folded_region_skipper(fmt.get_folding(), firstchar)
						);
					fragment_assembler ass(*edt);

					r.begin_drawing(*rt.target);
					r.push_matrix_mult(matd3x3::scale(vec2d(0.0, 0.0), scale));
					while (gen.get_position() < plastchar) {
						fragment_generation_result tok = gen.generate_and_update();
						std::visit([&ass, &r](auto &&frag) {
							auto &&rendering = ass.append(frag);
							ass.render(r, rendering);
							}, tok.result);
						if (std::holds_alternative<linebreak_fragment>(tok.result)) {
							++curvisline;
						} else if (ass.get_horizontal_position() > _width / scale) {
							++curvisline;
							std::size_t
								pos = fmt.get_linebreaks().get_beginning_char_of_visual_line(
									fmt.get_folding().folded_to_unfolded_line_number(curvisline)
								).first;
							gen.reposition(pos);
							ass.advance_vertical_position(1);
							ass.set_horizontal_position(0.0);
						}
					}
					r.pop_matrix();
					r.end_drawing();
					pages.insert(std::make_pair(s, std::move(rt)));
				}
			}
		};

		/// Checks and validates \ref _pgcache by calling \ref _page_cache::prepare.
		void _on_prerender() override {
			element::_on_prerender();
			_pgcache.prepare();
		}
		/// Renders all visible pages.
		void _custom_render() const override {
			element::_custom_render();
			if (contents_region *edt = component_helper::get_contents_region(*this)) {
				std::pair<std::size_t, std::size_t> vlines = _get_visible_visual_lines();
				double
					slh = edt->get_line_height() * get_scale(),
					top = std::round(get_padding().top - _get_y_offset());
				auto
					ibeg = _pgcache.pages.upper_bound(vlines.first),
					iend = _pgcache.pages.lower_bound(vlines.second);
				if (ibeg != _pgcache.pages.begin()) {
					--ibeg;
				} else {
					logger::get().log_error(CP_HERE) << "agnomaly in page range selection";
				}

				ui::renderer_base &r = get_manager().get_renderer();
				r.push_rectangle_clip(rectd::from_corners(vec2d(), get_layout().size()));
				for (auto i = ibeg; i != iend; ++i) {
					auto &bmp = *i->second.target_bitmap;
					vec2d topleft(get_padding().left, std::floor(top + slh * static_cast<double>(i->first)));
					r.draw_rectangle(
						rectd::from_corner_and_size(topleft, bmp.get_size()),
						ui::generic_brush_parameters(
							ui::brush_parameters::bitmap_pattern(&bmp), matd3x3::translate(topleft)
						),
						ui::generic_pen_parameters()
					);
				}
				// render visible region indicator
				_viewport_visuals.render(_get_clamped_viewport_rect(), r);
				r.pop_clip();
			}
		}

		/// Calculates and returns the vertical offset of all pages according to \ref editor::get_vertical_position.
		double _get_y_offset() const {
			if (auto && [box, edt] = component_helper::get_core_components(*this); edt) {
				std::size_t nlines = edt->get_num_visual_lines();
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
				double scale = get_scale();
				return rectd::from_xywh(
					get_padding().left - edt->get_padding().left * scale,
					get_padding().top - _get_y_offset() + (
						editor::get_encapsulating(*this)->get_vertical_position() - edt->get_padding().top
						) * scale,
					edt->get_layout().width() * scale, get_client_region().height() * scale
				);
			}
			return rectd();
		}
		/// Clamps the result of \ref _get_viewport_rect. The region is clamped so that its right border won't
		/// overflow when the \ref contents_region's width is large.
		rectd _get_clamped_viewport_rect() const {
			rectd r = _get_viewport_rect();
			r.xmin = std::max(r.xmin, get_padding().left);
			r.xmax = std::min(r.xmax, get_layout().width() - get_padding().right);
			return r;
		}
		/// Returns the range of lines that are visible in the \ref minimap.
		std::pair<std::size_t, std::size_t> _get_visible_visual_lines() const {
			if (contents_region *edt = component_helper::get_contents_region(*this)) {
				double scale = get_scale(), ys = _get_y_offset();
				return edt->get_visible_visual_lines(ys / scale, (ys + get_client_region().height()) / scale);
			}
			return { 0, 0 };
		}

		// TODO notify the visible region indicator of events

		/// Notifies and invalidates \ref _pgcache.
		void _on_layout_changed() override {
			_pgcache.on_width_changed(get_layout().width());
			_pgcache.invalidate(); // invalidate no matter what since the height may have also changed
			element::_on_layout_changed();
		}

		/// Registers event handlers to update the minimap and viewport indicator automatically.
		void _register_handlers() {
			if (auto && [box, edt] = component_helper::get_core_components(*this); edt) {
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
				if (auto && [box, edt] = component_helper::get_core_components(*this); edt) {
					rectd rv = _get_viewport_rect();
					if (rv.contains(info.position.get(*this))) {
						_dragoffset = rv.ymin - info.position.get(*this).y; // TODO not always accurate
						get_window()->set_mouse_capture(*this);
						_dragging = true;
					} else {
						double ch = get_client_region().height();
						box->set_vertical_position(std::min(
							(info.position.get(*this).y - get_padding().top + _get_y_offset()) / get_scale() - 0.5 * ch,
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
				if (auto && [box, edt] = component_helper::get_core_components(*this); edt) {
					rectd client = get_client_region();
					double
						scale = get_scale(),
						yp = info.new_position.get(*this).y + _dragoffset,
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

		/// Handles \ref _viewport_visuals.
		void _set_attribute(str_view_t name, const json::value_storage &v) override {
			if (name == u8"viewport_visuals") {
				if (auto visuals = v.get_value().parse<ui::visuals>(
					ui::managed_json_parser<ui::visuals>(get_manager())
					)) {
					_viewport_visuals = std::move(visuals.value());
				}
				return;
			}
			element::_set_attribute(name, v);
		}
		/// Handles animations related to \ref _viewport_visuals.
		ui::animation_subject_information _parse_animation_path(
			const ui::animation_path::component_list &components
		) override {
			if (!components.empty() && components[0].is_similar(u8"minimap", u8"viewport_visuals")) {
				return ui::animation_subject_information::from_member<&minimap::_viewport_visuals>(
					*this, ui::animation_path::builder::element_property_type::visual_only,
					++components.begin(), components.end()
					);
			}
			return element::_parse_animation_path(components);
		}

		_page_cache _pgcache{ *this }; ///< Caches rendered pages.
		ui::visuals _viewport_visuals; ///< The visuals for the viewport.
		info_event<>::token _vis_tok; ///< Used to listen to \ref contents_region::editing_visual_changed.
		/// The offset of the mouse relative to the top border of the visible region indicator.
		double _dragoffset = 0.0;
		bool _dragging = false; ///< Indicates whether the visible region indicator is being dragged.

		// TODO convert this into a setting
		static double _target_height; ///< The desired font height of minimaps.
	};
}
