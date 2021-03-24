// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Structs used to manage the formatting of a buffer that's independent of the view.

#include <codepad/core/red_black_tree.h>

#include "../buffer.h"

namespace codepad::editors::code {

	/// A registry of all the lines in the file. This is mainly used to accelerate operations
	/// such as finding the i-th line, and to handle multi-codepoint linebreaks.
	class linebreak_registry {
		friend class soft_linebreak_registry;
	public:
		/// Stores information about a single line.
		struct line_info {
			/// Default constructor.
			line_info() = default;
			/// Constructor that initializes all the fields of the struct.
			line_info(std::size_t d, ui::line_ending t) : nonbreak_chars(d), ending(t) {
			}

			std::size_t nonbreak_chars = 0; ///< The number of codepoints in this line, excluding the linebreak.
			/// The type of the line ending. This will be line_ending::none for the last line.
			ui::line_ending ending = ui::line_ending::none;
			red_black_tree::color color = red_black_tree::color::black; ///< The color of this node.
		};
		/// Stores additional data of a node in the tree.
		struct line_synth_data {
		public:
			/// A node in the tree.
			using node_type = binary_tree_node<line_info, line_synth_data>;
			/// Used to obtain the total number of codepoints, including the linebreak, in a line.
			struct get_node_codepoint_num {
				/// Returns the sum of line_info::nonbreak_chars and the corresponding length
				/// of line_info::ending.
				inline static std::size_t get(const node_type &n) {
					return n.value.nonbreak_chars + ui::get_line_ending_length(n.value.ending);
				}
			};
			/// Used to obtain the number of linebreaks that follows the line.
			struct get_node_linebreak_num {
				/// Returns 0 if the line is the last line of the buffer, 1 otherwise.
				inline static std::size_t get(const node_type &n) {
					return n.value.ending == ui::line_ending::none ? 0 : 1;
				}
			};
			/// Used to obtain the number of characters in a line. The linebreak counts as one character even
			/// if it's line_ending::rn.
			struct get_node_char_num {
				/// Returns line_info::nonbreak_chars plus the value returned by get_node_linebreak_num::get().
				inline static std::size_t get(const node_type &n) {
					return n.value.nonbreak_chars + get_node_linebreak_num::get(n);
				}
			};

			std::size_t
				total_codepoints = 0, ///< The total number of codepoints in the subtree.
				total_chars = 0, ///< The total number of characters in the subtree.
				total_linebreaks = 0; ///< The total number of linebreaks in the subtree.

			/// Property used to calculate the number of codepoints in a range of lines.
			using num_codepoints_property = sum_synthesizer::compact_property<
				get_node_codepoint_num, &line_synth_data::total_codepoints
			>;
			/// Property used to calculate the number of characters in a range of lines.
			using num_chars_property = sum_synthesizer::compact_property<
				get_node_char_num, &line_synth_data::total_chars
			>;
			/// Property used to calculate the number of linebreaks in a range of lines.
			using num_linebreaks_property = sum_synthesizer::compact_property<
				get_node_linebreak_num, &line_synth_data::total_linebreaks
			>;
			/// Property used to calculate the number of lines in a range of nodes. This may be inaccurate
			/// in certain occasions (more specifically, for right sub-trees) and is not used in synthesize().
			using num_lines_property = sum_synthesizer::compact_property<
				synthesization_helper::identity, &line_synth_data::total_linebreaks
			>;

			/// Calls \ref sum_synthesizer::synthesize to update the values regarding to the subtree.
			inline static void synthesize(node_type &n) {
				sum_synthesizer::synthesize<num_codepoints_property, num_chars_property, num_linebreaks_property>(n);
			}
		};
		/// A binary tree for storing line information.
		using tree_type = red_black_tree::tree<line_info, red_black_tree::member_red_black_access<&line_info::color>, line_synth_data>;
		/// A node of the binary tree.
		using node_type = tree_type::node;
		/// A const iterator through the nodes of the tree.
		using iterator = tree_type::const_iterator;

		/// Used to convert between positions of characters and codepoints. The queries must have increasing
		/// positions.
		struct position_converter {
		public:
			/// Initializes this converter with the corresponding \ref interpretation.
			position_converter(const linebreak_registry &lines) : _lines(lines) {
				reset();
			}

			/// Resets this converter.
			void reset() {
				_lineit = _lines.begin();
				_firstcp = _firstchar = 0;
			}

