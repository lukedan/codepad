// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Contains classes used to format a view of a \ref codepad::editors::code::interpretation.

#include "interpretation.h"

namespace codepad::editors::code {
	/// The type of a linebreak.
	enum class linebreak_type {
		soft, ///< A soft linebreak, caused by wrapping.
		hard ///< An actual linkebreak in the original text.
	};
	/// Keeps track of all soft linebreaks in a view.
	class soft_linebreak_registry {
	public:
		/// Default constructor.
		soft_linebreak_registry() = default;
		/// Initializes the registry with the associated \ref linebreak_registry.
		explicit soft_linebreak_registry(const linebreak_registry &reg) : _reg(&reg) {
		}

		/// Stores the number of characters between two consecutive soft linebreaks.
		struct node_data {
			/// Default constructor.
			node_data() = default;
			/// Initializes the struct with the given length.
			explicit node_data(std::size_t l) : length(l) {
			}

			std::size_t length = 0; ///< The number of characters between the two soft linebreaks.
			red_black_tree::color color = red_black_tree::color::red; ///< The color of this node.
		};
		/// Stores additional synthesized data of a subtree.
		///
		/// \sa node_data
		struct node_synth_data {
			/// The type of a node.
			using node_type = binary_tree_node<node_data, node_synth_data>;

			std::size_t
				total_length = 0, ///< The total number of characters in the subtree.
				total_softbreaks = 0; ///< The total number of soft linebreaks in the subtree.

			using length_property = sum_synthesizer::compact_property<
				synthesization_helper::field_value_property<&node_data::length>,
				&node_synth_data::total_length
			>; ///< Property used to obtain the total number of characters in a subtree.
			using softbreaks_property = sum_synthesizer::compact_property<
				synthesization_helper::identity, &node_synth_data::total_softbreaks
			>; ///< Property used to obtain the total number of soft linebreaks in a subtree.

			/// Calls \ref sum_synthesizer::synthesize to update the recorded values.
			inline static void synthesize(node_type &n) {
				sum_synthesizer::synthesize<length_property, softbreaks_property>(n);
			}
		};
		/// The type of the tree.
		using tree_type = red_black_tree::tree<
			node_data, red_black_tree::member_red_black_access<&node_data::color>, node_synth_data
		>;
		/// The type of a node in the tree.
		using node_type = typename tree_type::node;
		/// Const iterators to elements in the tree.
		using iterator = typename tree_type::const_iterator;

		/// Used to wrap up the results of a query.
		struct softbreak_info {
			/// Default constructor.
			softbreak_info() = default;
			/// Initializes all fields of the struct.
			softbreak_info(const iterator &it, std::size_t pc, std::size_t pl) :
				entry(it), prev_chars(pc), prev_softbreaks(pl) {
			}
			iterator entry; ///< Iterator to the corresponding soft `segment'.
			std::size_t
				prev_chars = 0, ///< The number of characters before the beginning of \ref entry.
				prev_softbreaks = 0; ///< The number of soft linebreaks before the one \ref entry points to.
		};

