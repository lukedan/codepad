// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Support for whitespace rendering.

#include "caret_set.h"
#include "fragment_generation.h"

namespace codepad::editors::code {
	/// Gathers whitespaces (such as spaces, tabs, line breaks) for rendering in a text document.
	class whitespace_gatherer {
	public:
		/// Information about a single whitespace character.
		struct whitespace {
			/// The type of a whitespace.
			enum class type : unsigned char {
				whitespace, ///< A space character.
				tab, ///< A tab character.
				crlf, ///< A CRLF line break.
				cr, ///< A CR line break.
				lf, ///< A LF line break.

				max_count ///< The total number of whitespace types.
			};

			/// Default constructor.
			whitespace() = default;
			/// Initializes all fields of this struct.
			whitespace(rectd plac, type ty) : placement(plac), whitespace_type(ty) {
			}

			rectd placement; ///< The placement of the character.
			type whitespace_type = type::max_count; ///< The type of this whitespace.
		};


		/// Initializes \ref _carets and \ref _next_caret.
		whitespace_gatherer(const caret_set &set, std::size_t first_char, fragment_assembler &ass) :
			_ass(ass), _carets(set) {

			_caret = _carets.find_first_ending_after(first_char);
			if (_caret.get_iterator() != _carets.carets.end()) {
				_range = _caret.get_caret_selection().get_range();
			}
		}

		/// Handles a clip of text.
		void handle_fragment(
			const text_fragment &frag, const fragment_assembler::text_rendering &rend,
			std::size_t steps, std::size_t posafter
		) {
			std::size_t frag_begin = posafter - steps;
			if (!_reposition(frag_begin)) {
				return;
			}
			// the _reposition() call ensure that _caret is non-null
			while (_range.first < posafter) {
				// gather all whitespaces that overlap with the current selection
				std::size_t
					beg = std::max(_range.first, frag_begin) - frag_begin,
					end = std::min(_range.second, posafter) - frag_begin;
				for (std::size_t i = beg; i < end; ++i) {
					if (frag.text[i] == U' ') {
						rectd placement = rend.text->get_character_placement(i);
						placement = placement.translated(rend.topleft);
						placement.ymax = placement.ymin + _ass.get_line_height();
						whitespaces.emplace_back(placement, whitespace::type::whitespace);
					}
				}

				// break if the caret extends beyond this fragment
				if (_range.second > posafter) {
					break;
				}
				// update loop variable
				_caret.move_next();
				if (_caret.get_iterator() == _carets.carets.end()) {
					break;
				}
				_range = _caret.get_caret_selection().get_range();
			}
		}
		/// Handles a tab character.
		void handle_fragment(
			const tab_fragment&, const fragment_assembler::basic_rendering &rend,
			std::size_t steps, std::size_t posafter
		) {
			std::size_t frag_begin = posafter - steps;
			if (!_reposition(frag_begin)) {
				return;
			}
			if (_range.first <= frag_begin && _range.second >= posafter) {
				// add a tab character
				whitespaces.emplace_back(rectd::from_corner_and_size(
					rend.topleft, vec2d(rend.width, _ass.get_line_height())
				), whitespace::type::tab);
			}
		}
		/// Handles a line break.
		void handle_fragment(
			const linebreak_fragment &frag, const fragment_assembler::basic_rendering &rend,
			std::size_t steps, std::size_t posafter
		) {
			std::size_t frag_begin = posafter - steps;
			if (!_reposition(frag_begin)) {
				return;
			}
			if (_range.first <= frag_begin && _range.second >= posafter) {
				whitespace::type type;
				switch (frag.type) {
				case line_ending::r:
					type = whitespace::type::cr;
					break;
				case line_ending::n:
					type = whitespace::type::lf;
					break;
				case line_ending::rn:
					type = whitespace::type::crlf;
					break;
				default:
					return; // don't render soft linebreaks
				}
				// add a new line character
				whitespaces.emplace_back(rectd::from_corner_and_size(
					rend.topleft, vec2d(rend.width, _ass.get_line_height())
				), type);
			}
		}
		/// Ignore all other fragment types.
		template <typename Frag, typename Rend> void handle_fragment(
			const Frag&, const Rend&, std::size_t, std::size_t
		) {
		}

		std::vector<whitespace> whitespaces; ///< The gathered list of whitespaces.
	protected:
		caret_set::iterator_position _caret; ///< Iterator to the current caret that's being considered.
		std::pair<std::size_t, std::size_t> _range; ///< The cached range of \ref _next_caret.
		fragment_assembler &_ass; ///< The associated \ref fragment_assembler.
		const caret_set &_carets; ///< The set of carets.

		/// Repositions \ref _next_caret and updates \ref _range so that the caret is the first one after the given
		/// position or contains the caret at the given position.
		///
		/// \return Whether \ref _caret points to a caret after this operation.
		bool _reposition(std::size_t pos) {
			if (_caret.get_iterator() == _carets.carets.end()) {
				return false;
			}
			if (_range.second > pos) {
				return true;
			}
			// fast path: move to the next one and check if we're at the end
			_caret.move_next();
			if (_caret.get_iterator() == _carets.carets.end()) {
				return false;
			}
			_range = _caret.get_caret_selection().get_range();
			if (_range.second > pos) {
				return true;
			}
			// slow path: use lower_bound
			_caret = _carets.find_first_ending_after(pos);
			if (_caret.get_iterator() != _carets.carets.end()) {
				_range = _caret.get_caret_selection().get_range();
				return true;
			}
			return false;
		}
	};
}