			/// Returns the position of the first codepoint of the character at the given position.
			std::size_t character_to_codepoint(std::size_t pos) {
				if (_firstchar + line_synth_data::get_node_char_num::get(*_lineit.get_node()) > pos) {
					return pos - _firstchar + _firstcp;
				}
				auto line = _lines.get_line_and_column_and_codepoint_of_char(pos);
				_lineit = line.first.line_iterator;
				_firstchar = pos - line.first.position_in_line;
				_firstcp = line.second - line.first.position_in_line;
				return line.second;
			}
			/// Returns the position of the character that contains the codepoint at the given position.
			std::size_t codepoint_to_character(std::size_t pos) {
				if (_firstcp + line_synth_data::get_node_codepoint_num::get(*_lineit.get_node()) > pos) {
					return std::min(pos - _firstcp + _firstchar, _firstchar + _lineit->nonbreak_chars);
				}
				auto line = _lines.get_line_and_column_and_char_of_codepoint(pos);
				_lineit = line.first.line_iterator;
				_firstcp = pos - line.first.position_in_line;
				_firstchar = line.second - line.first.position_in_line;
				return line.second;
			}
		protected:
			iterator _lineit; ///< Iterator to the current line.
			const linebreak_registry &_lines; ///< The associated \ref linebreak_registry.
			std::size_t
				_firstcp = 0, ///< The number of codepoints before \ref _lineit.
				_firstchar = 0; ///< The number of characters before \ref _lineit.
		};

		/// Stores the line and column of a certain character or codepoint.
		struct line_column_info {
			/// Default constructor.
			line_column_info() = default;
			/// Constructor that initializes the struct with the given values.
			line_column_info(iterator lit, std::size_t l, std::size_t c) :
				line_iterator(lit), line(l), position_in_line(c) {
			}

			iterator line_iterator; ///< An iterator to the line corresponding to \ref line.
			std::size_t
				line = 0, ///< The line that the character or codepoint is on.
				/// The position of the character or codepoint relative to the beginning of this line.
				position_in_line = 0;
		};
		/// Stores information of a text clip, including the number of characters, and the lengths and line endings
		/// of each line in the text clip.
		struct text_clip_info {
			/// Default constructor.
			text_clip_info() = default;
			/// Initializes all fields of this struct.
			text_clip_info(std::size_t c, std::vector<line_info> ls) :
				total_chars(c), lines(std::move(ls)) {
			}
			std::size_t total_chars = 0; ///< The total number of characters in the text clip.
			std::vector<line_info> lines; ///< The information of all invidual lines.

			/// Appends a line to this struct.
			void append_line(std::size_t nonbreak_chars, ui::line_ending ending) {
				total_chars += nonbreak_chars;
				if (ending != ui::line_ending::none) {
					++total_chars;
				}
				lines.emplace_back(nonbreak_chars, ending);
			}
			/// \overload
			void append_line(line_info line) {
				append_line(line.nonbreak_chars, line.ending);
			}
		};
		/// Information about an insert operation.
		struct insert_result {
			bool
				/// Whether this insertion occurred at the middle of a CRLF line break and caused it to split.
				split = false,
				/// Whether a LF codepoint at the very beginning of the inserted content merged with the previous
				/// codepoint that is a CR into a CRLF line break.
				merge_front = false,
				/// Whether a CR codepoint at the very end of the inserted content merged with the next codepoint
				/// that is a LF into a CRLF line break.
				merge_back = false;
		};
		/// Information about an erase operation.
		struct erase_result {
			bool
				/// Whether the first erased codepoint is part of a CRLF line break that caused it to be split.
				split_front = false,
				/// Whether the last erased codepoint is part of a CRLF line break that caused it to be split.
				split_back = false,
				/// Whether the codepoints before and after the erased content gets merged into a CRLF line break.
				merge = false;
		};

		/// Stores information about a line in the tree.
		struct linebreak_info {
			/// Default constructor.
			linebreak_info() = default;
			/// Initializes all field of this struct.
			linebreak_info(const iterator &it, std::size_t fc) : entry(it), first_char(fc) {
			}
			iterator entry; ///< An iterator to the line.
			/// The number of characters before the first character of the line in the whole buffer.
			std::size_t first_char = 0;
		};


		/// Calls clear() to initialize the tree to contain a single empty line with no linebreaks.
		linebreak_registry() {
			clear();
		}

