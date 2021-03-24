// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Declaration of \ref codepad::editors::code::decoration_gatherer.

#include <vector>
#include <list>

#include "../decoration.h"

namespace codepad::editors::code {
	/// Used to collect decorations to render while going through a text document.
	class decoration_gatherer {
	public:
		/// Initializes the list of `current' iterators.
		decoration_gatherer(
			const std::list<std::unique_ptr<decoration_provider>> &decos, std::size_t position,
			const fragment_assembler &ass
		) : _providers(decos), _assembler(ass) {
			_next.resize(_providers.size());
			std::size_t i = 0;
			for (auto iter = _providers.begin(); iter != _providers.end(); ++iter, ++i) {
				_next[i] = (*iter)->decorations.find_first_range_ending_after(position);
			}
		}

		/// Calls \ref _single_decoration_renderer::start_at_fragment() to process any decorations that should start
		/// at this fragment, then updates all fragments in \ref _active.
		template <typename Fragment, typename Rendering> void handle_fragment(
			const Fragment &frag, const Rendering &rend, std::size_t steps, std::size_t posafter
		) {
			// check & start new decorations
			std::size_t i = 0;
			for (auto iter = _providers.begin(); iter != _providers.end(); ++iter, ++i) {
				while (_next[i].get_iterator() != (*iter)->decorations.end()) {
					if (_next[i].get_range_start() + _next[i].get_iterator()->length + steps <= posafter) {
						// we've gone past this range, discard
						_next[i] = (*iter)->decorations.find_next_range_ending_after(posafter - steps, _next[i]);
						continue;
					}
					auto deco = _single_decoration_renderer::start_at_fragment(
						rend, steps, posafter, *this, _next[i]
					);
					if (deco) {
						_active.emplace_back(std::move(deco.value()));
						_next[i] = (*iter)->decorations.find_next_range_ending_after(posafter - steps, _next[i]);
					} else {
						break;
					}
				}
			}
			// update all active decorations
			for (auto it = _active.begin(); it != _active.end(); ) {
				if (!it->handle_fragment(frag, rend, steps, posafter, *this)) {
					it = _active.erase(it);
				} else {
					++it;
				}
			}
		}

		/// Skips the rest of the current line and possibly part of the next line. This function should be called
		/// *before* the metrics in the \ref fragment_assembler are updated.
		void skip_line(std::size_t posafter) {
			// update active renderers
			for (auto it = _active.begin(); it != _active.end(); ) {
				if (!it->handle_line_skip(posafter, *this)) {
					it = _active.erase(it);
				} else {
					++it;
				}
			}
			// jumpstart or discard new carets
			std::size_t i = 0;
			for (auto iter = _providers.begin(); iter != _providers.end(); ++iter, ++i) {
				while (_next[i].get_iterator() != (*iter)->decorations.end()) {
					std::size_t range_start = _next[i].get_range_start();
					std::size_t range_end = range_start + _next[i].get_iterator()->length;
					if (range_start > posafter) { // not there yet
						break;
					}
					if (range_end > posafter) {
						_active.emplace_back(_single_decoration_renderer::jumpstart_at_skip_line(
							get_fragment_assembler(), _next[i]
						));
					} // otherwise this one is discarded - too late
					_next[i] = (*iter)->decorations.find_next_range_ending_after(posafter, _next[i]);
				}
			}
		}

		/// Finishes all active renderers.
		void finish() {
			for (auto iter = _active.begin(); iter != _active.end(); ++iter) {
				iter->finish(*this);
			}
		}


		/// Returns a reference to the associated \ref fragment_assembler.
		[[nodiscard]] const fragment_assembler &get_fragment_assembler() const {
			return _assembler;
		}