		/// Obtains information about a visual line.
		///
		/// \sa _find_line_ending
		std::pair<linebreak_registry::linebreak_info, softbreak_info> get_line_info(std::size_t line) const {
			if (_t.empty()) {
				return {_reg->get_line_info(line), softbreak_info(_t.end(), 0, 0)};
			}
			std::size_t softc = 0, hardc = 0;
			iterator soft;
			linebreak_registry::iterator hard;
			std::size_t slb = _find_line_ending(soft, hard, softc, hardc, line);
			return {
				linebreak_registry::linebreak_info(hard, hardc),
				softbreak_info(soft, softc, slb)
			};
		}
		/// Obtains the position of the given line's beginning and the type of the linebreak before the line.
		std::pair<std::size_t, linebreak_type> get_beginning_char_of_visual_line(std::size_t line) const {
			if (_t.empty()) {
				return {_reg->get_line_info(line).first_char, linebreak_type::hard};
			}
			std::size_t softc = 0, hardc = 0;
			iterator soft;
			linebreak_registry::iterator hard;
			_find_line_ending(soft, hard, softc, hardc, line);
			if (softc > hardc) {
				return {softc, linebreak_type::soft};
			}
			return {hardc, linebreak_type::hard};
		}
		/// Obtains the position after the given line's ending and the type of the linebreak after the line.
		/// If the line ends with a nonempty hard linebreak, the returned position is that of the linebreak.
		/// Otherwise, the position is that of the next line's beginning, or after the end of the document
		/// if it's the last line.
		std::pair<std::size_t, linebreak_type> get_past_ending_char_of_visual_line(std::size_t line) const {
			if (_t.empty()) {
				linebreak_registry::linebreak_info info = _reg->get_line_info(line);
				if (info.entry != _reg->end()) {
					info.first_char += info.entry->nonbreak_chars;
				}
				return {info.first_char, linebreak_type::hard};
			}
			std::size_t softc = 0, hardc = 0;
			iterator soft;
			linebreak_registry::iterator hard;
			_find_line_ending(soft, hard, softc, hardc, line);
			if (hard != _reg->end()) {
				hardc += hard->nonbreak_chars;
				if (soft != _t.end()) {
					softc += soft->length;
					if (softc < hardc) {
						return {softc, linebreak_type::soft};
					}
				}
			}
			return {hardc, linebreak_type::hard};
		}
		/// Obtains information about the soft `segment' that the given position is at. If the position is at a soft
		/// linebreak, the segment after it is returned.
		softbreak_info get_softbreak_before_or_at_char(std::size_t c) const {
			_get_softbreaks_before selector;
			std::size_t nc = c;
			auto it = _t.find(selector, nc);
			return softbreak_info(it, c - nc, selector.num_softbreaks);
		}
		/// Returns the index of the visual line that the given character is on.
		std::size_t get_visual_line_of_char(std::size_t c) const {
			return
				_reg->get_line_and_column_of_char(c).line +
				get_softbreak_before_or_at_char(c).prev_softbreaks;
		}
		/// Returns the visual line and column of a given character.
		std::pair<std::size_t, std::size_t> get_visual_line_and_column_of_char(std::size_t c) const {
			auto hard = _reg->get_line_and_column_of_char(c);
			_get_softbreaks_before selector;
			_t.find(selector, c);
			return {hard.line + selector.num_softbreaks, std::min(c, hard.position_in_line)};
		}
		/// Returns the combined result of \ref get_visual_line_and_column_of_char and
		/// \ref get_softbreak_before_or_at_char.
		std::tuple<std::size_t, std::size_t, softbreak_info>
			get_visual_line_and_column_and_softbreak_before_or_at_char(std::size_t c) const {
			std::size_t nc = c;
			auto hard = _reg->get_line_and_column_of_char(c);
			_get_softbreaks_before selector;
			auto it = _t.find(selector, nc);
			return {
				hard.line + selector.num_softbreaks,
				std::min(nc, hard.position_in_line),
				softbreak_info(it, c - nc, selector.num_softbreaks)
			};
		}

		/// Returns an iterator to the first soft linebreak.
		iterator begin() const {
			return _t.begin();
		}
		/// Returns an iterator past the last soft linebreak.
		iterator end() const {
			return _t.end();
		}

		/// Deletes all soft linebreaks.
		void clear_softbreaks() {
			_t.clear();
		}
		/// Sets the contents of this registry.
		///
		/// \param poss The new list of soft linebreaks' positions, sorted in increasing order.
		void set_softbreaks(const std::vector<std::size_t> &poss) {
			_t.clear();

			std::size_t last = 0; // no reason to insert softbreak at position 0
			for (std::size_t cp : poss) {
				assert_true_usage(cp > last, "softbreak list not properly sorted");
				_t.emplace_before(_t.end(), cp - last);
				last = cp;
			}
		}

		/// Returns the total number of soft linebreaks.
		std::size_t num_softbreaks() const {
			return _t.root() ? _t.root()->synth_data.total_softbreaks : 0;
		}
		/// Returns the total number of visual lines.
		std::size_t num_visual_lines() const {
			return _reg->num_linebreaks() + num_softbreaks() + 1;
		}
		/// Returns the associated \ref linebreak_registry.
		const linebreak_registry &get_hard_linebreaks() const {
			return *_reg;
		}
	protected:
		/// Used to obtain the number of soft linebreaks before a given position.
		struct _get_softbreaks_before {
			/// The underlying \ref sum_synthesizer::index_finder.
			using finder = sum_synthesizer::index_finder<node_synth_data::length_property>;
			/// Interface to \ref red_black_tree::tree::find_custom().
			int select_find(node_type &n, std::size_t &target) {
				return finder::template select_find<
					node_synth_data::softbreaks_property
				>(n, target, num_softbreaks);
			}
			std::size_t num_softbreaks = 0; ///< Records the number of soft linebreaks before the resulting node.
		};

		tree_type _t; ///< The underlying binary tree that records all soft linebreaks.
		const linebreak_registry *_reg = nullptr; ///< The associated \ref linebreak_registry.


