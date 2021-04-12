// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Structs and classes used to store carets.

#include <map>
#include <algorithm>
#include <compare>

#include <codepad/core/misc.h>
#include <codepad/core/assert.h>
#include <codepad/core/red_black_tree.h>
#include <codepad/ui/misc.h>

namespace codepad::editors {
	/// Template class of containers used to store a set of carets. Derived classes should define two additional
	/// types: \p position, which indicates the position of a caret, and \p selection, which indicates the full
	/// position of a caret and the associated selection, and must contain two \p std::size_t fields: \p caret and
	/// \p selection.
	///
	/// \tparam Data The data associated with a single caret.
	/// \tparam Derived The derived class, if any. Used for CRTP.
	template <typename Data, typename Derived> class caret_set_base {
	public:
		/// A caret's position and associated data of type \p Data.
		struct caret_data {
			/// Default constructor.
			caret_data() = default;
			/// Initializes \ref data and \ref caret.
			caret_data(Data d, ui::caret_selection c) : data(std::move(d)), caret(c) {
			}

			Data data; ///< Additional data associated with this caret.
			/// Caret and selection position. The position of this caret is relative to the end of the previous
			/// selection region.
			ui::caret_selection caret;
			red_black_tree::color color = red_black_tree::color::red; ///< The color of this node.

			/// Returns the sum of \ref offset and \ref length.
			[[nodiscard]] std::size_t get_total_offset() const {
				return caret.selection_begin + caret.selection_length;
			}
		};
		/// Synthesized data of a subtree.
		struct node_data {
			using node = binary_tree_node<caret_data, node_data>; ///< A node in the tree.

			/// The sum of all \ref caret_data::offset and \ref caret_data::length in this subtree.
			std::size_t offset_sum = 0;

			using offset_sum_property = sum_synthesizer::compact_property<
				synthesization_helper::func_value_property<&caret_data::get_total_offset>,
				&node_data::offset_sum
			>; ///< Property for the accumulated offset.

			/// Refreshes the given node's synthesized data for \ref offset_sum_property.
			inline static void synthesize(node &n) {
				sum_synthesizer::synthesize<offset_sum_property>(n);
			}
		};
		/// The container used to store carets.
		using tree_type = red_black_tree::tree<
			caret_data, red_black_tree::member_red_black_access<&caret_data::color>, node_data
		>;
		/// Iterator over the carets.
		using iterator = typename tree_type::const_iterator;

		/// An iterator and its associated accumulated offset.
		struct iterator_position {
			friend caret_set_base;
		public:
			/// Moves on to the next iterator.
			void move_next() {
				_pos += _iter->get_total_offset();
				++_iter;
			}
			/// Moves to the previous iterator.
			void move_prev() {
				--_iter;
				_pos -= _iter->get_total_offset();
			}

			/// Returns \ref _iter.
			[[nodiscard]] const iterator &get_iterator() const {
				return _iter;
			}
			/// Returns the total amount of offset before this caret.
			[[nodiscard]] std::size_t get_total_offset() const {
				return _pos;
			}

			/// Returns the absolute beginning position of the current caret. The caller must make sure that the
			/// iterator is not at the end.
			[[nodiscard]] std::size_t get_selection_begin() const {
				return _pos + _iter->caret.selection_begin;
			}
			/// Returns the \ref caret_selection where the position is global instead of relative to the previous
			/// caret. The caller must make sure that the iterator is not at the end.
			[[nodiscard]] ui::caret_selection get_caret_selection() const {
				ui::caret_selection result = _iter->caret;
				result.selection_begin += _pos;
				return result;
			}
		protected:
			iterator _iter; ///< The iterator.
			std::size_t _pos = 0; ///< The end position of the previous caret, or 0 if this is the first caret.
		};
		/// Result of a range query.
		struct range_query_result {
			iterator_position first; ///< Iterator to the first caret that ends at or after the range.
			/// Iterator to the last caret that ends after the range. Note that this is different from the convention
			/// where the `end' iterator points to one element past the end.
			iterator_position last;
		};

		/// Default virtual destructor.
		virtual ~caret_set_base() = default;

		/// Calls \ref _add_impl() to add a caret to the set of carets. The caret may be merged with existing carets
		/// with overlapping selection ranges.
		///
		/// \return An iterator to the added entry, and a boolean indicating whether the inserted caret has been
		///         merged with any other caret.
		std::pair<iterator_position, bool> add(ui::caret_selection caret, Data data) {
			return Derived::_add_impl(static_cast<Derived&>(*this), caret, std::move(data));
		}
		/// Removes the given caret.
		void remove(iterator it) {
			std::size_t total_offset = it->get_total_offset();
			it = carets.erase(it);
			if (it != carets.end()) {
				carets.get_modifier_for(it.get_node())->caret.selection_begin += total_offset;
			}
		}

		/// Resets the contents of this caret set, leaving only one caret at the beginning of the buffer.
		void reset() {
			Derived::_reset_impl(static_cast<Derived&>(*this));
		}

		/// Returns an \ref iterator_position pointing to the first caret.
		[[nodiscard]] iterator_position begin() const {
			iterator_position result;
			result._iter = carets.begin();
			return result;
		}
		/// Finds the first caret that ends at or after the specified position.
		[[nodiscard]] iterator_position find_first_ending_at_or_after(std::size_t pos) const {
			iterator_position result;
			std::size_t offset = pos;
			result._iter = carets.find(_finder(), offset);
			result._pos = pos - offset;
			return result;
		}
		/// Finds the first caret that ends strictly after the specified position.
		[[nodiscard]] iterator_position find_first_ending_after(std::size_t pos) const {
			iterator_position result;
			std::size_t offset = pos;
			result._iter = carets.find(_finder_exclusive(), offset);
			result._pos = pos - offset;
			return result;
		}

		/// Finds all carets that intersects with the given range.
		[[nodiscard]] range_query_result find_intersecting_ranges(std::size_t beg, std::size_t end) const {
			range_query_result result;
			result.first = find_first_ending_at_or_after(beg);
			result.last = find_first_ending_after(end);
			return result;
		}


		/// Adds a caret to the given container, merging it with existing ones when necessary. Any caret whose
		/// selection intersects the selection of this caret will be merged into this caret. If two carets touch
		/// each other, then they're *not* merged only if they both have selections. Merging doesn't change the
		/// position of the caret.
		///
		/// \param cont The container.
		/// \param et The caret to be added to the container.
		/// \return An iterator of the container to the inserted caret, and a boolean indicating whether the inserted
		///         caret was merged with other carets.
		inline static std::pair<iterator_position, bool> add_caret_to(
			caret_set_base &set, ui::caret_selection caret, Data data
		) {
			auto range = set.find_intersecting_ranges(caret.selection_begin, caret.get_selection_end());
			std::size_t selection_end = caret.get_selection_end();

			// find merged carets & compute actual inserted caret
			std::size_t new_beg = caret.selection_begin, new_end = selection_end;
			if (caret.has_selection()) {
				// if two selections are touching, don't merge
				if (range.first.get_iterator() != set.carets.end()) {
					if (
						range.first.get_iterator()->caret.has_selection() &&
						range.first.get_caret_selection().get_selection_end() == caret.selection_begin
					) {
						range.first.move_next();
					}
				}
			}
			if (range.first.get_iterator() != set.carets.end()) {
				new_beg = std::min(new_beg, range.first.get_selection_begin());
			}
			if (range.last.get_iterator() != set.carets.end()) {
				auto caret_sel = range.last.get_caret_selection();
				if (caret_sel.selection_begin < selection_end || (
					caret_sel.selection_begin == selection_end &&
					!(caret.has_selection() && caret_sel.has_selection())
				)) {
					new_end = std::max(new_end, caret_sel.get_selection_end());
					range.last.move_next();
				}
			}

			bool has_merged = range.first.get_iterator() != range.last.get_iterator();
			std::size_t erased_offset = 0;
			if (has_merged) {
				// compute new merged caret_selection
				caret.caret_offset = caret.selection_begin + caret.caret_offset - new_beg;
				caret.selection_length = new_end - new_beg;
				caret.selection_begin = new_beg;

				// erase any merged carets
				erased_offset = range.last.get_total_offset() - range.first.get_total_offset();
				set.carets.erase(range.first.get_iterator(), range.last.get_iterator());
			}

			// update the affected offset
			if (range.last.get_iterator() != set.carets.end()) {
				set.carets.get_modifier_for(range.last.get_iterator().get_node())->caret.selection_begin =
					range.last.get_selection_begin() - caret.get_selection_end();
			}

			// insert the new caret
			caret.selection_begin -= range.first.get_total_offset();
			iterator_position result;
			result._iter = set.carets.emplace_before(range.last.get_iterator(), std::move(data), caret);
			result._pos = range.first.get_total_offset();

			return { result, has_merged };
		}

		/// Tests if the given position belongs to a selected region. Carets that have no selected regions
		/// are ignored.
		///
		/// \tparam FrontCmp Used to determine whether being at the starting boundary of selected regions counts as
		///                  being inside the region.
		/// \tparam RearCmp Used to determine whether being at the finishing boundary of selected regions counts as
		///                 being inside the region.
		template <
			typename FrontCmp = std::less_equal<>, typename RearCmp = FrontCmp
		> [[nodiscard]] bool is_in_selection(std::size_t cp) const {
			auto cur = find_first_ending_at_or_after(cp);
			FrontCmp front_cmp;
			RearCmp rear_cmp;
			while (cur.get_iterator() != carets.end() && cur.get_caret_selection().selection_begin <= cp) {
				if (cur.get_iterator()->caret.has_selection()) {
					auto range = cur.get_caret_selection().get_range();
					if (front_cmp(range.first, cp) && rear_cmp(cp, range.second)) {
						return true;
					}
				}
				cur.move_next();
			}
			return false;
		}

		tree_type carets; ///< The carets.
	protected:
		/// Used to find carets that end at or after a specific position.
		using _finder = sum_synthesizer::index_finder<
			typename node_data::offset_sum_property, false, std::less_equal<>
		>;
		/// Used to find carets that end strictly after a specific position.
		using _finder_exclusive = sum_synthesizer::index_finder<typename node_data::offset_sum_property>;

		/// Implementation of \ref add(). The derived class can declare a method with the same name to override this.
		[[nodiscard]] inline static std::pair<iterator_position, bool> _add_impl(
			caret_set_base &set, ui::caret_selection caret, Data data
		) {
			return add_caret_to(set, caret, std::move(data));
		}
		/// Implementation of \ref reset(). The derived class can declare a method with the same name to override
		/// this.
		inline static void _reset_impl(caret_set_base &set) {
			set.carets.clear();
			set.carets.emplace_before(set.carets.end()); // insert caret at the front of the document
		}
	};
}
