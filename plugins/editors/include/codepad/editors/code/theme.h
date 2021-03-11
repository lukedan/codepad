// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes used to record and manage font color, style, etc. in a \ref codepad::editors::code::interpretation.

#include <codepad/core/red_black_tree.h>
#include <codepad/ui/renderer.h>

namespace codepad::editors::code {
	/// Specifies the theme of the text at a specific point.
	struct text_theme_specification {
		/// Default constructor.
		text_theme_specification() = default;
		/// Initializes all members of this struct.
		text_theme_specification(colord c, ui::font_style st, ui::font_weight w) : color(c), style(st), weight(w) {
		}

		colord color; ///< The color of the text.
		ui::font_style style = ui::font_style::normal; ///< The font style.
		ui::font_weight weight = ui::font_weight::normal; ///< The font weight.
	};
}
namespace codepad::ui {
	/// Managed parser for \ref text_theme_specification.
	template <> struct managed_json_parser<editors::code::text_theme_specification> {
	public:
		/// Initializes \ref _manager.
		explicit managed_json_parser(manager &man) : _manager(man) {
		}

		/// Parses the theme specification.
		template <typename Value> std::optional<
			editors::code::text_theme_specification
		> operator()(const Value&) const;
	protected:
		manager &_manager; ///< The associated \ref manager.
	};
}
namespace codepad::editors::code {
	/// Records a parameter of the theme of the entire buffer.
	template <typename T> class text_theme_parameter_info {
	public:
		/// Data for a segment of text that has the parameter set to the given value.
		struct segment_data {
			/// Default constructor.
			segment_data() = default;
			/// Initializes \ref value and \ref length.
			segment_data(T val, std::size_t len) : value(std::move(val)), length(len) {
			}

			T value{}; ///< The property value for this segment.
			std::size_t length = 0; ///< The length of this segment, in characters.
			red_black_tree::color color = red_black_tree::color::red; ///< The color of this node.
		};
		/// Contains additional synthesized data of a node.
		struct node_data {
			/// A node of the tree.
			using node = binary_tree_node<segment_data, node_data>;

			std::size_t total_chars = 0; ///< The total number of characters in this subtree.

			using num_chars_property = sum_synthesizer::compact_property<
				synthesization_helper::field_value_property<&segment_data::length>,
				&node_data::total_chars
			>; ///<	Property used to obtain the total number of characters in a subtree.

			/// Refreshes the given node's data.
			inline static void synthesize(node &n) {
				sum_synthesizer::synthesize<num_chars_property>(n);
			}
		};
		/// Type of binary tree used for storing theme information.
		using tree_type = red_black_tree::tree<
			segment_data, red_black_tree::member_red_black_access<&segment_data::color>, node_data
		>;
		/// Iterator.
		using iterator = typename tree_type::iterator;
		/// Const iterator.
		using const_iterator = typename tree_type::const_iterator;

		/// Default constructor. Adds a default-initialized value to position 0.
		text_theme_parameter_info() : text_theme_parameter_info(T()) {
		}
		/// Constructor that adds the given value to position 0.
		explicit text_theme_parameter_info(T def) {
			_segments.emplace_before(_segments.end(), std::move(def), 0);
		}

		/// Clears the parameter of the theme, and adds the given value to position 0.
		void clear(T def) {
			_segments.clear();
			_segments.emplace_before(_segments.end(), std::move(def), 0);
		}
		/// Sets the parameter of the given range to the given value.
		void set_range(std::size_t s, std::size_t pe, T val) {
			if (s == pe) {
				return;
			}
			assert_true_usage(s < pe, "invalid range");
			auto [start_iter, start_pos] = get_segment_at(s);
			auto [end_iter, end_pos] = get_segment_at(pe);
			auto [insert_iter, insert_offset] = _erase_range_no_merging(
				start_iter, s - start_pos, end_iter, pe - end_pos
			);
			_insert_at(insert_iter, insert_offset, std::move(val), pe - s);
		}
		/// Invoked when a modification has been made to the \ref interpretation to update its theme data.
		void on_modification(std::size_t start, std::size_t removed_length, std::size_t inserted_length) {
			if (removed_length == 0 && inserted_length == 0) {
				return;
			}
			std::size_t past_removed = start + removed_length;
			auto [start_iter, start_pos] = get_segment_at(start);
			auto [end_iter, end_pos] = get_segment_at(past_removed);
			auto [insert_iter, insert_offset] = _erase_range_no_merging(
				start_iter, start - start_pos, end_iter, past_removed - end_pos
			);
			T value = insert_iter->value;
			if (insert_offset == 0 && insert_iter != _segments.begin()) {
				auto prev_iter = insert_iter;
				--prev_iter;
				if (prev_iter->value != value) {
					value = (--_segments.end())->value;
				}
			}
			_insert_at(insert_iter, insert_offset, std::move(value), inserted_length);
		}

		/// Retrieves the value of the parameter at the given character.
		[[nodiscard]] T get_at(std::size_t cp) const {
			return get_segment_at(cp).first->value;
		}

		/// Returns an iterator to the first pair.
		[[nodiscard]] iterator begin() {
			return _segments.begin();
		}
		/// Returns an iterator past the last pair.
		[[nodiscard]] iterator end() {
			return _segments.end();
		}
		/// Returns an iterator to the segment that contains the given character, and the number of characters before
		/// that segment.
		[[nodiscard]] std::pair<iterator, std::size_t> get_segment_at(std::size_t cp) {
			std::size_t segment_offset = cp;
			auto iter = _segments.find(_char_finder(), segment_offset);
			return { iter, cp - segment_offset };
		}
		/// Const version of \ref begin().
		[[nodiscard]] const_iterator begin() const {
			return _segments.begin();
		}
		/// Const version of \ref end().
		[[nodiscard]] const_iterator end() const {
			return _segments.end();
		}
		/// Const version of \ref get_segment_at().
		[[nodiscard]] std::pair<const_iterator, std::size_t> get_segment_at(std::size_t cp) const {
			std::size_t segment_offset = cp;
			auto iter = _segments.find(_char_finder(), segment_offset);
			return { iter, cp - segment_offset };
		}
	protected:
		/// Used to find the segment that contains the i-th character.
		using _char_finder = sum_synthesizer::index_finder<typename node_data::num_chars_property, true>;

		/// The underlying binary tree that stores the segments. The sum of segment lengths may be shorter than the
		/// document length, in which case the text after all segments will inherit the color of the last segment.
		tree_type _segments;

		/// Erases the given range of characters without merging segments that have the same value.
		///
		/// \return The position of the erased sequence after the operation.
		std::pair<iterator, std::size_t> _erase_range_no_merging(
			iterator beg_iter, std::size_t beg_offset, iterator end_iter, std::size_t end_offset
		) {
			// if beg and end are in the same segment, simply reduce the length of it
			if (beg_iter == end_iter) {
				std::size_t nchars = end_offset - beg_offset;
				beg_iter.get_modifier()->length = std::max(beg_iter->length, nchars) - nchars;
				return { beg_iter, beg_offset };
			}
			if (beg_offset > 0) {
				// reduce the length of the segment at the beginning, and increment beg_iter so that
				// [beg_iter, end_iter) can be completely erased
				// the last segment will have a length of 0, here we try not to change that
				beg_iter.get_modifier()->length = std::min(beg_iter->length, beg_offset);
				++beg_iter;
			}
			// reduce the length of the segment at the end
			if (end_offset > 0) {
				end_iter.get_modifier()->length = std::max(end_iter->length, end_offset) - end_offset;
			}
			// erase full segments
			_segments.erase(beg_iter, end_iter);
			return { end_iter, 0 };
		}
		/// Inserts a segment at the given position. This function merges segments with the same parameter value and
		/// makes sure that the last segment's length is 0 if the insertion occurs near the end.
		void _insert_at(iterator iter, std::size_t offset, T value, std::size_t length) {
			if (length == 0) {
				// no need to insert, but still merge segments
				if (offset == 0 && iter != _segments.begin()) {
					auto prev_iter = iter;
					--prev_iter;
					if (prev_iter->value == iter->value) { // needs merging
						iter.get_modifier()->length += prev_iter->length;
						_segments.erase(prev_iter);
					}
				}
			} else {
				if (offset == 0) {
					// insert at segment boundary; may need to merge segments
					auto prev_iter = iter;
					bool merge_before = false;
					if (iter != _segments.begin()) {
						--prev_iter;
						merge_before = prev_iter->value == value;
					}
					if (merge_before) {
						if (iter->value == value) {
							iter.get_modifier()->length += prev_iter->length + length;
							_segments.erase(prev_iter);
						} else {
							prev_iter.get_modifier()->length += length;
						}
					} else {
						if (iter->value == value) {
							iter.get_modifier()->length += length;
						} else {
							_segments.emplace_before(iter, std::move(value), length);
						}
					}
				} else {
					// insert at middle
					if (value == iter->value) {
						iter.get_modifier()->length += length;
					} else {
						_segments.emplace_before(iter, iter->value, offset);
						_segments.emplace_before(iter, std::move(value), length);
						iter.get_modifier()->length = std::max(iter->length, offset) - offset;
					}
				}
			}
			auto next = iter;
			if (++next == _segments.end()) {
				iter.get_modifier()->length = 0;
			}
		}
	};