		/// Obtains information about the visual line at the given position. For the visual line with the given
		/// line number, it returns two iterators, one to a soft `segment' and one to a hard `segment', that contain
		/// the visual line. Values indicating the positions of these iterators are also returned. If the file
		/// has less visual lines than requested, both iterators will point to the end of their containers.
		///
		/// \param softit Contains the resulting iterator to the soft linebreak when this function returns.
		/// \param hardit Contains the resulting iterator to the hard linebreak when this function returns.
		/// \param softc Contains the number of characters before \p softit when this function returns.
		/// \param hardc Contains the number of characters before \p hardit when this function returns.
		/// \param line The visual line to be queried.
		/// \return The number of soft linebreaks before \p softit. Subtract this from \p line to obtain the
		///         number of hard linebreaks before \p hardit.
		std::size_t _find_line_ending(
			iterator &softit, linebreak_registry::iterator &hardit,
			std::size_t &softc, std::size_t &hardc, std::size_t line
		) const {
			assert_true_logical(softc == 0 && hardc == 0, "irresponsible caller");
			std::size_t softbreaks = num_softbreaks(), res = softbreaks;
			if (line > softbreaks + _reg->num_linebreaks()) { // past the end
				softit = _t.end();
				softc = _t.root()->synth_data.total_length;
				hardit = _reg->end();
				hardc = _reg->num_chars();
				return softbreaks;
			}
			if (line >= softbreaks) { // first candidate result
				softit = _t.end();
				softc = _t.root()->synth_data.total_length;
				auto hres = _reg->get_line_info(line - softbreaks);
				hardit = hres.entry;
				hardc = hres.first_char;
			}
			node_type *mynode = _t.root();
			linebreak_registry::node_type *theirnode = _reg->_t.root();
			// the four following variables contain the sum of values of all nodes before (not including) the
			// current node, but not including nodes in its left subtree
			std::size_t myanccount = 0, myancpos = 0, theiranccount = 0, theirancpos = 0;
			while (mynode && theirnode) {
				std::size_t
					mycount = myanccount, myexcpos = myancpos,
					theircount = theiranccount, theirexcpos = theirancpos;
				if (mynode->left) {
					mycount += mynode->left->synth_data.total_softbreaks;
					myexcpos += mynode->left->synth_data.total_length;
				}
				if (theirnode->left) {
					theircount += theirnode->left->synth_data.total_linebreaks;
					theirexcpos += theirnode->left->synth_data.total_chars;
				}
				std::size_t
					mypos = myexcpos + mynode->value.length,
					theirpos = theirexcpos + linebreak_registry::line_synth_data::get_node_char_num::get(*theirnode),
					before = mycount + theircount;

				if (before > line) {
					// one of the nodes right before mynode and theirnode is not in the part on the left, must be the
					// larger one
					if (myexcpos > theirexcpos) {
						mynode = mynode->left;
					} else {
						theirnode = theirnode->left;
					}
				} else if (before < line) {
					// one of mynode and theirnode is in the part on the left, must be the smaller one
					if (mypos < theirpos) {
						mynode = mynode->right;
						myanccount = mycount + 1;
						myancpos = mypos;
					} else {
						theirnode = theirnode->right;
						theiranccount = theircount + 1;
						theirancpos = theirpos;
					}
				} else { // new candidate
					// basically, a candidate linebreak of the same type as the first current result linebreak cannot
					// be between the two current result linebreaks, or it's not the result
					if (!(
						(softc > myexcpos && softc < theirexcpos) ||
						(hardc > theirexcpos && hardc < myexcpos)
						)) {
						softit = _t.get_iterator_for(mynode);
						softc = myexcpos;
						hardit = _reg->_t.get_iterator_for(theirnode);
						hardc = theirexcpos;
						res = mycount;
					}
					// if myexcpos > theirexcpos and mynode goes to the left branch, then the original mynode will
					// always be between the new mynode and the new theirnode, and vice versa
					if (myexcpos > theirexcpos) {
						mynode = mynode->left;
						theirnode = theirnode->right;
						theiranccount = theircount + 1;
						theirancpos = theirpos;
					} else {
						theirnode = theirnode->left;
						mynode = mynode->right;
						myanccount = mycount + 1;
						myancpos = mypos;
					}
				}
			}
			return res;
		}
	};

	/// Records all folded areas in a view.
	///
	/// \todo Remove redundant fields in nodes.
	struct folding_registry {
		friend class view_formatting;
	public:
		/// Contains information about a folded region.
		struct fold_region_data {
			/// Default constructor.
			fold_region_data() = default;
			/// Initializes all fields of this struct.
			fold_region_data(std::size_t b, std::size_t e, std::size_t bl, std::size_t el) :
				begin(b), end(e), begin_line(bl), end_line(el) {
			}

			std::size_t
				begin = 0, ///< The beginning of the folded region.
				end = 0, ///< The ending of the folded region.
				begin_line = 0, ///< The visual line that \ref begin is on.
				end_line = 0; ///< The visual line that \ref end is on.
		};
		/// A node in the tree that contains information about one folded region.
		struct fold_region_node_data {
			/// Default constructor.
			fold_region_node_data() = default;
			/// Initializes all fields of this struct.
			fold_region_node_data(std::size_t g, std::size_t r, std::size_t gl, std::size_t rl) :
				gap(g), range(r), gap_lines(gl), folded_lines(rl) {
			}

