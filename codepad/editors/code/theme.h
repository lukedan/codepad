// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes used to record and manage font color, style, etc. in a \ref codepad::editor::code::interpretation.

#include "../../os/font.h"

namespace codepad::editor::code {
	/// The type of a parameter of the text's theme.
	enum class text_theme_parameter {
		style, ///< The `style' parameter, corresponding to \ref font_style.
		color ///< The `color' parameter.
	};
	/// Specifies the theme of the text.
	struct text_theme_specification {
		/// Default constructor.
		text_theme_specification() = default;
		/// Initializes the struct with the given parameters.
		text_theme_specification(font_style fs, colord c) : style(fs), color(c) {
		}
		font_style style = font_style::normal; ///< The style of the font.
		colord color; ///< The color of the text.
	};
	/// Records a parameter of the theme of the entire buffer. Internally, it keeps a list of
	/// (position, value) pairs, and characters will use the last value specified before it.
	template <typename T> class text_theme_parameter_info {
	public:
		/// Iterator.
		using iterator = typename std::map<size_t, T>::iterator;
		/// Const iterator.
		using const_iterator = typename std::map<size_t, T>::const_iterator;

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
		void set_range(size_t s, size_t pe, T c) {
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
		T get_at(size_t cp) const {
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
		iterator get_iter_at(size_t cp) {
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
		/// Const version of \ref get_iter_at(size_t).
		const_iterator get_iter_at(size_t cp) const {
			return --_changes.upper_bound(cp);
		}

		/// Returns the number of position-value pairs in this parameter.
		size_t size() const {
			return _changes.size();
		}
	protected:
		std::map<size_t, T> _changes; ///< The underlying \p std::map that stores the position-value pairs.
	};
	/// Records the text's theme across the entire buffer.
	struct text_theme_data {
		text_theme_parameter_info<font_style> style; ///< Redords the text's style across the ehtire buffer.
		text_theme_parameter_info<colord> color; ///< Redords the text's color across the ehtire buffer.

		/// An iterator used to obtain the theme of the text at a certain position.
		struct char_iterator {
			text_theme_specification current_theme; ///< The current theme of the text.
			/// The iterator to the next position-style pair.
			text_theme_parameter_info<font_style>::const_iterator next_style_iterator;
			/// The iterator to the next position-color pair.
			text_theme_parameter_info<colord>::const_iterator next_color_iterator;
		};

		/// Sets the theme of the text in the given range.
		void set_range(size_t s, size_t pe, text_theme_specification tc) {
			color.set_range(s, pe, tc.color);
			style.set_range(s, pe, tc.style);
		}
		/// Returns the theme of the text at the given position.
		text_theme_specification get_at(size_t p) const {
			return text_theme_specification(style.get_at(p), color.get_at(p));
		}
		/// Sets the theme of all text to the given value.
		void clear(const text_theme_specification &def) {
			style.clear(def.style);
			color.clear(def.color);
		}

		/// Returns a \ref char_iterator specifying the text theme at the given position.
		char_iterator get_iter_at(size_t p) const {
			char_iterator rv;
			rv.next_style_iterator = style.get_iter_at(p);
			rv.next_color_iterator = color.get_iter_at(p);
			assert_true_logical(rv.next_style_iterator != style.end(), "empty theme parameter info encountered");
			assert_true_logical(rv.next_color_iterator != color.end(), "empty theme parameter info encountered");
			rv.current_theme = text_theme_specification(rv.next_style_iterator->second, rv.next_color_iterator->second);
			++rv.next_style_iterator;
			++rv.next_color_iterator;
			return rv;
		}
	protected:
		/// Used when incrementing a \ref char_iterator, to check whether a
		/// \ref text_theme_parameter_info::const_iterator needs incrementing,
		/// and to increment it if necessary.
		///
		/// \param cp The new position in the text.
		/// \param it The iterator.
		/// \param fullset The set of position-value pairs.
		/// \param fval The value at the position.
		template <typename T> inline static void _incr_iter_elem(
			size_t cp,
			typename text_theme_parameter_info<T>::const_iterator &it,
			const text_theme_parameter_info<T> &fullset,
			T &fval
		) {
			if (it != fullset.end() && it->first <= cp) {
				fval = it->second;
				++it;
			}
		}
	public:
		/// Moves the given \ref char_iterator to the given position. The position must be immediately after
		/// where the \ref char_iterator was originally at.
		void incr_iter(char_iterator &cv, size_t cp) const {
			_incr_iter_elem(cp, cv.next_color_iterator, color, cv.current_theme.color);
			_incr_iter_elem(cp, cv.next_style_iterator, style, cv.current_theme.style);
		}
	};
}
