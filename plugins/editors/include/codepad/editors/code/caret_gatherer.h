// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Declaration of \ref codepad::editors::code::caret_gatherer.

#include "../decoration.h"
#include "fragment_generation.h"

namespace codepad::editors::code {
	/// A standalone component that gathers information about carets to be rendered later.
	class caret_gatherer {
	public:
		constexpr static std::size_t maximum_num_lookahead_carets = 2; ///< The maximum number of pending carets.

		/// Constructs this struct with the given \ref caret_set, position, \ref fragment_assembler, and a boolean
		/// indicating whether the previous fragment is a stall.
		caret_gatherer(const caret_set&, std::size_t pos, const fragment_assembler&, bool stall);

		/// Handles a generated fragment. This function first checks for any carets to be started, then updates all
		/// active renderers.
		template <typename Fragment, typename Rendering> void handle_fragment(
			const Fragment &frag, const Rendering &rend, std::size_t steps, std::size_t posafter
		) {
			// check & start new renderers
			for (auto it = _queued.begin(); it != _queued.end(); ) {
				if (auto renderer = _single_caret_renderer::start_at_fragment(
					frag, rend, steps, posafter, *this, *it
				)) {
					_active.emplace_back(std::move(renderer.value()));
					// maintain the number of queued carets
					caret_set::iterator_position next = _queued.back();
					next.move_next();
					if (next.get_iterator() != _carets.carets.end()) {
						_queued.emplace_back(next);
					}
					it = _queued.erase(it);
				} else {
					++it;
				}
			}
			// update all active renderers
			for (auto it = _active.begin(); it != _active.end(); ) {
				if (!it->handle_fragment(frag, rend, steps, posafter, _prev_stall, *this)) { // finished
					it = _active.erase(it);
				} else {
					++it;
				}
			}

			_prev_stall = steps == 0;
		}

		/// Skips the rest of the current line and possibly part of the next line. This function should be called
		/// *before* the metrics in the \ref fragment_assembler are updated.
		void skip_line(bool stall, std::size_t posafter);


		/// Properly stops all active renderers.
		void finish(std::size_t);

		/// Returns a reference to the associated \ref fragment_assembler.
		[[nodiscard]] const fragment_assembler &get_fragment_assembler() const {
			return *_assembler;
		}

		/// Returns the bounding boxes of all carets.
		[[nodiscard]] std::vector<rectd> &get_caret_rects() {
			return _caret_rects;
		}
		/// Returns the layout of all selected regions.
		[[nodiscard]] std::vector<decoration_layout> &get_selection_rects() {
			return _selected_regions;
		}
	protected:
		/// Returns \p true if \p at_stall is \p true and the caret should start before the stall.
		inline static bool _should_start_before_stall(
			ui::caret_selection caret, const caret_data &data, bool at_stall
		) {
			return at_stall && caret.caret_offset == 0 && !data.after_stall;
		}
		/// Returns \p true if \p at_stall is \p true and the caret should end before the stall.
		inline static bool _should_end_before_stall(
			ui::caret_selection caret, const caret_data &data, bool at_stall
		) {
			return at_stall && (caret.caret_offset == caret.selection_length || !data.after_stall);
		}