			std::size_t
				/// The gap between the ending of the last folded region (or the beginning of the document
				/// if this is the first region) and the beginning of this folded region.
				gap = 0,
				range = 0, ///< The length of this folded region, i.e., the number of characters folded.
				/// The number of linebreaks (including both hard and soft ones) that \ref gap covers.
				gap_lines = 0,
				/// The number of linebreaks that \ref range covers. They can only be hard linebreaks.
				folded_lines = 0,
				bytepos_first = 0, ///< Position of the first byte of this folded region.
				bytepos_second = 0; ///< Position past the last byte of this folded region.
			red_black_tree::color color = red_black_tree::color::red; ///< The color of this node.
		};
		/// Contains additional synthesized data of a subtree.
		///
		/// \sa fold_region_node_data
		struct fold_region_synth_data {
			/// A node in a tree.
			using node_type = binary_tree_node<fold_region_node_data, fold_region_synth_data>;
			/// Used to obtain the total number of characters that a node covers.
			struct get_node_span {
				/// Returns the sum of \ref fold_region_node_data::gap and \ref fold_region_node_data::range.
				inline static std::size_t get(const node_type &n) {
					return n.value.gap + n.value.range;
				}
			};
			/// Used to obtain the total number of linebreaks (both hard and soft ones) that a node covers.
			struct get_node_line_span {
				/// Returns the sum of \ref fold_region_node_data::gap_lines and
				/// \ref fold_region_node_data::folded_lines.
				inline static std::size_t get(const node_type &n) {
					return n.value.gap_lines + n.value.folded_lines;
				}
			};

			std::size_t
				total_length = 0, ///< The total number of characters in this subtree.
				total_folded_chars = 0, ///< The total number of characters that are folded in this subtree.
				total_lines = 0, ///< The total number of linebreaks (both soft and hard ones) in this subtree.
				/// The total number of folded linebreaks in this subtree. They can only be hard linebreaks.
				total_folded_lines = 0,
				tree_size = 0; ///< The number of nodes in this subtree.

			using span_property = sum_synthesizer::compact_property<
				get_node_span, &fold_region_synth_data::total_length
			>; ///< Property used to obtain the total number of characters in a subtree.
			using folded_chars_property = sum_synthesizer::compact_property<
				synthesization_helper::field_value_property<&fold_region_node_data::range>,
				&fold_region_synth_data::total_folded_chars
			>; ///< Property used to obtain the total number of folded characters in a subtree.
			using line_span_property = sum_synthesizer::compact_property<
				get_node_line_span, &fold_region_synth_data::total_lines
			>; ///< Property used to obtain the total number of linebreaks (both soft and hard ones) in a subtree.
			/// Property used to obtain the total number of folded linebreaks (both soft and hard ones) in a subtree.
			using folded_lines_property = sum_synthesizer::compact_property<
				synthesization_helper::field_value_property<&fold_region_node_data::folded_lines>,
				&fold_region_synth_data::total_folded_lines
			>;
			using tree_size_property = sum_synthesizer::compact_property<
				synthesization_helper::identity, &fold_region_synth_data::tree_size
			>; ///< Property used to obtain the total number of nodes in a subtree.

			/// Base struct of properties that obtains data only of unfolded text. This is useful since
			/// \ref fold_region_node_data stores data of the folded region and the unfolded region before it,
			/// while \ref fold_region_synth_data stores data of the folded region and the whole region.
			/// Such properties are only used by finders when querying unfolded positions with folded ones.
			///
			/// \tparam Node The corresponding field of \ref fold_region_node_data.
			/// \tparam TreeTotal The field of this struct that holds to the data of the whole region.
			/// \tparam TreeFolded The field of this struct that holds to the data of the folded region.
			template <
				std::size_t fold_region_node_data::*Node,
				std::size_t fold_region_synth_data::*TreeTotal, std::size_t fold_region_synth_data::*TreeFolded
			> struct unfolded_property_base {
				/// Returns the value of \p Node.
				inline static std::size_t get_node_value(const node_type &n) {
					return n.value.*Node;
				}
				/// The same as \ref get_node_value.
				inline static std::size_t get_node_synth_value(const node_type &n) {
					return get_node_value(n);
				}
				/// Returns the difference between \p TreeFolded and \p TreeTotal.
				inline static std::size_t get_tree_synth_value(const node_type &n) {
					return n.synth_data.*TreeTotal - n.synth_data.*TreeFolded;
				}
			};
			using unfolded_length_property = unfolded_property_base<
				&fold_region_node_data::gap,
				&fold_region_synth_data::total_length, &fold_region_synth_data::total_folded_chars
			>; ///< Property used to obtain the total number of unfolded characters in a subtree.
			/// Property used to obtain the total number of unfolded linbreaks (both soft and hard ones) in a subtree.
			using unfolded_lines_property = unfolded_property_base<
				&fold_region_node_data::gap_lines,
				&fold_region_synth_data::total_lines, &fold_region_synth_data::total_folded_lines
			>;

