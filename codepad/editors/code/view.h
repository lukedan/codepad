#pragma once

/// \file
/// Contains classes used to format a view of a \ref codepad::editor::code::document.

#include "document.h"

namespace codepad::editor::code {
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
			explicit node_data(size_t l) : length(l) {
			}

			size_t length = 0; ///< The number of characters between the two soft linebreaks.
		};
		/// Stores additional synthesized data of a subtree.
		///
		/// \sa node_data
		struct node_synth_data {
			/// The type of a node.
			using node_type = binary_tree_node<node_data, node_synth_data>;

			size_t
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
		using tree_type = binary_tree<node_data, node_synth_data>;
		/// The type of a node in the tree.
		using node_type = typename tree_type::node;
		/// Const iterators to elements in the tree.
		using iterator = typename tree_type::const_iterator;

		/// Used to wrap up the results of a query.
		struct softbreak_info {
			/// Default constructor.
			softbreak_info() = default;
			/// Initializes all fields of the struct.
			softbreak_info(const iterator &it, size_t pc, size_t pl) :
				entry(it), prev_chars(pc), prev_softbreaks(pl) {
			}
			iterator entry; ///< Iterator to the corresponding soft `segment'.
			size_t
				prev_chars = 0, ///< The number of characters before the beginning of \ref entry.
				prev_softbreaks = 0; ///< The number of soft linebreaks before the one \ref entry points to.
		};

		/// Obtains information about a visual line.
		///
		/// \sa _find_line_ending
		std::pair<linebreak_registry::linebreak_info, softbreak_info> get_line_info(size_t line) const {
			if (_t.empty()) {
				return {_reg->get_line_info(line), softbreak_info(_t.end(), 0, 0)};
			}
			size_t softc = 0, hardc = 0;
			iterator soft;
			linebreak_registry::iterator hard;
			size_t slb = _find_line_ending(soft, hard, softc, hardc, line);
			return {
				linebreak_registry::linebreak_info(hard, hardc),
				softbreak_info(soft, softc, slb)
			};
		}
		/// Obtains the position of the given line's beginning and the type of the linebreak before the line.
		std::pair<size_t, linebreak_type> get_beginning_char_of_visual_line(size_t line) const {
			if (_t.empty()) {
				return {_reg->get_line_info(line).first_char, linebreak_type::hard};
			}
			size_t softc = 0, hardc = 0;
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
		std::pair<size_t, linebreak_type> get_past_ending_char_of_visual_line(size_t line) const {
			if (_t.empty()) {
				linebreak_registry::linebreak_info info = _reg->get_line_info(line);
				if (info.entry != _reg->end()) {
					info.first_char += info.entry->nonbreak_chars;
				}
				return {info.first_char, linebreak_type::hard};
			}
			size_t softc = 0, hardc = 0;
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
		softbreak_info get_softbreak_before_or_at_char(size_t c) const {
			_get_softbreaks_before selector;
			size_t nc = c;
			auto it = _t.find_custom(selector, nc);
			return softbreak_info(it, c - nc, selector.num_softbreaks);
		}
		/// Returns the index of the visual line that the given character is on.
		size_t get_visual_line_of_char(size_t c) const {
			return
				_reg->get_line_and_column_of_char(c).line +
				get_softbreak_before_or_at_char(c).prev_softbreaks;
		}
		/// Returns the visual line and column of a given character.
		std::pair<size_t, size_t> get_visual_line_and_column_of_char(size_t c) const {
			auto hard = _reg->get_line_and_column_of_char(c);
			_get_softbreaks_before selector;
			_t.find_custom(selector, c);
			return {hard.line + selector.num_softbreaks, std::min(c, hard.column)};
		}
		/// Returns the combined result of \ref get_visual_line_and_column_of_char and
		/// \ref get_softbreak_before_or_at_char.
		std::tuple<size_t, size_t, softbreak_info>
			get_visual_line_and_column_and_softbreak_before_or_at_char(size_t c) const {
			size_t nc = c;
			auto hard = _reg->get_line_and_column_of_char(c);
			_get_softbreaks_before selector;
			auto it = _t.find_custom(selector, nc);
			return {
				hard.line + selector.num_softbreaks,
				std::min(nc, hard.column),
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
		void set_softbreaks(const std::vector<size_t> &poss) {
			std::vector<node_data> vs;
			vs.reserve(poss.size());
			size_t last = 0; // no reason to insert softbreak at position 0
			for (size_t cp : poss) {
				assert_true_usage(cp > last, "softbreak list not properly sorted");
				vs.emplace_back(cp - last);
				last = cp;
			}
			_t = tree_type(vs.begin(), vs.end());
		}

		/// Returns the total number of soft linebreaks.
		size_t num_softbreaks() const {
			return _t.root() ? _t.root()->synth_data.total_softbreaks : 0;
		}
		/// Returns the total number of visual lines.
		size_t num_visual_lines() const {
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
			/// Interface to binary_tree::find_custom.
			int select_find(node_type &n, size_t &target) {
				return finder::template select_find<
					node_synth_data::softbreaks_property
				>(n, target, num_softbreaks);
			}
			size_t num_softbreaks = 0; ///< Records the number of soft linebreaks before the resulting node.
		};

		tree_type _t; ///< The underlying \ref binary_tree that records all soft linebreaks.
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
		size_t _find_line_ending(
			iterator &softit, linebreak_registry::iterator &hardit,
			size_t &softc, size_t &hardc, size_t line
		) const {
			assert_true_logical(softc == 0 && hardc == 0, "irresponsible caller");
			if (line > _t.root()->synth_data.total_softbreaks + _reg->num_linebreaks()) { // past the end
				softit = _t.end();
				softc = _t.root()->synth_data.total_length;
				hardit = _reg->end();
				hardc = _reg->num_chars();
				return _t.root()->synth_data.total_softbreaks;
			}
			size_t
				cursc = 0, // number of characters before the node, excluding the left subtree
				softl = 0, // number of soft linebreaks before the node, excluding the left subtree
				softlres = 0; // number of soft linebreaks before the node

			for (node_type *n = _t.root(); n; ) { // descend
				size_t
					sccs = cursc, // like cursc but does not exclude the left subtree
					scls = softl; // like softl but does not exclude the left subtree
				if (n->left) { // add left subtree
					sccs += n->left->synth_data.total_length;
					scls += n->left->synth_data.total_softbreaks;
				}
				if (scls > line) { // too many soft linebreaks
					n = n->left;
				} else {
					auto hres = _reg->get_line_info(line - scls); // query hard linebreaks
					// basically, cannot have a linebreak of the same type as the first linebreak
					// between the current two, or it's not the result
					if (!(
						(hardc > hres.first_char && hardc < sccs) ||
						(softc > sccs && softc < hres.first_char)
						)) { // update the result
						hardit = hres.entry;
						hardc = hres.first_char;
						softit = _t.get_const_iterator_for(n);
						softc = sccs;
						softlres = scls;
					}
					if (hres.first_char < sccs) { // the hard linebreak is in front, goto left subtree
						n = n->left;
					} else { // goto right subtree
						cursc = sccs + n->value.length;
						softl = scls + 1;
						n = n->right;
					}
				}
			}
			if (softc + softit->length < hardc) { // after the last soft linebreak
				--hardit;
				hardc -= hardit->nonbreak_chars + (hardit->ending == line_ending::none ? 0 : 1);
				softit = _t.end();
				softc = _t.root()->synth_data.total_length;
				softlres = _t.root()->synth_data.total_softbreaks;
			}
			return softlres;
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
			fold_region_data(size_t b, size_t e, size_t bl, size_t el) :
				begin(b), end(e), begin_line(bl), end_line(el) {
			}

			size_t
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
			fold_region_node_data(size_t g, size_t r, size_t gl, size_t rl) :
				gap(g), range(r), gap_lines(gl), folded_lines(rl) {
			}

			size_t
				/// The gap between the ending of the last folded region (or the beginning of the document
				/// if this is the first region) and the beginning of this folded region.
				gap = 0,
				range = 0, ///< The length of this folded region, i.e., the number of characters folded.
				/// The number of linebreaks (including both hard and soft ones) that \ref gap covers.
				gap_lines = 0,
				/// The number of linebreaks (including both hard and soft ones) that \ref range covers.
				folded_lines = 0;
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
				inline static size_t get(const node_type &n) {
					return n.value.gap + n.value.range;
				}
			};
			/// Used to obtain the total number of linebreaks (both hard and soft ones) that a node covers.
			struct get_node_line_span {
				/// Returns the sum of \ref fold_region_node_data::gap_lines and
				/// \ref fold_region_node_data::folded_lines.
				inline static size_t get(const node_type &n) {
					return n.value.gap_lines + n.value.folded_lines;
				}
			};

			size_t
				total_length = 0, ///< The total number of characters in this subtree.
				total_folded_chars = 0, ///< The total number of characters that are folded in this subtree.
				total_lines = 0, ///< The total number of linebreaks (both soft and hard ones) in this subtree.
				/// The total number of folded linebreaks (both soft and hard ones) in this subtree.
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
				size_t fold_region_node_data::*Node,
				size_t fold_region_synth_data::*TreeTotal, size_t fold_region_synth_data::*TreeFolded
			> struct unfolded_property_base {
				/// Returns the value of \p Node.
				inline static size_t get_node_value(const node_type &n) {
					return n.value.*Node;
				}
				/// The same as \ref get_node_value.
				inline static size_t get_node_synth_value(const node_type &n) {
					return get_node_value(n);
				}
				/// Returns the difference between \p TreeFolded and \p TreeTotal.
				inline static size_t get_tree_synth_value(const node_type &n) {
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
		using tree_type = binary_tree<fold_region_node_data, fold_region_synth_data>;
		using node_type = tree_type::node; ///< Nodes of \ref tree_type.
		using iterator = tree_type::const_iterator; ///< Const iterators through the registry.

		/// Given a line index in the document with folding enabled, returns the index of the same line when folding
		/// is disabled.
		size_t folded_to_unfolded_line_number(size_t line) const {
			_folded_to_unfolded_line finder;
			_t.find_custom(finder, line);
			return finder.total + line;
		}
		/// Given a line index in the document with folding disabled, returns the index of the same line when folding
		/// is enabled.
		size_t unfolded_to_folded_line_number(size_t line) const {
			_unfolded_to_folded_line finder;
			auto it = _t.find_custom(finder, line);
			if (it != _t.end()) {
				line = std::min(line, it->gap_lines);
			}
			return finder.total_unfolded + line;
		}
		/// Given a caret position in the document with folding enabled (i.e., as if all folded regions have been
		/// removed), returns the corresponding position in the document with folding disabled.
		size_t folded_to_unfolded_caret_pos(size_t pos) const {
			_folded_to_unfolded_pos finder;
			_t.find_custom(finder, pos);
			return finder.total + pos;
		}
		/// Given a caret position in the document with folding disabled, returns the corresponding position in the
		/// document with folding enabled (i.e., as if all folded regions have been removed).
		size_t unfolded_to_folded_caret_pos(size_t pos) const {
			_unfolded_to_folded_pos finder;
			auto it = _t.find_custom(finder, pos);
			if (it != _t.end()) {
				pos = std::min(pos, it->gap);
			}
			return finder.total_unfolded + pos;
		}

		/// Given a line index in the document with folding disabled, returns the index (with folding disabled) of
		/// the first of the set of lines that, with folding enabled, belongs to the line at the given index.
		size_t get_beginning_line_of_folded_lines(size_t line) const {
			return folded_to_unfolded_line_number(unfolded_to_folded_line_number(line));
		}
		/// Given a line index in the document with folding disabled, returns the index (with folding disabled) past
		/// the last of the set of lines that, with folding enabled, belongs to the line at the given index.
		size_t get_past_ending_line_of_folded_lines(size_t line) const {
			return folded_to_unfolded_line_number(unfolded_to_folded_line_number(line) + 1);
		}

		/// Stores information about a fold region. Used to pack query results.
		struct fold_region_info {
			/// Default constructor.
			fold_region_info() = default;
			/// Initializes all fields of this struct.
			fold_region_info(const iterator &it, size_t pc, size_t pl) :
				entry(it), prev_chars(pc), prev_lines(pl) {
			}

			iterator entry; ///< Iterator to the resulting fold region.
			size_t
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
		fold_region_info find_region_containing_open(size_t cp) const {
			_find_region_open finder;
			auto it = _t.find_custom(finder, cp);
			if (it != _t.end() && cp > it->gap) {
				return fold_region_info(it, finder.total_chars, finder.total_lines);
			}
			return fold_region_info(_t.end(), 0, 0);
		}
		/// Finds the folded region that encapsules the given position. The position may lie on the boundary of the
		/// result. If two such regions exist (happens when the position is between two adjancent folded regions),
		/// the one in front is returned. If no such region is found, returns (\ref end(), 0, 0).
		fold_region_info find_region_containing_closed(size_t cp) const {
			_find_region_closed finder;
			auto it = _t.find_custom(finder, cp);
			if (it != _t.end() && cp >= it->gap) {
				return fold_region_info(it, finder.total_chars, finder.total_lines);
			}
			return fold_region_info(_t.end(), 0, 0);
		}
		/// Similar to \ref find_region_containing_open, but returns the first folded region after the given
		/// position if no such region is found.
		fold_region_info find_region_containing_or_first_after_open(size_t cp) const {
			_find_region_open finder;
			auto it = _t.find_custom(finder, cp);
			return fold_region_info(it, finder.total_chars, finder.total_lines);
		}
		/// Similar to \ref find_region_containing_open, but returns the first folded region before the given
		/// position if no such region is found. If the given position is before the first folded region,
		/// returns (\ref end(), 0, 0).
		fold_region_info find_region_containing_or_first_before_open(size_t cp) const {
			_find_region_closed finder; // doesn't really matter whether it's open or closed
			auto it = _t.find_custom(finder, cp);
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
		fold_region_info find_region_containing_or_first_after_closed(size_t cp) const {
			_find_region_closed finder;
			auto it = _t.find_custom(finder, cp);
			return fold_region_info(it, finder.total_chars, finder.total_lines);
		}
		/// Similar to \ref find_region_containing_closed, but returns the first folded region before the given
		/// position if no such region is found. If the given position is before the first folded region,
		/// returns (\ref end(), 0, 0).
		fold_region_info find_region_containing_or_first_before_closed(size_t cp) const {
			_find_region_closed finder;
			auto it = _t.find_custom(finder, cp);
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
			size_t dp = it->gap + it->range, dl = it->gap_lines + it->folded_lines;
			iterator next = _t.erase(it);
			if (next != _t.end()) {
				auto mod = _t.get_modifier_for(next.get_node());
				mod->gap += dp;
				mod->gap_lines += dl;
			}
		}
		/// Removes all folded regions from the registry.
		void clear_folded_regions() {
			_t.clear();
		}

		/// Adjusts the positions of the folded regions after a modification so that their positions are as close to
		/// those before the modification as possible.
		///
		/// \todo Currently only adjusts the caracter positions, and is therefore not used.
		void fixup_positions(const caret_fixup_info &edt) {
			if (_t.empty()) {
				return;
			}
			for (const modification_position &mod : edt.mods) {
				fold_region_info pfirst = find_region_containing_or_first_after_open(mod.position);
				if (pfirst.entry == _t.end()) {
					break;
				}
				if (mod.removed_range > 0) {
					size_t endp = mod.position + mod.removed_range;
					fold_region_info plast = find_region_containing_or_first_before_open(endp);
					if (plast.entry != _t.end()) {
						size_t
							ffbeg = pfirst.prev_chars + pfirst.entry->gap,
							ffend = ffbeg + pfirst.entry->range;
						if (pfirst.entry == plast.entry && endp < ffend) {
							if (mod.position >= ffbeg) {
								_t.get_modifier_for(pfirst.entry.get_node())->range -= mod.removed_range;
							} else {
								auto modder = _t.get_modifier_for(pfirst.entry.get_node());
								modder->range = ffend - endp;
								modder->gap = mod.position - pfirst.prev_chars;
							}
						} else {
							size_t
								lfbeg = plast.prev_chars + plast.entry->gap,
								lfend = lfbeg + plast.entry->range,
								addlen = 0;
							if (endp < lfend) {
								_t.get_modifier_for(plast.entry.get_node())->range -= endp - lfbeg;
							} else {
								plast.move_next();
								addlen -= endp - plast.prev_chars;
							}
							// the stats in plast becomes invalid since here
							if (mod.position > ffbeg) {
								_t.get_modifier_for(pfirst.entry.get_node())->range -= ffend - mod.position;
								pfirst.move_next();
							} else {
								addlen += mod.position - pfirst.prev_chars;
							}
							_t.erase(pfirst.entry, plast.entry);
							if (plast.entry != _t.end()) {
								_t.get_modifier_for(plast.entry.get_node())->gap += addlen;
							}
						}
					} else {
						_t.begin().get_modifier()->gap -= mod.removed_range;
					}
				}
				if (mod.added_range > 0) {
					if (mod.position <= pfirst.prev_chars + pfirst.entry->gap) {
						_t.get_modifier_for(pfirst.entry.get_node())->gap += mod.added_range;
					} else {
						_t.get_modifier_for(pfirst.entry.get_node())->range += mod.added_range;
					}
				}
			}
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
		size_t folded_linebreaks() const {
			return _t.root() ? _t.root()->synth_data.total_folded_lines : 0;
		}
		/// Returns the total number of folded regions.
		size_t folded_region_count() const {
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
			/// Interface for \ref binary_tree::find_custom.
			int select_find(const node_type &n, size_t &v) {
				return finder::template select_find<SynProp>(n, v, total_unfolded);
			}
			size_t total_unfolded = 0; ///< Stores the value of the additional synthesized property.
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
			/// Interface for \ref binary_tree::find_custom.
			int select_find(const node_type &n, size_t &v) {
				return finder::template select_find<SynProp>(n, v, total);
			}
			size_t total = 0; ///< Stores the value of the additional synthesized property.
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
			int select_find(const node_type &n, size_t &v) {
				/// Interface for \ref binary_tree::find_custom.
				return finder::template select_find<
					fold_region_synth_data::span_property, fold_region_synth_data::line_span_property
				>(n, v, total_chars, total_lines);
			}
			size_t
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

		tree_type _t; ///< The underlying \ref binary_tree.
	};

	/// Controls the formatting of a single view of a document. This is different for each view of the same
	/// opened document.
	class view_formatting {
	public:
		using fold_region = std::pair<size_t, size_t>; ///< Contains information about a folded region.

		/// Default constructor.
		view_formatting() = default;
		/// Initializes \ref _lbr with the given \ref linebreak_registry.
		explicit view_formatting(const linebreak_registry &reg) : _lbr(reg) {
		}

		/// Sets the soft linebreaks of this view.
		void set_softbreaks(const std::vector<size_t> &breaks) {
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

		/// Calls \ref folding_registry::fixup_positions.
		void fixup_folding_positions(const caret_fixup_info &info) {
			_fr.fixup_positions(info);
		}
		/// Recalculates all line information of \ref _fr.
		///
		/// \todo Remove this when \ref folding_registry has been improved.
		void recalc_foldreg_lines() {
			size_t plines = 0, totc = 0;
			for (auto i = _fr._t.begin(); i != _fr._t.end(); ++i) {
				totc += i->gap;
				size_t bl = _lbr.get_hard_linebreaks().get_line_and_column_of_char(totc).line;
				i.get_value_rawmod().gap_lines = bl - plines;
				totc += i->range;
				size_t el = _lbr.get_hard_linebreaks().get_line_and_column_of_char(totc).line;
				i.get_value_rawmod().folded_lines = el - bl;
				plines = el;
			}
			_fr._t.refresh_tree_synthesized_result();
		}

		/// Returns the \ref soft_linebreak_registry \ref _lbr.
		const soft_linebreak_registry &get_linebreaks() const {
			return _lbr;
		}
		/// Returns the \ref folding_registry \ref _fr.
		const folding_registry &get_folding() const {
			return _fr;
		}
	protected:
		soft_linebreak_registry _lbr; ///< Registry of all soft linebreaks.
		folding_registry _fr; ///< Registry of all folded regions.
	};
}