		/// Returns the number of codepoints before the character at the given index.
		[[nodiscard]] std::size_t position_char_to_codepoint(std::size_t c) const {
			_pos_char_to_cp selector;
			_t.find(selector, c);
			return selector.total_codepoints + c;
		}
		/// Returns a \ref linebreak_info containing information about the given line.
		[[nodiscard]] linebreak_info get_line_info(std::size_t l) const {
			_line_beg_char_accum_finder selector;
			auto it = _t.find(selector, l);
			return linebreak_info(it, selector.total);
		}
		/// Returns the position of the first codepoint of the given line.
		[[nodiscard]] std::size_t get_beginning_codepoint_of_line(std::size_t l) const {
			_line_beg_codepoint_accum_finder selector;
			_t.find(selector, l);
			return selector.total;
		}

		/// Returns an iterator to the first line.
		[[nodiscard]] iterator begin() const {
			return _t.begin();
		}
		/// Returns an iterator past the last line.
		[[nodiscard]] iterator end() const {
			return _t.end();
		}
		/// Returns an iterator to the specified line.
		[[nodiscard]] iterator at_line(std::size_t line) const {
			return _t.find(_line_beg_finder(), line);
		}

		/// Returns a \ref line_column_info containing information about the codepoint at the given index. If the
		/// codepoint is at the end of the buffer (i.e., EOF), the returned iterator will still be end() - 1.
		[[nodiscard]] line_column_info get_line_and_column_of_codepoint(std::size_t cp) const {
			_get_line<line_synth_data::num_codepoints_property> selector;
			iterator it = _t.find(selector, cp);
			return line_column_info(it, selector.total_lines, cp);
		}
		/// Returns a \ref line_column_info containing information about the character at the given index. If the
		/// character is at the end of the buffer (i.e., EOF), the returned iterator will still be end() - 1.
		[[nodiscard]] line_column_info get_line_and_column_of_char(std::size_t c) const {
			_get_line<line_synth_data::num_chars_property> selector;
			iterator it = _t.find(selector, c);
			return line_column_info(it, selector.total_lines, c);
		}
		/// Returns a \ref line_column_info containing information about the character at the given index,
		/// and the index of the codepoint corresponding to the character. If the character is at the end
		/// of the buffer (i.e., EOF), the returned iterator will still be end() - 1.
		[[nodiscard]] std::pair<line_column_info, std::size_t> get_line_and_column_and_codepoint_of_char(
			std::size_t c
		) const {
			_pos_char_to_cp selector;
			iterator it = _t.find(selector, c);
			return {line_column_info(it, selector.total_lines, c), selector.total_codepoints + c};
		}
		/// Returns a \ref line_column_info containing information about the codepoint at the given index, and the
		/// index of the character that contains the codepoint. If the codepoint is at the end of the buffer
		/// (i.e., EOF), the returned iterator will still be end() - 1.
		///
		/// \sa get_line_and_column_of_codepoint()
		[[nodiscard]] std::pair<line_column_info, std::size_t> get_line_and_column_and_char_of_codepoint(
			std::size_t cp
		) const {
			_pos_cp_to_char selector;
			iterator it = _t.find(selector, cp);
			return {
				line_column_info(it, selector.total_lines, cp),
				selector.total_chars + std::min(cp, it->nonbreak_chars)
			};
		}

		/// Returns the index of the line to which the given iterator points.
		[[nodiscard]] std::size_t get_line(iterator i) const {
			return _get_node_sum_before<line_synth_data::num_lines_property>(i);
		}
		/// Returns the index of the first codepoint of the line corresponding to the given iterator.
		[[nodiscard]] std::size_t get_beginning_codepoint_of(iterator i) const {
			return _get_node_sum_before<line_synth_data::num_codepoints_property>(i);
		}
		/// Returns the index of the first character of the line corresponding to the given iterator.
		[[nodiscard]] std::size_t get_beginning_char_of(iterator i) const {
			return _get_node_sum_before<line_synth_data::num_chars_property>(i);
		}