		/// Handles a single caret. The return values of the \p handle_fragment() methods return \p false if this
		/// caret has ended.
		struct _single_caret_renderer {
		public:
			/// Does nothing when at a \ref no_fragment. Actually, this shouldn't happen.
			[[nodiscard]] inline static std::optional<_single_caret_renderer> start_at_fragment(
				const no_fragment&, const fragment_assembler::basic_rendering&,
				std::size_t, std::size_t, caret_gatherer&, caret_set::iterator_position
			) {
				return std::nullopt;
			}
			/// Tries to start rendering a new caret at the given \ref text_fragment.
			[[nodiscard]] static std::optional<_single_caret_renderer> start_at_fragment(
				const text_fragment&, const fragment_assembler::text_rendering&,
				std::size_t steps, std::size_t posafter,
				caret_gatherer&, caret_set::iterator_position
			);
			/// Tries to start rendering a new caret at the given \ref linebreak_fragment.
			[[nodiscard]] inline static std::optional<_single_caret_renderer> start_at_fragment(
				const linebreak_fragment&, const fragment_assembler::basic_rendering &r,
				std::size_t steps, std::size_t posafter,
				caret_gatherer &rend, caret_set::iterator_position iter
			) {
				return _start_at_solid_fragment(rectd::from_xywh(
					r.topleft.x, r.topleft.y, r.width, rend.get_fragment_assembler().get_line_height()
				), steps, posafter, rend, iter);
			}
			/// Tries to start rendering a new caret at the given generic solid fragment.
			template <
				typename Frag, typename Rendering
			> [[nodiscard]] inline static std::optional<_single_caret_renderer> start_at_fragment(
				const Frag&, const Rendering &r, std::size_t steps, std::size_t posafter,
				caret_gatherer &rend, caret_set::iterator_position iter
			) {
				return _start_at_solid_fragment(
					_get_solid_fragment_caret_position(r, rend.get_fragment_assembler()),
					steps, posafter, rend, iter
				);
			}

			/// Starts rendering a caret halfway at the beginning of the view.
			[[nodiscard]] inline static _single_caret_renderer jumpstart(
				const fragment_assembler &ass, caret_set::iterator_position iter
			) {
				return _single_caret_renderer(
					iter, ass.get_horizontal_position(), ass.get_vertical_position(),
					ass.get_line_height(), ass.get_baseline()
				);
			}

			/// Starts rendering a caret halfway when skipping part of a line.
			[[nodiscard]] static _single_caret_renderer jumpstart_at_skip_line(
				const fragment_assembler&, caret_set::iterator_position
			);

			/// Handles a \ref no_fragment by doing nothing. Normally this should not happen.
			bool handle_fragment(
				const no_fragment&, const fragment_assembler::basic_rendering&,
				std::size_t, std::size_t, bool, caret_gatherer&
			) {
				return true;
			}
			/// Handles a \ref text_fragment.
			bool handle_fragment(
				const text_fragment&, const fragment_assembler::text_rendering&,
				std::size_t steps, std::size_t posafter, bool prev_stall, caret_gatherer&
			);
			/// Handles a \ref linebreak_fragment.
			bool handle_fragment(
				const linebreak_fragment&, const fragment_assembler::basic_rendering&,
				std::size_t steps, std::size_t posafter, bool prev_stall, caret_gatherer&
			);
			/// Handles a generic solid fragment. This includes the \ref image_gizmo_fragment, the
			/// \ref text_gizmo_fragment, the \ref tab_fragment, and the \ref invalid_codepoint_fragment.
			template <typename Frag, typename Rendering> bool handle_fragment(
				const Frag&, const Rendering &r,
				std::size_t steps, std::size_t posafter, bool prev_stall, caret_gatherer &rend
			) {
				return _handle_solid_fragment(
					_get_solid_fragment_caret_position(r, rend.get_fragment_assembler()),
					steps, posafter, prev_stall, rend
				);
			}

			/// Called when the rest of the current line and possibly part of the next line are skipped.
			///
			/// \param posafter The text position after skipping.
			/// \param stall Indicates whether the last skipped fragment is a stall.
			/// \param x The right boundary of the last rendered fragment.
			/// \param rend The \ref caret_gatherer.
			bool handle_line_skip(std::size_t posafter, bool stall, double x, caret_gatherer &rend);

			/// Properly finishes rendering to this caret and adds all accumulated data to the given
			/// \ref caret_gatherer.
			void finish(std::size_t pos, bool prev_stall, caret_gatherer &rend) {
				const fragment_assembler &ass = rend.get_fragment_assembler();
				rectd caret = rectd::from_xywh(
					ass.get_horizontal_position(), rend.get_fragment_assembler().get_vertical_position(),
					10.0, ass.get_line_height()
				); // TODO use the width of a space?

				// check if we should add a caret right now - this is mostly used to handle the end of the document
				if (
					pos == _caret_selection.get_caret_position() &&
					!(prev_stall && !_caret_iter.get_iterator()->data.after_stall)
				) {
					rend._caret_rects.emplace_back(caret);
				}

				_terminate(caret.xmin, rend);
			}