			/// Calls \ref sum_synthesizer::synthesize to update the stored values.
			inline static void synthesize(node_type &n) {
				sum_synthesizer::synthesize<
					span_property, folded_chars_property,
					line_span_property, folded_lines_property, tree_size_property
				>(n);
			}
		};
		/// Trees for storing folded regions.
		using tree_type = red_black_tree::tree<
			fold_region_node_data,
			red_black_tree::member_red_black_access<&fold_region_node_data::color>,
			fold_region_synth_data
		>;
		using node_type = tree_type::node; ///< Nodes of \ref tree_type.
		using iterator = tree_type::const_iterator; ///< Const iterators through the registry.

		/// Given a line index in the document with folding enabled, returns the index of the same line when folding
		/// is disabled.
		std::size_t folded_to_unfolded_line_number(std::size_t line) const {
			_folded_to_unfolded_line finder;
			_t.find(finder, line);
			return finder.total + line;
		}
		/// Given a line index in the document with folding disabled, returns the index of the same line when folding
		/// is enabled.
		std::size_t unfolded_to_folded_line_number(std::size_t line) const {
			_unfolded_to_folded_line finder;
			auto it = _t.find(finder, line);
			if (it != _t.end()) {
				line = std::min(line, it->gap_lines);
			}
			return finder.total_unfolded + line;
		}
		/// Given a caret position in the document with folding enabled (i.e., as if all folded regions have been
		/// removed), returns the corresponding position in the document with folding disabled.
		std::size_t folded_to_unfolded_caret_pos(std::size_t pos) const {
			_folded_to_unfolded_pos finder;
			_t.find(finder, pos);
			return finder.total + pos;
		}
		/// Given a caret position in the document with folding disabled, returns the corresponding position in the
		/// document with folding enabled (i.e., as if all folded regions have been removed).
		std::size_t unfolded_to_folded_caret_pos(std::size_t pos) const {
			_unfolded_to_folded_pos finder;
			auto it = _t.find(finder, pos);
			if (it != _t.end()) {
				pos = std::min(pos, it->gap);
			}
			return finder.total_unfolded + pos;
		}

		/// Given a line index in the document with folding disabled, returns the index (with folding disabled) of
		/// the first of the set of lines that, with folding enabled, belongs to the line at the given index.
		std::size_t get_beginning_line_of_folded_lines(std::size_t line) const {
			return folded_to_unfolded_line_number(unfolded_to_folded_line_number(line));
		}
		/// Given a line index in the document with folding disabled, returns the index (with folding disabled) past
		/// the last of the set of lines that, with folding enabled, belongs to the line at the given index.
		std::size_t get_past_ending_line_of_folded_lines(std::size_t line) const {
			return folded_to_unfolded_line_number(unfolded_to_folded_line_number(line) + 1);
		}

		/// Stores information about a fold region. Used to pack query results.
		struct fold_region_info {
			/// Default constructor.
			fold_region_info() = default;
			/// Initializes all fields of this struct.
			fold_region_info(const iterator &it, std::size_t pc, std::size_t pl) :
				entry(it), prev_chars(pc), prev_lines(pl) {
			}

			iterator entry; ///< Iterator to the resulting fold region.
			std::size_t
				prev_chars = 0, ///< The number of characters before \ref entry.
				prev_lines = 0; ///< The number of linebreaks before \ref entry.

			/// Moves \ref entry to point to the next fold region, adjusting \ref prev_chars and \ref prev_lines
			/// accordingly. The caller is responsible of checking whether the iterator is already at the end.
			void move_next() {
				assert_true_logical(
					entry != entry.get_container()->end(), "iterator to fold region already at the end"
				);
				prev_chars += entry->gap + entry->range;
				prev_lines += entry->gap_lines + entry->folded_lines;
				++entry;
			}
			/// Moves \ref entry to point to the previous fold region, adjusting \ref prev_chars and \ref prev_lines
			/// accordingly. The caller is responsible of checking whether the iterator is already at the beginning.
			void move_prev() {
				assert_true_logical(
					entry != entry.get_container()->begin(), "iterator to fold region already at the beginning"
				);
				--entry;
				prev_chars -= entry->gap + entry->range;
				prev_lines -= entry->gap_lines + entry->folded_lines;
			}
		};