		/// Called when a text clip has been inserted to the buffer. \p at will remain valid after this operation and
		/// will point to the last line with newly inserted contents.
		///
		/// \param at The line at which the text is to be inserted.
		/// \param offset The position in the line at whith the text is to be inserted.
		/// \param lines Lines of the text clip.
		insert_result insert_codepoints(iterator at, std::size_t offset, const std::vector<line_info> &lines) {
			assert_true_logical(!(at == _t.end() && offset != 0), "invalid insert position");
			assert_true_logical(!lines.empty() && lines.back().ending == ui::line_ending::none, "invalid text");
			if (lines.size() == 1 && lines[0].nonbreak_chars == 0) {
				return insert_result(); // nothing to insert; if we continue we would incorrectly break up \r\n
			}
			insert_result result;
			if (at != _t.end() && offset > at->nonbreak_chars) { // break \r\n
				assert_true_logical(at->ending == ui::line_ending::rn, "invalid begin padding");
				std::size_t n = at->nonbreak_chars;
				{
					auto mod = _t.get_modifier_for(at.get_node());
					mod->nonbreak_chars = 0;
					mod->ending = ui::line_ending::n;
				}
				_t.emplace_before(at, n, ui::line_ending::r);
				offset = 0;
				result.split = true;
			}
			if (at == _t.end()) {
				assert_true_logical(!_t.empty(), "corrupted line_ending_registry");
				--at;
				offset = at->nonbreak_chars;
			}
			if (lines.size() == 1) {
				_t.get_modifier_for(at.get_node())->nonbreak_chars += lines[0].nonbreak_chars;
			} else {
				// the last line
				_t.get_modifier_for(at.get_node())->nonbreak_chars += lines.back().nonbreak_chars - offset;
				// the first line
				auto it = _t.emplace_before(at, offset + lines[0].nonbreak_chars, lines[0].ending);
				// insert all other lines
				for (auto rest_it = lines.begin() + 1; rest_it != lines.end() - 1; ++rest_it) {
					_t.emplace_before(at, *rest_it);
				}
				result.merge_front = _try_merge_rn_linebreak(it);
				result.merge_back = _try_merge_rn_linebreak(at);
			}
			return result;
		}
		/// \overload
		///
		/// \param pos The position of the codepoint before which the clip will be inserted.
		/// \param lines The structure of inserted text.
		insert_result insert_codepoints(std::size_t pos, const std::vector<line_info> &lines) {
			line_column_info posinfo = get_line_and_column_of_codepoint(pos);
			return insert_codepoints(posinfo.line_iterator, posinfo.position_in_line, lines);
		}

		/// Called when a text clip has been erased from the buffer. \p end will remain valid after this operation.
		///
		/// \param beg Iterator to the line of the first erased codepoint.
		/// \param begoff The position of the first erased codepoint in the line.
		/// \param end Iterator to the line of the codepoint after the last erased char.
		/// \param endoff The position of the char after the last erased codepoint in the line.
		/// \return A \ref text_clip_info containing information about the removed text.
		erase_result erase_codepoints(iterator beg, std::size_t begoff, iterator end, std::size_t endoff) {
			assert_true_logical(!(end == _t.end() && endoff != 0), "invalid iterator position");
			if (beg == end && begoff == endoff) {
				return erase_result();
			}
			erase_result result;
			if (end == _t.end()) { // cannot erase EOF
				--end;
				endoff = end->nonbreak_chars;
			}
			if (beg->nonbreak_chars < begoff) { // break \r\n
				result.split_front = true;
				assert_true_logical(beg->ending == ui::line_ending::rn, "invalid begin padding");
				_t.get_modifier_for(beg.get_node())->ending = ui::line_ending::r;
				++beg;
				begoff = 0;
			}
			if (end != _t.end() && end->nonbreak_chars < endoff) { // break \r\n
				result.split_back = true;
				assert_true_logical(end->ending == ui::line_ending::rn, "invalid end padding");
				{
					auto mod = _t.get_modifier_for(end.get_node());
					endoff = mod->nonbreak_chars = (beg == end ? begoff : 0);
					mod->ending = ui::line_ending::n;
				}
			}
			if (beg != end) {
				_t.erase(beg, end);
			}
			_t.get_modifier_for(end.get_node())->nonbreak_chars += begoff - endoff;
			result.merge = _try_merge_rn_linebreak(end);
			return result;
		}
		/// \overload
		///
		/// \param beg The position of the first codepoint that will be erased.
		/// \param end The position past the last codepoint that will be erased.
		erase_result erase_codepoints(std::size_t beg, std::size_t end) {
			line_column_info
				begp = get_line_and_column_of_codepoint(beg),
				endp = get_line_and_column_of_codepoint(end);
			return erase_codepoints(begp.line_iterator, begp.position_in_line, endp.line_iterator, endp.position_in_line);
		}


		/// Returns the total number of linebreaks in the buffer. Add 1 to the result to obtain
		/// the total number of lines.
		std::size_t num_linebreaks() const {
			return _t.root() != nullptr ? _t.root()->synth_data.total_linebreaks : 0;
		}
		/// Returns the total number of characters in the buffer.
		std::size_t num_chars() const {
			return _t.root() != nullptr ? _t.root()->synth_data.total_chars : 0;
		}
		/// Clears all registered line information.
		void clear() {
			_t.clear();
			_t.emplace_before(_t.end());
		}
	protected:
		tree_type _t; ///< The underlying binary tree that stores the information of all lines.