		/// Callback function that's called when a decoration's layout has been fully computed.
		std::function<void(decoration_layout, decoration_renderer*)> render_callback;
	protected:
		/// Computes the layout of a single decoration.
		struct _single_decoration_renderer {
		public:
			/// Does nothing when at a \ref no_fragment. This shouldn't happen.
			[[nodiscard]] inline static std::optional<_single_decoration_renderer> start_at_fragment(
				const no_fragment&, const fragment_assembler::basic_rendering&,
				std::size_t, std::size_t, decoration_gatherer&, decoration_provider::registry::iterator_position
			) {
				return std::nullopt;
			}
			/// Tries to start rendering a new decoration at the given \ref text_fragment.
			[[nodiscard]] static std::optional<_single_decoration_renderer> start_at_fragment(
				const fragment_assembler::text_rendering &rend,
				std::size_t steps, std::size_t posafter, decoration_gatherer &gatherer,
				decoration_provider::registry::iterator_position iter
			) {
				std::size_t range_start = iter.get_range_start();
				assert_true_logical(
					range_start + iter.get_iterator()->length + steps >= posafter,
					"single decoration renderer was not started or discarded in time"
				);
				if (posafter > range_start) {
					double position =
						rend.topleft.x +
						rend.text->get_character_placement(range_start - (posafter - steps)).xmin;
					return _single_decoration_renderer(
						vec2d(position, rend.topleft.y), range_start + iter.get_iterator()->length,
						iter.get_iterator()->value.renderer,
						gatherer.get_fragment_assembler().get_line_height(),
						gatherer.get_fragment_assembler().get_baseline()
					);
				}
				return std::nullopt;
			}
			/// Tries to start rendering a new decoration at the given generic solid fragment.
			template <
				typename Rendering
			> [[nodiscard]] inline static std::optional<_single_decoration_renderer> start_at_fragment(
				const Rendering &r, std::size_t steps, std::size_t posafter,
				decoration_gatherer &rend, decoration_provider::registry::iterator_position iter
			) {
				return _start_at_solid_fragment(r.topleft, steps, posafter, rend, iter);
			}

			/// Starts rendering a decoration halfway at the beginning of the view.
			[[nodiscard]] inline static _single_decoration_renderer jumpstart(
				const fragment_assembler &ass, decoration_provider::registry::iterator_position iter
			) {
				return _single_decoration_renderer(
					ass.get_position(), iter.get_range_start() + iter.get_iterator()->length,
					iter.get_iterator()->value.renderer, ass.get_line_height(), ass.get_baseline()
				);
			}

			/// Starts rendering a decoration halfway when skipping part of a line.
			[[nodiscard]] static _single_decoration_renderer jumpstart_at_skip_line(
				const fragment_assembler &ass, decoration_provider::registry::iterator_position iter
			) {
				_single_decoration_renderer result(
					ass.get_position(), iter.get_range_start() + iter.get_iterator()->length,
					iter.get_iterator()->value.renderer, ass.get_line_height(), ass.get_baseline()
				);
				result._layout.line_bounds.back().second = result._layout.line_bounds.back().first;
				result._layout.line_bounds.emplace_back(0.0, 0.0);
				return result;
			}

			/// Does nothing at a \ref no_fragment. This shouldn't happen.
			bool handle_fragment(
				const no_fragment&, const fragment_assembler::basic_rendering&,
				std::size_t, std::size_t, decoration_gatherer&
			) {
				return true;
			}
			/// Handles a \ref text_fragment.
			bool handle_fragment(
				const text_fragment&, const fragment_assembler::text_rendering &text,
				std::size_t steps, std::size_t posafter, decoration_gatherer &rend
			) {
				if (_end <= posafter) {
					double offset = text.text->get_character_placement(_end - (posafter - steps)).xmin;
					_terminate(text.topleft.x + offset, rend);
					return false;
				}
				return true;
			}
			/// Handles a line break.
			bool handle_fragment(
				const linebreak_fragment&, const fragment_assembler::basic_rendering &rendering,
				std::size_t, std::size_t posafter, decoration_gatherer &rend
			) {
				double xmax = rendering.topleft.x;
				logger::get().log_debug(CP_HERE) << "xmax: " << xmax;
				xmax += rendering.width;
				if (_end <= posafter) {
					_terminate(xmax, rend);
					return false;
				}
				_layout.line_bounds.back().second = xmax;
				_layout.line_bounds.emplace_back(0.0, 0.0);
				return true;
			}
			/// Handles a generic solid fragment.
			template <typename Frag, typename Rendering> bool handle_fragment(
				const Frag&, const Rendering &rendering, std::size_t, std::size_t posafter,
				decoration_gatherer &rend
			) {
				if (_end <= posafter) {
					double x = rendering.topleft.x;
					if (_end == posafter) {
						x = rend.get_fragment_assembler().get_horizontal_position();
					}
					_terminate(x, rend);
					return false;
				}
				return true;
			}

