// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Declaration of minimaps.

#include "contents_region.h"
#include "fragment_generation.h"

namespace codepad::editors::code {
	/// Displays a minimap of the code, similar to that of sublime text.
	class minimap : public ui::element {
	public:
		/// The maximum amount of time allowed for rendering a page (i.e., an entry of \ref _page_cache).
		constexpr static std::chrono::duration<double> page_rendering_time_redline{ 0.03 };
		constexpr static std::size_t minimum_page_size = 500; /// Maximum height of a page, in pixels.

		/// Returns the scale of the text based on \ref _target_height.
		double get_scale() const {
			return get_target_line_height() / _contents_region->get_line_height();
		}

		/// Sets the desired line height of minimaps.
		inline static void set_target_line_height(double h) {
			_target_height = h;
		}
		/// Returns the current line height of minimaps.
		inline static double get_target_line_height() {
			return _target_height;
		}

		/// Returns the role of the associated \ref contents_region.
		[[nodiscard]] inline static std::u8string_view get_contents_region_role() {
			return u8"contents_region";
		}
		/// Returns the default class of elements of type \ref minimap.
		[[nodiscard]] inline static std::u8string_view get_default_class() {
			return u8"minimap";
		}
		/// Returns the default class of the minimap's viewport.
		[[nodiscard]] inline static std::u8string_view get_viewport_class() {
			return u8"minimap_viewport";
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
			void restart();
			/// Ensures that all visible pages have been rendered. If \ref pages is empty, calls \ref restart;
			/// otherwise checks if new pages need to be rendered.
			void prepare();
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
			/// corresponding \ref ui::render_target_data.
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
			void _render_page(std::size_t s, std::size_t pe);
		};

		/// Handles the \p viewport_visuals property.
		ui::property_info _find_property_path(const ui::property_path::component_list&) const override;
		/// Handles \ref _contents_region and registers for events.
		bool _handle_reference(std::u8string_view, element*) override;

		/// Checks and validates \ref _pgcache by calling \ref _page_cache::prepare.
		void _on_prerender() override {
			element::_on_prerender();
			_pgcache.prepare();
		}
		/// Renders all visible pages.
		void _custom_render() const override;

		/// Calculates and returns the vertical offset of all pages according to \ref editor::get_vertical_position.
		double _get_y_offset() const {
			std::size_t nlines = _contents_region->get_num_visual_lines();
			double
				lh = _contents_region->get_line_height(),
				vh = get_client_region().height(),
				maxh = static_cast<double>(nlines) * lh - vh,
				maxvh = static_cast<double>(nlines) * lh * get_scale() - vh,
				perc = (
					_contents_region->get_editor().get_vertical_position() -
					_contents_region->get_padding().top
				) / maxh;
			perc = std::clamp(perc, 0.0, 1.0);
			return std::max(0.0, perc * maxvh);
		}
		/// Returns the rectangle marking the \ref contents_region's visible region.
		rectd _get_viewport_rect() const {
			double scale = get_scale();
			return rectd::from_xywh(
				get_padding().left - _contents_region->get_padding().left * scale,
				get_padding().top - _get_y_offset() + (
					_contents_region->get_editor().get_vertical_position() - _contents_region->get_padding().top
				) * scale,
				_contents_region->get_layout().width() * scale, get_client_region().height() * scale
			);
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
			double scale = get_scale(), ys = _get_y_offset();
			return _contents_region->get_visible_visual_lines(
				ys / scale, (ys + get_client_region().height()) / scale
			);
		}

		// TODO notify the visible region indicator of events

		/// Notifies and invalidates \ref _pgcache.
		void _on_layout_changed() override {
			_pgcache.on_width_changed(get_layout().width());
			_pgcache.invalidate(); // invalidate no matter what since the height may have also changed
			element::_on_layout_changed();
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
				rectd rv = _get_viewport_rect();
				if (rv.contains(info.position.get(*this))) {
					_dragoffset = rv.ymin - info.position.get(*this).y; // TODO not always accurate
					get_window()->set_mouse_capture(*this);
					_dragging = true;
				} else {
					double client_height = get_client_region().height();
					_contents_region->get_editor().set_target_vertical_position(std::min(
						(info.position.get(*this).y - get_padding().top + _get_y_offset()) / get_scale() -
							0.5 * client_height,
						static_cast<double>(_contents_region->get_num_visual_lines()) *
							_contents_region->get_line_height() -
							client_height
					) + _contents_region->get_padding().top);
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
				rectd client = get_client_region();
				double
					scale = get_scale(),
					y_pos = info.new_position.get(*this).y + _dragoffset;
				double total_height =
					static_cast<double>(_contents_region->get_num_visual_lines()) *
					_contents_region->get_line_height() -
					client.height();
				double total_scaled_height = std::min(client.height() * (1.0 - scale), total_height * scale);
				_contents_region->get_editor().set_vertical_position_immediate(
					total_height * y_pos / total_scaled_height + _contents_region->get_padding().top
				);
			}
		}
		/// Stops dragging.
		void _on_capture_lost() override {
			element::_on_capture_lost();
			_dragging = false;
		}

		_page_cache _pgcache{ *this }; ///< Caches rendered pages.
		contents_region *_contents_region = nullptr; ///< The associated \ref contents_region.
		ui::visuals _viewport_visuals; ///< The visuals for the viewport.
		/// The offset of the mouse relative to the top border of the visible region indicator.
		double _dragoffset = 0.0;
		bool _dragging = false; ///< Indicates whether the visible region indicator is being dragged.

		// TODO convert this into a setting
		static double _target_height; ///< The desired font height of minimaps.
	};
}
