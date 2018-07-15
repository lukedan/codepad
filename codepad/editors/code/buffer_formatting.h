#pragma once

/// \file
/// Structs used to manage the formatting of a buffer that's independent of the view.

#include "buffer.h"

namespace codepad::editor::code {
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
			line_info(size_t d, line_ending t) : nonbreak_chars(d), ending(t) {
			}

			size_t nonbreak_chars = 0; ///< The number of codepoints in this line, excluding the linebreak.
			/// The type of the line ending. This will be line_ending::none for the last line.
			line_ending ending = line_ending::none;
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
				inline static size_t get(const node_type &n) {
					return n.value.nonbreak_chars + get_linebreak_length(n.value.ending);
				}
			};
			/// Used to obtain the number of linebreaks that follows the line.
			struct get_node_linebreak_num {
				/// Returns 0 if the line is the last line of the buffer, 1 otherwise.
				inline static size_t get(const node_type &n) {
					return n.value.ending == line_ending::none ? 0 : 1;
				}
			};
			/// Used to obtain the number of characters in a line. The linebreak counts as one character even
			/// if it's line_ending::rn.
			struct get_node_char_num {
				/// Returns line_info::nonbreak_chars plus the value returned by get_node_linebreak_num::get().
				inline static size_t get(const node_type &n) {
					return n.value.nonbreak_chars + get_node_linebreak_num::get(n);
				}
			};

			size_t
				total_codepoints = 0, ///< The total number of codepoints in the subtree.
				node_codepoints = 0, ///< The number of codepoints in the line.
				total_chars = 0, ///< The total number of characters in the subtree.
				node_chars = 0, ///< The number of characters in the line.
				total_linebreaks = 0; ///< The total number of linebreaks in the subtree.

			/// Property used to calculate the number of codepoints in a range of lines.
			using num_codepoints_property = sum_synthesizer::property<
				get_node_codepoint_num,
				&line_synth_data::node_codepoints, &line_synth_data::total_codepoints
			>;
			/// Property used to calculate the number of characters in a range of lines.
			using num_chars_property = sum_synthesizer::property<
				get_node_char_num,
				&line_synth_data::node_chars, &line_synth_data::total_chars
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
		using tree_type = binary_tree<line_info, line_synth_data>;
		/// A node of the binary tree.
		using node_type = tree_type::node;
		/// A const iterator through the nodes of the tree.
		using iterator = tree_type::const_iterator;
		/// Stores the line and column of a certain character.

		struct line_column_info {
			/// Default constructor.
			line_column_info() = default;
			/// Constructor that initializes the struct with the given values.
			line_column_info(iterator lit, size_t l, size_t c) : line_iterator(lit), line(l), column(c) {
			}

			iterator line_iterator; ///< An iterator to the line corresponding to \ref line.
			size_t
				line = 0, ///< The line that the character is on.
				column = 0; ///< The column that the character is on.
		};
		/// Stores information of a text clip, including the number of characters, and the lengths and line endings
		/// of each line in the text clip.
		struct text_clip_info {
			/// Default constructor.
			text_clip_info() = default;
			/// Initializes all fields of this struct.
			text_clip_info(size_t c, std::vector<line_info> ls) :
				total_chars(c), lines(std::move(ls)) {
			}
			size_t total_chars = 0; ///< The total number of characters in the text clip.
			std::vector<line_info> lines; ///< The information of all invidual lines.

			/// Appends a line to this struct.
			void append_line(size_t nonbreak_chars, line_ending ending) {
				total_chars += nonbreak_chars;
				if (ending != line_ending::none) {
					++total_chars;
				}
				lines.emplace_back(nonbreak_chars, ending);
			}
			/// \overload
			void append_line(line_info line) {
				append_line(line.nonbreak_chars, line.ending);
			}
		};


		/// Calls clear() to initialize the tree to contain a single empty line with no linebreaks.
		linebreak_registry() {
			clear();
		}

		/// Stores information about a line in the tree.
		struct linebreak_info {
			/// Default constructor.
			linebreak_info() = default;
			/// Initializes all field of this struct.
			linebreak_info(const iterator &it, size_t fc) : entry(it), first_char(fc) {
			}
			iterator entry; ///< An iterator to the line.
			/// The number of characters before the first character of the line in the whole buffer.
			size_t first_char = 0;
		};

		/// Returns the number of codepoints before the character at the given index.
		size_t position_char_to_codepoint(size_t c) const {
			_pos_char_to_cp selector;
			_t.find_custom(selector, c);
			return selector.total_codepoints + c;
		}
		/// Returns the number of characters before the codepoint at the given index.
		size_t position_codepoint_to_char(size_t c) const {
			_pos_cp_to_char selector;
			auto it = _t.find_custom(selector, c);
			return selector.total_chars + std::min(c, it->nonbreak_chars);
		}
		/// Returns a \ref linebreak_info containing information about the given line.
		linebreak_info get_line_info(size_t l) const {
			_line_beg_char_accum_finder selector;
			auto it = _t.find_custom(selector, l);
			return linebreak_info(it, selector.total);
		}
		/// Returns the position of the first codepoint of the given line.
		size_t get_beginning_codepoint_of_line(size_t l) const {
			_line_beg_codepoint_accum_finder selector;
			_t.find_custom(selector, l);
			return selector.total;
		}

		/// Returns an iterator to the first line.
		iterator begin() const {
			return _t.begin();
		}
		/// Returns an iterator past the last line.
		iterator end() const {
			return _t.end();
		}
		/// Returns an iterator to the specified line.
		iterator at_line(size_t line) const {
			return _t.find_custom(_line_beg_finder(), line);
		}

		/// Returns a \ref line_column_info containing information about the codepoint at the given index.
		/// If the codepoint is at the end of the buffer (i.e., EOF), the returned iterator will still be end() - 1.
		line_column_info get_line_and_column_of_codepoint(size_t cp) const {
			_get_line<line_synth_data::num_codepoints_property> selector;
			iterator it = _t.find_custom(selector, cp);
			return line_column_info(it, selector.total_lines, cp);
		}
		/// Returns a \ref line_column_info containing information about the character at the given index.
		/// If the character is at the end of the buffer (i.e., EOF), the returned iterator will still be end() - 1.
		line_column_info get_line_and_column_of_char(size_t c) const {
			_get_line<line_synth_data::num_chars_property> selector;
			iterator it = _t.find_custom(selector, c);
			return line_column_info(it, selector.total_lines, c);
		}
		/// Returns a \ref line_column_info containing information about the character at the given index,
		/// and the index of the codepoint corresponding to the character. If the character is at the end
		/// of the buffer (i.e., EOF), the returned iterator will still be end() - 1.
		std::pair<line_column_info, size_t> get_line_and_column_and_codepoint_of_char(size_t c) const {
			_pos_char_to_cp selector;
			iterator it = _t.find_custom(selector, c);
			return {line_column_info(it, selector.total_lines, c), selector.total_codepoints + c};
		}

		/// Returns the index of the line to which the given iterator points.
		size_t get_line(iterator i) const {
			return _get_node_sum_before<line_synth_data::num_lines_property>(i.get_node());
		}
		/// Returns the index of the first codepoint of the line corresponding to the given iterator.
		size_t get_beginning_codepoint_of(iterator i) const {
			return _get_node_sum_before<line_synth_data::num_codepoints_property>(i.get_node());
		}
		/// Returns the index of the first character of the line corresponding to the given iterator.
		size_t get_beginning_char_of(iterator i) const {
			return _get_node_sum_before<line_synth_data::num_chars_property>(i.get_node());
		}

		/// Called when a text clip has been inserted to the buffer.
		///
		/// \param at The line at which the text is to be inserted.
		/// \param offset The position in the line at whith the text is to be inserted.
		/// \param lines Lines of the text clip.
		void insert_chars(iterator at, size_t offset, const std::vector<line_info> &lines) {
			assert_true_logical(!(at == _t.end() && offset != 0), "invalid insert position");
			assert_true_logical(!lines.empty() && lines.back().ending == line_ending::none, "invalid text");
			if (at == _t.end()) { // insert at end
				node_type *maxn = _t.max();
				assert_true_logical(maxn != nullptr, "corrupted line_ending_registry");
				{
					auto mod = _t.get_modifier_for(maxn);
					mod->ending = lines[0].ending;
					mod->nonbreak_chars += lines[0].nonbreak_chars;
				}
				_t.insert_range_before_copy(at, lines.begin() + 1, lines.end());
			} else {
				if (lines.size() == 1) {
					_t.get_modifier_for(at.get_node())->nonbreak_chars += lines[0].nonbreak_chars;
				} else {
					// the last line
					_t.get_modifier_for(at.get_node())->nonbreak_chars += lines.back().nonbreak_chars - offset;
					// the first line
					_t.emplace_before(at, offset + lines[0].nonbreak_chars, lines[0].ending);
					_t.insert_range_before_copy(at, lines.begin() + 1, lines.end() - 1); // all other lines
				}
			}
		}
		/// \overload
		void insert_chars(iterator at, size_t offset, const text_clip_info &clipstats) {
			insert_chars(at, offset, clipstats.lines);
		}

		/// Called when a text clip has been erased from the buffer.
		///
		/// \param beg Iterator to the line of the first erased char.
		/// \param begoff The position of the first erased char in the line.
		/// \param end Iterator to the line of the char after the last erased char.
		/// \param endoff The position of the char after the last erased char in the line.
		/// \return A \ref text_clip_info containing information about the removed text.
		text_clip_info erase_chars(iterator beg, size_t begoff, iterator end, size_t endoff) {
			assert_true_logical(!(end == _t.end() && endoff != 0), "invalid iterator position");
			if (end == _t.end()) { // cannot erase EOF
				--end;
				endoff = end->nonbreak_chars;
			}
			text_clip_info stats;
			if (beg == end) {
				stats.append_line(endoff - begoff, line_ending::none);
			} else {
				stats.append_line(beg->nonbreak_chars - begoff, beg->ending);
				iterator it = beg;
				for (++it; it != end; ++it) {
					stats.append_line(it->nonbreak_chars, it->ending);
				}
				stats.append_line(endoff, line_ending::none);
				_t.erase(beg, end);
			}
			_t.get_modifier_for(end.get_node())->nonbreak_chars += begoff - endoff;
			return stats;
		}
		/// Called when a text clip has been erased from the buffer.
		///
		/// \param beg The index of the first char that has been erased.
		/// \param end One plus the index of the last char that has been erased.
		void erase_chars(size_t beg, size_t end) {
			line_column_info
				begp = get_line_and_column_of_char(beg),
				endp = get_line_and_column_of_char(end);
			erase_chars(begp.line_iterator, begp.column, endp.line_iterator, endp.column);
		}

		/// Returns the total number of linebreaks in the buffer. Add 1 to the result to obtain
		/// the total number of lines.
		size_t num_linebreaks() const {
			return _t.root() ? _t.root()->synth_data.total_linebreaks : 0;
		}
		/// Returns the total number of characters in the buffer.
		size_t num_chars() const {
			return _t.root() ? _t.root()->synth_data.total_chars : 0;
		}
		/// Clears all registered line information.
		void clear() {
			_t.clear();
			_t.emplace_before(nullptr);
		}
	protected:
		tree_type _t; ///< The underlying binary tree that stores the information of all lines.

		/// Used to obtain the number of codepoints before a given character.
		struct _pos_char_to_cp {
			/// The specialization of sum_synthesizer::index_finder used for this conversion.
			using finder = sum_synthesizer::index_finder<line_synth_data::num_chars_property, true>;
			/// Interface for binary_tree::find_custom.
			int select_find(const node_type &n, size_t &c) {
				return finder::template select_find<
					line_synth_data::num_codepoints_property, line_synth_data::num_lines_property
				>(n, c, total_codepoints, total_lines);
			}
			size_t
				total_codepoints = 0, ///< Records the total number of codepoints before the given character.
				total_lines = 0; ///< Records the total number of lines before the given character.
		};
		/// Used to obtain the number of characters before a given codepoint.
		struct _pos_cp_to_char {
			/// The specialization of sum_synthesizer::index_finder used for this conversion.
			using finder = sum_synthesizer::index_finder<line_synth_data::num_codepoints_property, true>;
			/// Interface for binary_tree::find_custom.
			int select_find(const node_type &n, size_t &c) {
				return finder::template select_find<line_synth_data::num_chars_property>(n, c, total_chars);
			}
			size_t total_chars = 0; ///< Records the total number of characters before the given codepoint.
		};
		/// Used to obtain the line an object (character, codepoint) is on.
		///
		/// \tparam Prop The property that indicates the type of the object.
		template <typename Prop> struct _get_line {
			/// The specialization of sum_synthesizer::index_finder used for this conversion.
			using finder = sum_synthesizer::index_finder<Prop, true>;
			/// Interface for binary_tree::find_custom.
			int select_find(const node_type &n, size_t &c) {
				return finder::template select_find<line_synth_data::num_lines_property>(n, c, total_lines);
			}
			size_t total_lines = 0; ///< Records the total number of lines before the given object.
		};
		/// Used to find the node corresponding to the i-th line.
		using _line_beg_finder = sum_synthesizer::index_finder<line_synth_data::num_lines_property>;
		/// Used to find the node corresponding to the i-th line and collect
		/// additional information in the process.
		template <typename AccumProp> struct _line_beg_accum_finder {
			/// Interface for binary_tree::find_custom.
			int select_find(const node_type &n, size_t &l) {
				return _line_beg_finder::template select_find<AccumProp>(n, l, total);
			}
			size_t total = 0; ///< The additional data collected.
		};
		/// Used to find the node corresponding to the i-th line and the number of characters before it.
		using _line_beg_char_accum_finder = _line_beg_accum_finder<line_synth_data::num_chars_property>;
		/// Used to find the node corresponding to the i-th line and the number of codepoints before it.
		using _line_beg_codepoint_accum_finder = _line_beg_accum_finder<line_synth_data::num_codepoints_property>;

		/// Wrapper for \ref sum_synthesizer::sum_before when there's only one property.
		template <typename Prop> size_t _get_node_sum_before(node_type *n) const {
			size_t v = 0;
			sum_synthesizer::sum_before<Prop>(_t.get_const_iterator_for(n), v);
			return v;
		}
	};
}