			/// Returns \ref _caret_iter.
			[[nodiscard]] const caret_set::iterator_position &get_iterator() const {
				return _caret_iter;
			}
		protected:
			caret_set::iterator_position _caret_iter; ///< Iterator to the caret data.
			ui::caret_selection _caret_selection; ///< Caret position data.
			/// The layout of the selection.
			decoration_layout _selected_regions;
			double _region_left = 0.0; ///< The left boundary of the next selected region.

			/// Initializes this struct given the corresponding caret, position, and font parameters.
			_single_caret_renderer(
				caret_set::iterator_position iter, double x, double y, double line_height, double baseline
			) : _caret_iter(iter), _caret_selection(iter.get_caret_selection()), _region_left(x) {
				_selected_regions.top = y;
				_selected_regions.line_height = line_height;
				_selected_regions.baseline = baseline;
			}

			/// Returns the caret layout for a \ref fragment_assembler::basic_rendering.
			[[nodiscard]] inline static rectd _get_solid_fragment_caret_position(
				const fragment_assembler::basic_rendering &r, const fragment_assembler &ass
			) {
				return rectd(
					r.topleft.x, ass.get_horizontal_position(),
					r.topleft.y, r.topleft.y + ass.get_line_height()
				);
			}
			/// Returns the caret layout for a \ref fragment_assembler::text_rendering.
			[[nodiscard]] inline static rectd _get_solid_fragment_caret_position(
				const fragment_assembler::text_rendering &r, const fragment_assembler &ass
			) {
				return rectd::from_xywh(
					r.topleft.x, r.topleft.y, r.text->get_width(), ass.get_line_height()
				);
			}

			/// Returns whether a caret should be inserted at the current fragment. If the caret is at the middle of
			/// the current fragment, returns \p true; otherwise checks if any stalls prevent the caret from being
			/// added at the very beginning. Carets are never handled if they're at the end of the fragment.
			[[nodiscard]] bool _should_insert_caret(std::size_t steps, std::size_t posafter, bool prev_stall) const;

			/// Checks if the given caret starts at the given solid fragment (one that's not a text fragment). If so,
			/// returns a corresponding \ref _single_caret_renderer and adds a caret if necessary.
			[[nodiscard]] static std::optional<_single_caret_renderer> _start_at_solid_fragment(
				rectd caret, std::size_t steps, std::size_t posafter,
				caret_gatherer&, caret_set::iterator_position
			);

			/// Handles a fragmen which the caret cannot appear inside.
			[[nodiscard]] bool _handle_solid_fragment(
				rectd caret, std::size_t steps, std::size_t posafter, bool prev_stall, caret_gatherer&
			);

			/// Appends to \ref _selected_regions the horizontal range of this line.
			void _append_line_selection(double x) {
				_selected_regions.line_bounds.emplace_back(_region_left, x);
			}

			/// Called right before finishing this caret and selected region. This function appends a final selected
			/// rectangle to \ref _selected_regions, then moves it to \ref caret_gatherer::_selected_regions.
			void _terminate(double x, caret_gatherer &rend) {
				_append_line_selection(x);
				rend._selected_regions.emplace_back(std::move(_selected_regions));
			}
		};

		std::vector<rectd> _caret_rects; ///< The positions of all carets.
		std::vector<decoration_layout> _selected_regions; ///< The positions of selected regions.
		const caret_set &_carets; ///< The set of carets.
		std::list<_single_caret_renderer> _active; ///< The list of active renderers for individual carets.
		std::list<caret_set::iterator_position> _queued; ///< The list of carets that would start shortly.
		const fragment_assembler *_assembler = nullptr; ///< The associated \ref fragment_assembler.
		bool _prev_stall = false; ///< Whether the previous fragment was a stall.
	};
}