		/// Finds the folded region that fully encapsules the given position, i.e., the position does not lie on the
		/// boundary of the result. If no such region is found, returns (\ref end(), 0, 0).
		fold_region_info find_region_containing_open(std::size_t cp) const {
			_find_region_open finder;
			auto it = _t.find(finder, cp);
			if (it != _t.end() && cp > it->gap) {
				return fold_region_info(it, finder.total_chars, finder.total_lines);
			}
			return fold_region_info(_t.end(), 0, 0);
		}
		/// Finds the folded region that encapsules the given position. The position may lie on the boundary of the
		/// result. If two such regions exist (happens when the position is between two adjancent folded regions),
		/// the one in front is returned. If no such region is found, returns (\ref end(), 0, 0).
		fold_region_info find_region_containing_closed(std::size_t cp) const {
			_find_region_closed finder;
			auto it = _t.find(finder, cp);
			if (it != _t.end() && cp >= it->gap) {
				return fold_region_info(it, finder.total_chars, finder.total_lines);
			}
			return fold_region_info(_t.end(), 0, 0);
		}
		/// Similar to \ref find_region_containing_open, but returns the first folded region after the given
		/// position if no such region is found.
		fold_region_info find_region_containing_or_first_after_open(std::size_t cp) const {
			_find_region_open finder;
			auto it = _t.find(finder, cp);
			return fold_region_info(it, finder.total_chars, finder.total_lines);
		}
		/// Similar to \ref find_region_containing_open, but returns the first folded region before the given
		/// position if no such region is found. If the given position is before the first folded region,
		/// returns (\ref end(), 0, 0).
		fold_region_info find_region_containing_or_first_before_open(std::size_t cp) const {
			_find_region_closed finder; // doesn't really matter whether it's open or closed
			auto it = _t.find(finder, cp);
			if (it == _t.end() || cp <= it->gap) {
				if (it == _t.begin()) {
					return fold_region_info(_t.end(), 0, 0);
				}
				--it;
				finder.total_chars -= it->gap + it->range;
				finder.total_lines -= it->gap_lines + it->folded_lines;
			}
			return fold_region_info(it, finder.total_chars, finder.total_lines);
		}
		/// Similar to \ref find_region_containing_closed, but returns the first folded region after the given
		/// position if no such region is found.
		fold_region_info find_region_containing_or_first_after_closed(std::size_t cp) const {
			_find_region_closed finder;
			auto it = _t.find(finder, cp);
			return fold_region_info(it, finder.total_chars, finder.total_lines);
		}
		/// Similar to \ref find_region_containing_closed, but returns the first folded region before the given
		/// position if no such region is found. If the given position is before the first folded region,
		/// returns (\ref end(), 0, 0).
		fold_region_info find_region_containing_or_first_before_closed(std::size_t cp) const {
			_find_region_closed finder;
			auto it = _t.find(finder, cp);
			if (it == _t.end() || cp < it->gap) {
				if (it == _t.begin()) {
					return fold_region_info(_t.end(), 0, 0);
				}
				--it;
				finder.total_chars -= it->gap + it->range;
				finder.total_lines -= it->gap_lines + it->folded_lines;
			}
			return fold_region_info(it, finder.total_chars, finder.total_lines);
		}

		/// Adds a folded region to the registry. All overlapping regions will be removed.
		///
		/// \return An iterator to the newly added region.
		/// \todo Save the removed overlapping regions so that they'll be recovered when the added region's unfolded.
		iterator add_fold_region(const fold_region_data &fr) {
			_bytepos_valid = false;
			fold_region_info
				beg = find_region_containing_or_first_after_open(fr.begin),
				end = find_region_containing_or_first_before_open(fr.end);
			iterator &endit = end.entry;
			if (endit != _t.end()) {
				end.prev_chars += endit->gap + endit->range;
				end.prev_lines += endit->gap_lines + endit->folded_lines;
				++endit;
			} else {
				endit = _t.begin();
			}
			if (endit != _t.end()) {
				auto mod = _t.get_modifier_for(endit.get_node());
				// can overflow, but as expected
				mod->gap += end.prev_chars - fr.end;
				mod->gap_lines += end.prev_lines - fr.end_line;
			}
			_t.erase(beg.entry, endit); // if you're gonna save the regions, save 'em here
			return _t.emplace_before(
				endit,
				fr.begin - beg.prev_chars, fr.end - fr.begin,
				fr.begin_line - beg.prev_lines, fr.end_line - fr.begin_line
			);
		}
		/// Removes the designated folded region.
		void remove_folded_region(iterator it) {
			assert_true_logical(it != _t.end(), "invalid iterator");
			std::size_t dp = it->gap + it->range, dl = it->gap_lines + it->folded_lines;
			iterator next = _t.erase(it);
			if (next != _t.end()) {
				auto mod = _t.get_modifier_for(next.get_node());
				mod->gap += dp;
				mod->gap_lines += dl;
			}
			// _bytepos_valid stays true
		}
		/// Removes all folded regions from the registry.
		void clear_folded_regions() {
			_t.clear();
			_bytepos_valid = true;
		}

