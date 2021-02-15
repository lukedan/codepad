// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Class used to manage a series of ranges that may overlap one another.

#include <codepad/core/red_black_tree.h>

namespace codepad::editors {
	/// A registry that stores a series of potentially overlapping ranges, that allows for element (character in
	/// most cases) clips to be inserted and erased while .
	template <typename T> class overlapping_range_registry {
	public:
		/// Data for a single range.
		struct range_data {
			/// Default constructor.
			range_data() = default;
			/// Initializes all fields of this struct.
			range_data(T val, std::size_t off, std::size_t len) : value(std::move(val)), offset(off), length(len) {
			}

			T value{}; ///< The value associated with this range.
			std::size_t
				/// The offset of the beginning of this range from the *beginning* of the previous range. This allows
				/// ranges to overlap.
				offset = 0,
				length = 0; ///< The length of this range.
			red_black_tree::color color = red_black_tree::color::red; ///< The color of this node.
		};
		/// Synthesized data of a subtree.
		struct node_data {
			using node = binary_tree_node<range_data, node_data>; ///< A node in the tree.

			std::size_t
				offset_sum = 0, ///< The sum of \ref range_data::offset.
				/// The maximum ending position for the subtree. For a single node, this would be
				/// \ref range_data::offset + \ref range_data::length. For a subtree, this would be the maximum
				/// ending position among all nodes, starting from the position of the range before this subtree.
				maximum_end_position = 0;

			using offset_property = sum_synthesizer::compact_property<
				synthesization_helper::field_value_property<&range_data::offset>,
				&node_data::offset_sum
			>; ///< Property for the accumulated offset.

			/// Refreshes the given node's synthesized data.
			inline static void synthesize(node &n) {
				sum_synthesizer::synthesize<offset_property>(n);

				// update `maximum_end_position`
				std::size_t &max_end = n.synth_data.maximum_end_position;
				max_end = 0;
				std::size_t length = 0;
				if (n.left) {
					max_end = n.left->synth_data.maximum_end_position;
					length = n.left->synth_data.offset_sum;
				}
				length += n.value.offset;
				max_end = std::max(max_end, length + n.value.length);
				if (n.right) {
					max_end = std::max(max_end, length + n.right->synth_data.maximum_end_position);
				}
			}
		};
		/// The binary tree type used to store all range data.
		using tree_type = red_black_tree::tree<
			range_data, red_black_tree::member_red_black_access<&range_data::color>, node_data
		>;
		using iterator = typename tree_type::const_iterator; ///< Iterators. Direct modifications are not allowed.

		/// An iterator and its associated position.
		struct iterator_position {
			friend overlapping_range_registry;
		public:
			/// Default constructor.
			iterator_position() = default;

			/// Moves the iterator to the next position. The caller must make sure that the iterator is not already
			/// at the end.
			void move_next() {
				_pos += _iter->offset;
				++_iter;
			}

			/// Returns \ref _iter.
			[[nodiscard]] const iterator &get_iterator() const {
				return _iter;
			}
			/// Returns the starting position of this range. The caller must make sure that the iterator is not at
			/// the end.
			[[nodiscard]] std::size_t get_range_start() const {
				return _pos + _iter->offset;
			}
		protected:
			iterator _iter; ///< The iterator.
			std::size_t _pos = 0; ///< The starting position of the *previous* range.
		};
		/// The result of a point query.
		struct point_query_result {
			iterator_position
				/// The first range that intersects the given point. Ranges between this element and \ref end may not
				/// actually intersect the point - use \ref find_next_range_ending_after() to iterate through ranges
				/// that are guaranteed to intersect the point.
				begin,
				/// Iterator past the last range that intersects the given point.
				end;
		};
		/// The result of a range query.
		struct range_query_result {
			iterator_position
				/// The first iterator that ends at or after the beginning position of the given range. All elements
				/// between this element and the element \ref begin points to potentially intersect with the range
				/// but needs further testing. Use \ref find_next_range_ending_after() to advance this iterator
				/// through elements that intersect the range until \ref begin.
				before_begin,
				begin, ///< Iterator to the first element that starts within the queried range.
				/// Iterator past the last element that starts within the queried range, which also must be the last
				/// element that intersects with the range.
				end;
		};

		/// Returns an iterator to the first element.
		[[nodiscard]] iterator begin() const {
			return _ranges.begin();
		}
		/// Returns an iterator to the last element.
		[[nodiscard]] iterator end() const {
			return _ranges.end();
		}
		/// Returns an \ref iterator_position corresponding to the first range.
		[[nodiscard]] iterator_position begin_position() const {
			iterator_position result;
			result._iter = begin();
			result._pos = 0;
			return result;
		}

		/// Inserts a range. If there are other ranges starting at the same position, this range will be inserted
		/// before all of them.
		///
		/// \return Iterator to the inserted element.
		iterator_position insert_range(std::size_t begin, std::size_t length, T value) {
			iterator_position before = _find(_position_finder(), begin);
			std::size_t insert_offset = begin - before._pos;
			if (before.get_iterator() != _ranges.end()) {
				_get_modifier_for(before.get_iterator())->offset -= insert_offset;
			}
			before._iter = _ranges.emplace_before(before.get_iterator(), std::move(value), insert_offset, length);
			return before;
		}
		/// Erases the given range.
		void erase(iterator iter) {
			std::size_t offset = iter->offset;
			iter = _ranges.erase(iter);
			if (iter != _ranges.end()) {
				_get_modifier_for(iter)->offset += offset;
			}
		}

		/// Finds the range of elements that may intersect with the given point.
		[[nodiscard]] point_query_result find_intersecting_ranges(std::size_t point) {
			point_query_result result;
			result.begin = _find(_extent_finder(), point);
			result.end = _find(_position_finder_exclusive(), point);
			return result;
		}
		/// Finds the range of elements that may intersect with the given range.
		[[nodiscard]] range_query_result find_intersecting_ranges(std::size_t begin, std::size_t past_end) {
			range_query_result result;
			result.before_begin = _find(_extent_finder(), begin);
			result.begin = _find(_position_finder(), begin);
			result.end = _find(_position_finder_exclusive(), past_end);
			return result;
		}
		/// Given an \ref iterator_position, finds an iterator to the next range that ends at or after the given
		/// position.
		[[nodiscard]] iterator_position find_next_range_ending_after(std::size_t begin, iterator_position iter) {
			assert_true_logical(iter.get_iterator() != _ranges.end(), "iterator already at the end");
			std::size_t &iter_pos = iter._pos;
			typename node_data::node *n = iter._iter.get_node();
			while (
				n->right == nullptr ||
				iter_pos + n->value.offset + n->right->synth_data.maximum_end_position < begin
				) { // ignore the right subtree and go up to the next node
				while (n->parent && n == n->parent->right) {
					if (n->left) {
						iter_pos -= n->left->synth_data.offset_sum;
					}
					iter_pos -= n->parent->value.offset;
					n = n->parent;
				}
				// now either n is the root, or n is a right child
				if (n->parent == nullptr) {
					// n is the root, there's no next range - return the end
					iter._iter = _ranges.end();
					iter._pos = n->synth_data.offset_sum;
					return iter;
				}
				iter_pos += n->value.offset;
				if (n->right) {
					iter_pos += n->right->synth_data.offset_sum;
				}
				n = n->parent;
				// we've moved to the next element - check if it ends after the position
				if (iter_pos + n->value.offset + n->value.length >= begin) {
					// yes - return the iterator
					// the right subtree will be checked next time this function is called
					iter._iter = _ranges.get_iterator_for(n);
					return iter;
				}
			}
			// check the right subtree - it must contain an element that ends at or after the position
			n = n->right;
			iter_pos += n->parent->value.offset;
			// now iter_pos is before the subtree; save it for querying
			if (begin <= iter_pos) {
				// every element in the subtree ends after begin; simply return the leftmost element
				for (; n->left; n = n->left) {
				}
				iter._iter = _ranges.get_iterator_for(n);
				// iter_pos is already at the correct position
				return iter;
			}
			// use a `_range_extent_finder` to find the first iterator
			std::size_t subtree_offset = begin - iter_pos;
			_extent_finder finder;
			// this must yield a non-null node; if the node somehow becomes null we'll just crash here
			while (true) {
				int branch = finder.select_find(*n, subtree_offset);
				if (branch == 0) {
					break;
				}
				n = branch > 0 ? n->right : n->left;
			}
			iter._iter = _ranges.get_iterator_for(n);
			iter_pos = begin - subtree_offset;
			return iter;
		}

		/// Called when a modification has been made - this function erases all ranges that are fully erased,
		/// truncates ranges that are partially erased, and extents ones that span over the erased range.
		void on_modification(std::size_t start, std::size_t erased, std::size_t inserted) {
			std::size_t
				erase_end = start + erased,
				char_diff = inserted - erased;
			range_query_result query = find_intersecting_ranges(start, erase_end);

			// ranges starting before `start`
			while (query.before_begin.get_iterator() != query.begin.get_iterator()) {
				std::size_t end = query.before_begin.get_range_start() + query.before_begin.get_iterator()->length;
				if (end > erase_end) {
					_get_modifier_for(query.before_begin.get_iterator())->length += char_diff;
				} else { // truncated
					_get_modifier_for(query.before_begin.get_iterator())->length -= end - start;
				}
				// update
				query.before_begin = find_next_range_ending_after(start, query.before_begin);
			}

			if (query.begin.get_iterator() == _ranges.end()) {
				return;
			}
			// ranges starting after `start`
			bool blank_inserted = false;
			std::size_t additional_offset = start - query.begin._pos;
			while (query.begin.get_iterator() != query.end.get_iterator()) {
				// since `move_next` does not use synthesized data, it's safe to modify element values here, and
				// `query.begin` will keep track of the old positions
				std::size_t end = query.begin.get_range_start() + query.begin.get_iterator()->length;
				iterator_position next = query.begin;
				next.move_next();
				if (end <= erase_end) {
					// fully erased, remove this entry
					_ranges.erase(query.begin.get_iterator());
				} else {
					std::size_t diff = erase_end - query.begin.get_range_start();
					{
						auto mod = _get_modifier_for(query.begin.get_iterator());
						mod->length -= diff;
						if (blank_inserted) {
							mod->offset = 0;
						} else {
							mod->offset = additional_offset + inserted;
							blank_inserted = true;
						}
					}
				}
				// update
				query.begin = next;
			}
			if (query.end.get_iterator() != _ranges.end()) {
				std::size_t end_offset = query.end.get_range_start() - erase_end;
				if (!blank_inserted) {
					end_offset += additional_offset + inserted;
				}
				_get_modifier_for(query.end.get_iterator())->offset = end_offset;
			}
		}
	protected:
		tree_type _ranges; ///< The binary tree structure that stores all the ranges.

		/// Used to find the first range that starts at or after the given position.
		using _position_finder = sum_synthesizer::index_finder<
			typename node_data::offset_property, false, std::less_equal<>
		>;
		/// Used to find the first range that starts after the given position.
		using _position_finder_exclusive = sum_synthesizer::index_finder<typename node_data::offset_property>;
		/// Used to find the first range that ends at or after the given position.
		struct _extent_finder {
			/// Interface for \ref binary_tree::find_custom().
			int select_find(const typename node_data::node &n, std::size_t &target) {
				if (n.left) {
					if (target <= n.left->synth_data.maximum_end_position) {
						// left branch extends over `target`
						return -1;
					}
					target -= n.left->synth_data.offset_sum;
				}
				// this node ends after `target`
				if (target <= n.value.offset + n.value.length) {
					return 0;
				}
				// right branch
				target -= n.value.offset;
				return 1;
			}
		};


		/// Finds the range and its position using the given \p Finder.
		template <typename Finder> [[nodiscard]] iterator_position _find(Finder &&finder, std::size_t pos) {
			iterator_position result;
			std::size_t offset = pos;
			result._iter = _ranges.find(std::forward<Finder>(finder), offset);
			result._pos = pos - offset;
			return result;
		}
		/// Returns a modifier for the given \ref iterator.
		[[nodiscard]] typename tree_type::binary_tree_t::template node_value_modifier<> _get_modifier_for(
			iterator iter
		) {
			return _ranges.get_modifier_for(iter.get_node());
		}
	};
}