			/// Called when the rest of the current line and possibly part of the next line are skipped.
			bool handle_line_skip(std::size_t posafter, decoration_gatherer &rend) {
				if (_end <= posafter) {
					_terminate(rend.get_fragment_assembler().get_horizontal_position(), rend);
					return false;
				}
				_layout.line_bounds.back().second = rend.get_fragment_assembler().get_horizontal_position();
				assert_true_logical(_layout.line_bounds.back().second >= _layout.line_bounds.back().first);
				_layout.line_bounds.emplace_back(0.0, 0.0);
				return true;
			}

			/// Finishes this decoration.
			void finish(decoration_gatherer &rend) {
				_terminate(_layout.line_bounds.back().first, rend);
			}
		protected:
			decoration_layout _layout; ///< The accumulated layout of the current decoration.
			decoration_renderer *_renderer = nullptr; ///< The renderer associated with the current decoration.
			std::size_t _end = 0; ///< The ending position of this decoration.

			/// Creates a new \ref single_decoration_renderer using the given starting position, end position,
			/// \ref decoration_renderer, and text parameters.
			_single_decoration_renderer(
				vec2d topleft, std::size_t end, decoration_renderer *rend, double line_height, double baseline
			) : _renderer(rend), _end(end) {
				_layout.top = topleft.y;
				_layout.line_bounds.emplace_back().first = topleft.x;
				_layout.line_height = line_height;
				_layout.baseline = baseline;
			}

			/// Attempts to start rendering at a solid fragment. This function only starts if the range's starting
			/// position is at or before the fragment and lasts past this fragment.
			[[nodiscard]] inline static std::optional<_single_decoration_renderer> _start_at_solid_fragment(
				vec2d topleft, std::size_t steps, std::size_t posafter, decoration_gatherer &deco,
				decoration_provider::registry::iterator_position iter
			) {
				if (steps == 0) { // never start at stalls
					return std::nullopt;
				}
				std::size_t range_start = iter.get_range_start();
				if (range_start + steps <= posafter && range_start + iter.get_iterator()->length >= posafter) {
					return _single_decoration_renderer(
						topleft, range_start + iter.get_iterator()->length, iter.get_iterator()->value.renderer,
						deco.get_fragment_assembler().get_line_height(), deco.get_fragment_assembler().get_baseline()
					);
				}
				return std::nullopt;
			}

			/// Terminates this renderer by updating the end position of the last range and invoking
			/// \ref decoration_gatherer::render_callback.
			void _terminate(double x, decoration_gatherer &rend) {
				_layout.line_bounds.back().second = x;
				rend.render_callback(std::move(_layout), _renderer);
			}
		};

		/// Next-up iterators and positions for all entries in \ref _providers.
		std::vector<decoration_provider::registry::iterator_position> _next;
		std::list<_single_decoration_renderer> _active; ///< Active renderers.
		const std::list<std::unique_ptr<decoration_provider>> &_providers; ///< The set of decorations providers.
		const fragment_assembler &_assembler; ///< The associated \ref fragment_assembler.
	};
}
