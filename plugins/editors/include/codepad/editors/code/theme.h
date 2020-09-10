// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes used to record and manage font color, style, etc. in a \ref codepad::editors::code::interpretation.

#include <codepad/ui/renderer.h>

namespace codepad::editors::code {
	/// Specifies the theme of the text.
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
	/// Records a parameter of the theme of the entire buffer. Internally, it keeps a list of
	/// (position, value) pairs, and characters will use the last value specified before it.
	template <typename T> class text_theme_parameter_info {
	public:
		/// Iterator.
		using iterator = typename std::map<std::size_t, T>::iterator;
		/// Const iterator.
		using const_iterator = typename std::map<std::size_t, T>::const_iterator;

		/// Default constructor. Adds a default-initialized value to position 0.
		text_theme_parameter_info() : text_theme_parameter_info(T()) {
		}
		/// Constructor that adds the given value to position 0.
		explicit text_theme_parameter_info(T def) {
			_changes.emplace(0, std::move(def));
		}

		/// Clears the parameter of the theme, and adds the given value to position 0.
		void clear(T def) {
			_changes.clear();
			_changes[0] = def;
		}
		/// Sets the parameter of the given range to the given value.
		void set_range(std::size_t s, std::size_t pe, T c) {
			assert_true_usage(s < pe, "invalid range");
			auto beg = get_iter_at(s), end = get_iter_at(pe);
			T begv = beg->second, endv = end->second;
			_changes.erase(++beg, ++end);
			if (begv != c) {
				_changes[s] = c;
			}
			if (endv != c) {
				_changes[pe] = endv;
			}
		}
		/// Retrieves the value of the parameter at the given position.
		T get_at(std::size_t cp) const {
			return get_iter_at(cp)->second;
		}

		/// Returns an iterator to the first pair.
		iterator begin() {
			return _changes.begin();
		}
		/// Returns an iterator past the last pair.
		iterator end() {
			return _changes.end();
		}
		/// Returns an iterator to the pair that determines the parameter at the given position.
		iterator get_iter_at(std::size_t cp) {
			return --_changes.upper_bound(cp);
		}
		/// Const version of \ref begin().
		const_iterator begin() const {
			return _changes.begin();
		}
		/// Const version of \ref end().
		const_iterator end() const {
			return _changes.end();
		}
		/// Const version of \ref get_iter_at(std::size_t).
		const_iterator get_iter_at(std::size_t cp) const {
			return --_changes.upper_bound(cp);
		}

		/// Returns the number of position-value pairs in this parameter.
		std::size_t size() const {
			return _changes.size();
		}
	protected:
		std::map<std::size_t, T> _changes; ///< The underlying \p std::map that stores the position-value pairs.
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
			position_iterator(const text_theme_data &data, std::size_t position) :
				_next_color(data.color.get_iter_at(position)),
				_next_style(data.style.get_iter_at(position)),
				_next_weight(data.weight.get_iter_at(position)),
				_data(&data) {

				current_theme = text_theme_specification(
					_next_color->second, _next_style->second, _next_weight->second
				);
				++_next_color;
				++_next_style;
				++_next_weight;
			}

			/// Moves the given \ref position_iterator to the given position. The position must be after where this
			/// \ref position_iterator was at.
			///
			/// \return All members that have potentially changed.
			text_theme_member move_forward(std::size_t pos) {
				text_theme_member result = text_theme_member::none;
				if (_move_iter_forward(_next_color, _data->color, current_theme.color, pos)) {
					result |= text_theme_member::color;
				}
				if (_move_iter_forward(_next_style, _data->style, current_theme.style, pos)) {
					result |= text_theme_member::style;
				}
				if (_move_iter_forward(_next_weight, _data->weight, current_theme.weight, pos)) {
					result |= text_theme_member::weight;
				}
				return result;
			}
			/// Returns the number of characters to the next change of any parameter, given the current position.
			std::size_t forecast(std::size_t pos) const {
				std::size_t res = _forecast_iter(_next_color, _data->color, pos);
				res = std::min(res, _forecast_iter(_next_style, _data->style, pos));
				res = std::min(res, _forecast_iter(_next_weight, _data->weight, pos));
				return res;
			}

			text_theme_specification current_theme; ///< The current theme of the text.
		protected:
			/// The iterator to the next position-color pair.
			text_theme_parameter_info<colord>::const_iterator _next_color;
			/// The iterator to the next position-\ref ui::font_style pair.
			text_theme_parameter_info<ui::font_style>::const_iterator _next_style;
			/// The iterator to the next position-\ref ui::font_weight pair.
			text_theme_parameter_info<ui::font_weight>::const_iterator _next_weight;
			const text_theme_data *_data = nullptr; ///< The associated \ref text_theme_data.

			/// Called by \ref move_forward(). Increments a \ref text_theme_parameter_info::const_iterator if
			/// necessary.
			///
			/// \param it The iterator.
			/// \param spec The set of position-value pairs.
			/// \param val The value at the position.
			/// \param pos The new position in the text.
			/// \return \p true if the iterator has been moved.
			template <typename T> inline static bool _move_iter_forward(
				typename text_theme_parameter_info<T>::const_iterator &it,
				const text_theme_parameter_info<T> &spec, T &val, std::size_t pos
			) {
				if (it != spec.end() && it->first <= pos) { // fast case: only need to increment once
					val = it->second;
					++it;
					if (it != spec.end() && it->first <= pos) { // slow: reposition
						it = spec.get_iter_at(pos);
						val = it->second;
						++it;
					}
					return true;
				}
				return false;
			}

			/// \ref forecast() for a single parameter.
			template <typename T> inline static std::size_t _forecast_iter(
				const typename text_theme_parameter_info<T>::const_iterator &it,
				const text_theme_parameter_info<T> &spec, std::size_t pos
			) {
				if (it != spec.end()) {
					return it->first - pos;
				}
				return std::numeric_limits<std::size_t>::max();
			}
		};

		/// Sets the theme of the text in the given range.
		void set_range(std::size_t s, std::size_t pe, text_theme_specification tc) {
			color.set_range(s, pe, tc.color);
			style.set_range(s, pe, tc.style);
			weight.set_range(s, pe, tc.weight);
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