	/// Bitfield indicating specific parameters of the text's theme.
	enum class text_theme_member : unsigned char {
		none = 0,
		color = 1,
		style = 2,
		weight = 4,

		all = color | style | weight
	};
}
namespace codepad {
	/// Enables bitwise operators for \ref editors::code::text_theme_member.
	template <> struct enable_enum_bitwise_operators<editors::code::text_theme_member> : public std::true_type {
	};
}

namespace codepad::editors::code {
	/// Records the text's theme across the entire buffer.
	struct text_theme_data {
		text_theme_parameter_info<colord> color; ///< Records the text's color across the ehtire buffer.
		text_theme_parameter_info<ui::font_style> style; ///< Records the text's style across the entire buffer.
		text_theme_parameter_info<ui::font_weight> weight; ///< Records the text's weight across the entire buffer.

		/// An iterator used to obtain the theme of the text at certain positions. This should only be used
		/// temporarily when the associated \ref text_theme_data isn't changing.
		struct position_iterator {
			/// Default constructor.
			position_iterator() = default;
			/// Initializes \ref current_theme from the given iterators, then initializes all \p next_* iterators by
			/// incrementing the given ones.
			position_iterator(const text_theme_data &data, std::size_t position) : _data(&data) {
				_next_color = _create_iterator_position(_data->color, position, current_theme.color);
				_next_style = _create_iterator_position(_data->style, position, current_theme.style);
				_next_weight = _create_iterator_position(_data->weight, position, current_theme.weight);
			}