		/// Used to obtain the number of codepoints before a given character.
		struct _pos_char_to_cp {
			/// The specialization of sum_synthesizer::index_finder used for this conversion.
			using finder = sum_synthesizer::index_finder<line_synth_data::num_chars_property, true>;
			/// Interface for \ref red_black_tree::tree::find_custom().
			int select_find(const node_type &n, std::size_t &c) {
				return finder::template select_find<
					line_synth_data::num_codepoints_property, line_synth_data::num_lines_property
				>(n, c, total_codepoints, total_lines);
			}
			std::size_t
				total_codepoints = 0, ///< Records the total number of codepoints before the given character.
				total_lines = 0; ///< Records the total number of lines before the given character.
		};
		/// Used to obtain the number of characters before a given codepoint.
		struct _pos_cp_to_char {
			/// The specialization of sum_synthesizer::index_finder used for this conversion.
			using finder = sum_synthesizer::index_finder<line_synth_data::num_codepoints_property, true>;
			/// Interface for \ref red_black_tree::tree::find_custom().
			int select_find(const node_type &n, std::size_t &c) {
				return finder::template select_find<
					line_synth_data::num_chars_property, line_synth_data::num_lines_property
				>(n, c, total_chars, total_lines);
			}
			std::size_t
				total_chars = 0, ///< Records the total number of characters before the given codepoint.
				total_lines = 0; ///< Records the total number of lines before the given codepoint.
		};
		/// Used to obtain the line an object (character, codepoint) is on.
		///
		/// \tparam Prop The property that indicates the type of the object.
		template <typename Prop> struct _get_line {
			/// The specialization of \ref sum_synthesizer::index_finder used for this conversion.
			using finder = sum_synthesizer::index_finder<Prop, true>;
			/// Interface for \ref red_black_tree::tree::find_custom().
			int select_find(const node_type &n, std::size_t &c) {
				return finder::template select_find<line_synth_data::num_lines_property>(n, c, total_lines);
			}
			std::size_t total_lines = 0; ///< Records the total number of lines before the given object.
		};
		/// Used to find the node corresponding to the i-th line.
		using _line_beg_finder = sum_synthesizer::index_finder<line_synth_data::num_lines_property>;
		/// Used to find the node corresponding to the i-th line and collect additional information in the process.
		template <typename AccumProp> struct _line_beg_accum_finder {
			/// Interface for \ref red_black_tree::tree::find_custom().
			int select_find(const node_type &n, std::size_t &l) {
				return _line_beg_finder::template select_find<AccumProp>(n, l, total);
			}
			std::size_t total = 0; ///< The additional data collected.
		};
		/// Used to find the node corresponding to the i-th line and the number of characters before it.
		using _line_beg_char_accum_finder = _line_beg_accum_finder<line_synth_data::num_chars_property>;
		/// Used to find the node corresponding to the i-th line and the number of codepoints before it.
		using _line_beg_codepoint_accum_finder = _line_beg_accum_finder<line_synth_data::num_codepoints_property>;

		/// Wrapper for \ref sum_synthesizer::sum_before when there's only one property.
		template <typename Prop> std::size_t _get_node_sum_before(tree_type::const_iterator it) const {
			std::size_t v = 0;
			sum_synthesizer::sum_before<Prop>(it, v);
			return v;
		}

		/// Tries to merge a \ref line_ending::r with a \ref line_ending::n, given an iterator that probably contains
		/// the \ref line_ending::n. The iterator remains valid after this operation even if the two lines are
		/// merged.
		///
		/// \return Whether the merge operation took place.
		bool _try_merge_rn_linebreak(iterator it) {
			if (it != _t.begin() && it != _t.end()) {
				if (it->nonbreak_chars == 0 && it->ending == ui::line_ending::n) {
					auto prev = it;
					--prev;
					if (prev->ending == ui::line_ending::r) { // bingo!
						std::size_t nc = prev->nonbreak_chars;
						_t.erase(prev);
						{
							auto mod = _t.get_modifier_for(it.get_node());
							mod->nonbreak_chars = nc;
							mod->ending = ui::line_ending::rn;
						}
						return true;
					}
				}
			}
			return false;
		}
	};
}