		/// Prepares this registry for editing by calling \ref _calculate_byte_positions() to calculate byte
		/// positions for each folded region.
		void prepare_for_edit(const interpretation &interp) {
			_calculate_byte_positions(interp);
		}
		/// Adjusts the positions of the folded regions after an edit so that their positions are as close to
		/// those before the modification as possible.
		void fixup_after_edit(buffer::end_edit_info &edt, const interpretation &interp) {
			if (_t.empty()) {
				return;
			}
			interpretation::character_position_converter cvt(interp);
			buffer::position_patcher patcher(edt.positions);
			std::size_t lastpos = 0;
			for (auto it = _t.begin(); it != _t.end(); ) {
				std::size_t
					begbyte = patcher.patch_next<buffer::position_patcher::strategy::back>(it->bytepos_first),
					endbyte = patcher.patch_next<buffer::position_patcher::strategy::front>(it->bytepos_second);
				if (begbyte < endbyte) {
					std::size_t begchar = cvt.byte_to_character(begbyte), endchar = cvt.byte_to_character(endbyte);
					if (begchar < endchar) {
						it.get_value_rawmod().gap = begchar - lastpos;
						it.get_value_rawmod().range = endchar - begchar;
						lastpos = endchar;
						++it;
						continue;
					}
				}
				it = _t.erase_custom_synth(it, no_synthesizer());
			}
			_t.refresh_tree_synthesized_result();
			_bytepos_valid = false;
		}

		/// Returns an iterator to the beginning of the registry.
		iterator begin() const {
			return _t.begin();
		}
		/// Returns an iterator past the end of the registry.
		iterator end() const {
			return _t.end();
		}

		/// Returns the total number of linebreaks that have been folded.
		std::size_t folded_linebreaks() const {
			return _t.root() ? _t.root()->synth_data.total_folded_lines : 0;
		}
		/// Returns the total number of folded regions.
		std::size_t folded_region_count() const {
			return _t.root() ? _t.root()->synth_data.tree_size : 0;
		}
		/// Returns the underlying \ref tree_type "tree".
		const tree_type &get_raw() const {
			return _t;
		}
	protected:
		/// Struct used to convert unfolded positions or line indices to folded ones.
		///
		/// \tparam Prop The property to convert.
		/// \tparam SynProp Additional property that will also be synthesized along the process.
		template <typename Prop, typename SynProp> struct _unfolded_to_folded {
			/// The underlying \ref sum_synthesizer::index_finder.
			using finder = sum_synthesizer::index_finder<Prop>;
			/// Interface for \ref red_black_tree::tree::find_custom().
			int select_find(const node_type &n, std::size_t &v) {
				return finder::template select_find<SynProp>(n, v, total_unfolded);
			}
			std::size_t total_unfolded = 0; ///< Stores the value of the additional synthesized property.
		};
		/// Used to convert unfolded positions to folded ones.
		using _unfolded_to_folded_pos = _unfolded_to_folded<
			fold_region_synth_data::span_property, fold_region_synth_data::unfolded_length_property
		>;
		/// Used to convert unfolded line indices to folded ones.
		using _unfolded_to_folded_line = _unfolded_to_folded<
			fold_region_synth_data::line_span_property, fold_region_synth_data::unfolded_lines_property
		>;

		/// Struct used to convert folded positions or line indices to unfolded ones.
		///
		/// \tparam Prop The property to convert.
		/// \tparam SynProp Additional property that will also be synthesized along the process.
		template <typename Prop, typename SynProp> struct _folded_to_unfolded {
			/// The underlying \ref sum_synthesizer::index_finder.
			using finder = sum_synthesizer::index_finder<Prop, false, std::less_equal<>>;
			/// Interface for \ref red_black_tree::tree::find_custom().
			int select_find(const node_type &n, std::size_t &v) {
				return finder::template select_find<SynProp>(n, v, total);
			}
			std::size_t total = 0; ///< Stores the value of the additional synthesized property.
		};
		/// Used to convert folded positions to unfolded ones.
		using _folded_to_unfolded_pos = _folded_to_unfolded<
			fold_region_synth_data::unfolded_length_property, fold_region_synth_data::span_property
		>;
		/// Used to convert folded line indices to unfolded ones.
		using _folded_to_unfolded_line = _folded_to_unfolded<
			fold_region_synth_data::unfolded_lines_property, fold_region_synth_data::line_span_property
		>;

		/// Struct used to find folded regions by their positions.
		template <typename Cmp> struct _find_region {
			/// The underlying \ref sum_synthesizer::index_finder.
			using finder = sum_synthesizer::index_finder<
				fold_region_synth_data::span_property, false, Cmp
			>;
			int select_find(const node_type &n, std::size_t &v) {
				/// Interface for \ref red_black_tree::tree::find_custom().
				return finder::template select_find<
					fold_region_synth_data::span_property, fold_region_synth_data::line_span_property
				>(n, v, total_chars, total_lines);
			}
			std::size_t
				total_chars = 0, ///< Stores the total number of characters before the resulting node.
				/// Stores the total number of linebreaks (both soft and hard ones) before the resulting node.
				total_lines = 0;
		};
		/// Used to find folded regions by their positions. If the given position is at the end of a fold region,
		/// the fold region will be the result.
		using _find_region_closed = _find_region<std::less_equal<>>;
		/// Used to find folded regions by their positions. If the given position is at the end of a fold region,
		/// the next fold region will be the result.
		using _find_region_open = _find_region<std::less<>>;