			/// Moves the given \ref position_iterator to the given position. The position must be after where this
			/// \ref position_iterator was at.
			///
			/// \return All members that have potentially changed.
			text_theme_member move_forward(std::size_t pos) {
				text_theme_member result = text_theme_member::none;
				if (_next_color.move_forward(_data->color, pos, current_theme.color)) {
					result |= text_theme_member::color;
				}
				if (_next_style.move_forward(_data->style, pos, current_theme.style)) {
					result |= text_theme_member::style;
				}
				if (_next_weight.move_forward(_data->weight, pos, current_theme.weight)) {
					result |= text_theme_member::weight;
				}
				return result;
			}
			/// Returns the number of characters to the next change of any parameter, given the current position.
			std::size_t forecast(std::size_t pos) const {
				std::size_t res = _next_color.forecast(_data->color, pos);
				res = std::min(res, _next_style.forecast(_data->style, pos));
				res = std::min(res, _next_weight.forecast(_data->weight, pos));
				return res;
			}

			text_theme_specification current_theme; ///< The current theme of the text.
		protected:
			/// Contains an iterator and its corresponding position.
			template <typename T> struct _iterator_position {
				/// Default constructor.
				_iterator_position() = default;

				typename text_theme_parameter_info<T>::const_iterator iterator; ///< The iterator.
				/// The position where the next parameter change will occur.
				std::size_t next_position = 0;

				/// Moves \ref iterator and \ref next_position forward.
				///
				/// \return Whether \ref value has been changed.
				bool move_forward(const text_theme_parameter_info<T> &param, std::size_t newpos, T &value) {
					if (newpos < next_position || iterator == param.end()) {
						return false;
					}
					// fast path: only needs to move to the next segment
					next_position += iterator->length;
					if (newpos < next_position) {
						value = iterator->value;
						++iterator;
						return true;
					}
					// slow path: recompute
					auto [iter, pos] = param.get_segment_at(newpos);
					iterator = iter;
					next_position = pos + iterator->length;
					value = iterator->value;
					++iterator;
					return true;
				}
				/// Returns the distance to where the next parameter change will happen.
				[[nodiscard]] std::size_t forecast(
					const text_theme_parameter_info<T> &param, std::size_t position
				) const {
					if (iterator != param.end()) {
						return next_position - position;
					}
					return std::numeric_limits<std::size_t>::max();
				}
			};
			/// Creates a \ref _iterator_position for the given parameter at the given position.
			template <typename T> [[nodiscard]] inline static _iterator_position<T> _create_iterator_position(
				const text_theme_parameter_info<T> &param, std::size_t pos, T &value
			) {
				_iterator_position<T> result;
				auto [iter, begin_pos] = param.get_segment_at(pos);
				result.iterator = iter;
				result.next_position = begin_pos + result.iterator->length;
				value = result.iterator->value;
				++result.iterator;
				return result;
			}


			/// The iterator to the next position-color pair.
			_iterator_position<colord> _next_color;
			/// The iterator to the next position-\ref ui::font_style pair.
			_iterator_position<ui::font_style> _next_style;
			/// The iterator to the next position-\ref ui::font_weight pair.
			_iterator_position<ui::font_weight> _next_weight;
			const text_theme_data *_data = nullptr; ///< The associated \ref text_theme_data.
		};

		/// Calls \ref clear() with a default-initialized \ref text_theme_specification.
		text_theme_data() {
			clear(text_theme_specification());
		}

		/// Sets the theme of the text in the given range.
		void set_range(std::size_t s, std::size_t pe, text_theme_specification tc) {
			color.set_range(s, pe, tc.color);
			style.set_range(s, pe, tc.style);
			weight.set_range(s, pe, tc.weight);
		}
		/// Called when the interpretation is modified to update the theme data associated with it.
		void on_modification(std::size_t start, std::size_t erased_length, std::size_t inserted_length) {
			color.on_modification(start, erased_length, inserted_length);
			style.on_modification(start, erased_length, inserted_length);
			weight.on_modification(start, erased_length, inserted_length);
		}

		/// Returns the theme of the text at the given position.
		text_theme_specification get_at(std::size_t p) const {
			return text_theme_specification(color.get_at(p), style.get_at(p), weight.get_at(p));
		}
		/// Sets the theme of all text to the given value.
		void clear(const text_theme_specification &def) {
			color.clear(def.color);
			style.clear(def.style);
			weight.clear(def.weight);
		}
	};
}