		/// Calculates \ref fold_region_node_data::bytepos_first and \ref fold_region_node_data::bytepos_second if
		/// necessary.
		void _calculate_byte_positions(const interpretation &interp) {
			if (_bytepos_valid) {
				return;
			}
			interpretation::character_position_converter cvt(interp);
			std::size_t pos = 0;
			for (auto it = _t.begin(); it != _t.end(); ++it) {
				pos += it->gap;
				it.get_value_rawmod().bytepos_first = cvt.character_to_byte(pos);
				pos += it->range;
				it.get_value_rawmod().bytepos_second = cvt.character_to_byte(pos);
			}
			_bytepos_valid = true;
		}

		tree_type _t; ///< The underlying binary tree.
		bool _bytepos_valid = false; ///< Whether the underlying byte positions are valid.
	};

	/// Controls the formatting of a single view of a document. This is different for each view of the same
	/// opened document.
	class view_formatting {
	public:
		using fold_region = std::pair<std::size_t, std::size_t>; ///< Contains information about a folded region.

		/// Default constructor.
		view_formatting() = default;
		/// Initializes \ref _lbr with the given \ref linebreak_registry.
		explicit view_formatting(const linebreak_registry &reg) : _lbr(reg) {
		}
		/// Initializes this class with the corresponding \ref interpretation.
		explicit view_formatting(const interpretation &interp) : view_formatting(interp.get_linebreaks()) {
		}

		/// Sets the soft linebreaks of this view.
		void set_softbreaks(const std::vector<std::size_t> &breaks) {
			_lbr.set_softbreaks(breaks);
			recalc_foldreg_lines();
		}
		/// Clears all soft linebreaks of this view.
		void clear_softbreaks() {
			_lbr.clear_softbreaks();
			recalc_foldreg_lines();
		}

		/// Folds the given region.
		///
		/// \return An iterator to the added entry in \ref _fr.
		folding_registry::iterator add_folded_region(const fold_region &rgn) {
			assert_true_usage(rgn.second > rgn.first, "invalid fold region");
			return _fr.add_fold_region(folding_registry::fold_region_data(
				rgn.first, rgn.second,
				_lbr.get_visual_line_of_char(rgn.first), _lbr.get_visual_line_of_char(rgn.second)
			));
		}
		/// Unfolds the given region.
		void remove_folded_region(folding_registry::iterator it) {
			_fr.remove_folded_region(it);
		}
		/// Unfolds all of the document.
		void clear_folded_regions() {
			_fr.clear_folded_regions();
		}

		/// Prepares this \ref view_formatting for modifications by calling \ref folding_registry::prepare_for_edit.
		void prepare_for_edit(const interpretation &interp) {
			_fr.prepare_for_edit(interp);
		}
		/// Fixup this \ref view_formatting after the underlying \ref buffer has been modified by calling
		/// \ref folding_registry::fixup_after_edit().
		void fixup_after_edit(buffer::end_edit_info &info, const interpretation &interp) {
			_fr.fixup_after_edit(info, interp);
		}
		/// Recalculates all line information of \ref _fr.
		///
		/// \todo Remove this when \ref folding_registry has been improved.
		void recalc_foldreg_lines() {
			std::size_t plines = 0, totc = 0;
			for (auto i = _fr._t.begin(); i != _fr._t.end(); ++i) {
				totc += i->gap;
				std::size_t bl = _lbr.get_visual_line_of_char(totc); /*_lbr.get_hard_linebreaks().get_line_and_column_of_char(totc).line;*/
				i.get_value_rawmod().gap_lines = bl - plines;
				totc += i->range;
				std::size_t el = _lbr.get_visual_line_of_char(totc); /*_lbr.get_hard_linebreaks().get_line_and_column_of_char(totc).line;*/
				i.get_value_rawmod().folded_lines = el - bl;
				plines = el;
			}
			_fr._t.refresh_tree_synthesized_result();
		}

		/// Sets the maximum width of a `tab' character in blank spaces.
		void set_tab_width(double v) {
			_tab = v;
		}

		/// Returns the \ref soft_linebreak_registry \ref _lbr.
		const soft_linebreak_registry &get_linebreaks() const {
			return _lbr;
		}
		/// Returns the \ref folding_registry \ref _fr.
		const folding_registry &get_folding() const {
			return _fr;
		}
		/// Returns the maximum width of a `tab' character, relative to the width of a blank space.
		double get_tab_width() const {
			return _tab;
		}
	protected:
		soft_linebreak_registry _lbr; ///< Registry of all soft linebreaks.
		folding_registry _fr; ///< Registry of all folded regions.
		double _tab = 4.0f; ///< The maximum width of a `tab' character relative to the width of a blank space.
	};
}
