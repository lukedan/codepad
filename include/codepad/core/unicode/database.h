// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Reads and parses Unicode Character Database files.

#include <vector>
#include <string>

#include "codepad/core/misc.h"
#include "codepad/core/assert.h"
#include "common.h"

namespace codepad::unicode {
	namespace database {
		/// Parses a single line of input with semicolon-separated fields.
		template <typename Stream> [[nodiscard]] std::vector<std::u8string> parse_line(Stream &stream) {
			std::vector<std::u8string> result;
			while (stream) {
				switch (stream.peek()) {
				case '#': // skip comment
					while (stream && stream.get() != '\n') {
					}
					continue;
				case '\r':
					[[fallthrough]];
				case '\n':
					stream.get();
					continue;
				}
				if (stream.peek() == Stream::traits_type::eof()) {
					return result;
				}
				break;
			}
			std::u8string field;
			while (true) {
				auto ch = stream.get();
				if (!stream || ch == '\r' || ch == '\n') {
					break;
				}
				if (ch == ';') {
					result.emplace_back(std::exchange(field, std::u8string()));
				} else if (ch == '#') { // consume the rest of the line
					while (stream && stream.get() != '\n') {
					}
					break;
				} else {
					field.push_back(static_cast<char8_t>(ch));
				}
			}
			result.emplace_back(std::move(field));
			return result;
		}
		/// Parses the first field which may be a single codepoint or a codepoint range, and strips all other fields
		/// in the line.
		[[nodiscard]] codepoint_range finalize_line(std::vector<std::u8string>&);
		/// Parses a sequence of codepoints separated by spaces.
		[[nodiscard]] codepoint_string parse_codepoint_sequence(std::u8string_view);
	}

	/// The number of specific general Unicode categories.
	constexpr std::size_t num_generic_categories = 30;
	/// General category of a codepoint.
	enum class general_category : std::uint32_t {
		uppercase_letter = 1 << 0, ///< Lu, Uppercase_Letter.
		lowercase_letter = 1 << 1, ///< Ll, Lowercase_Letter.
		titlecase_letter = 1 << 2, ///< Lt, Titlecase_Letter.
		cased_letter     = 1 << 3, ///< LC, Cased_Letter.
		modifier_letter  = 1 << 4, ///< Lm, Modifier_Letter.
		other_letter     = 1 << 5, ///< Lo, Other_Letter.
		letter =
			uppercase_letter |
			lowercase_letter |
			titlecase_letter |
			cased_letter |
			modifier_letter |
			other_letter,///< L, Letter.

		nonspacing_mark = 1 << 6, ///< Mn, Nonspacing_Mark.
		spacing_mark    = 1 << 7, ///< Mc, Spacing_Mark.
		enclosing_mark  = 1 << 8, ///< Me, Enclosing_Mark.
		mark = nonspacing_mark | spacing_mark | enclosing_mark, ///< M, Mark.

		decimal_number = 1 << 9, ///< Nd, Decimal_Number.
		letter_number  = 1 << 10, ///< Nl, Letter_Number.
		other_number   = 1 << 11, ///< No, Other_Number.
		number = decimal_number | letter_number | other_number, ///< N, Number.

		connector_punctuation = 1 << 12, ///< Pc, Connector_Punctuation.
		dash_punctuation      = 1 << 13, ///< Pd, Dash_Punctuation.
		open_punctuation      = 1 << 14, ///< Ps, Open_Punctuation.
		close_punctuation     = 1 << 15, ///< Pe, Close_Punctuation.
		initial_punctuation   = 1 << 16, ///< Pi, Initial_Punctuation.
		final_punctuation     = 1 << 17, ///< Pf, Final_Punctuation.
		other_punctuation     = 1 << 18, ///< Po, Other_Punctuation.
		punctuation =
			connector_punctuation |
			dash_punctuation |
			open_punctuation |
			close_punctuation |
			initial_punctuation |
			final_punctuation |
			other_punctuation, ///< P, Punctuation.

		math_symbol     = 1 << 19, ///< Sm, Math_Symbol.
		currency_symbol = 1 << 20, ///< Sc, Currency_Symbol.
		modifier_symbol = 1 << 21, ///< Sk, Modifier_Symbol.
		other_symbol    = 1 << 22, ///< So, Other_Symbol.
		symbol = math_symbol | currency_symbol | modifier_symbol | other_symbol, ///< S, Symbol.

		space_separator     = 1 << 23, ///< Zs, Space_Separator.
		line_separator      = 1 << 24, ///< Zl, Line_Separator.
		paragraph_separator = 1 << 25, ///< Zp, Paragraph_Separator.
		separator = space_separator | line_separator | paragraph_separator, ///< Z, Separator.

		control     = 1 << 26, ///< Cc, Control.
		format      = 1 << 27, ///< Cf, Format.
		surrogate   = 1 << 28, ///< Cs, Surrogate.
		private_use = 1 << 29, ///< Co, Private_Use.
		unassigned  = 1 << 30, ///< Cn, Unassigned.
		other =
			control |
			format |
			surrogate |
			private_use |
			unassigned, ///< C, Other.

		all = letter | mark | number | punctuation | symbol | separator | other, ///< All categories.
		unknown = 0 ///< Unknown category - no bit set.
	};
}
namespace codepad {
	/// Enables bitwise operators for \ref unicode::general_category.
	template <> struct enable_enum_bitwise_operators<unicode::general_category> : public std::true_type {
	};
	/// Parser for \ref unicode::general_category.
	template <> struct enum_parser<unicode::general_category> {
		/// The parser interface.
		static std::optional<unicode::general_category> parse(std::u8string_view);
	};
}

namespace codepad::unicode {
	/// Indices corresponding to non-generic categories in \ref general_category.
	enum class general_category_index : std::uint8_t {
		uppercase_letter = 0, ///< Lu, Uppercase_Letter.
		lowercase_letter = 1, ///< Ll, Lowercase_Letter.
		titlecase_letter = 2, ///< Lt, Titlecase_Letter.
		cased_letter = 3, ///< LC, Cased_Letter.
		modifier_letter = 4, ///< Lm, Modifier_Letter.
		other_letter = 5, ///< Lo, Other_Letter.
		nonspacing_mark = 6, ///< Mn, Nonspacing_Mark.
		spacing_mark = 7, ///< Mc, Spacing_Mark.
		enclosing_mark = 8, ///< Me, Enclosing_Mark.
		decimal_number = 9, ///< Nd, Decimal_Number.
		letter_number = 10, ///< Nl, Letter_Number.
		other_number = 11, ///< No, Other_Number.
		connector_punctuation = 12, ///< Pc, Connector_Punctuation.
		dash_punctuation = 13, ///< Pd, Dash_Punctuation.
		open_punctuation = 14, ///< Ps, Open_Punctuation.
		close_punctuation = 15, ///< Pe, Close_Punctuation.
		initial_punctuation = 16, ///< Pi, Initial_Punctuation.
		final_punctuation = 17, ///< Pf, Final_Punctuation.
		other_punctuation = 18, ///< Po, Other_Punctuation.
		math_symbol = 19, ///< Sm, Math_Symbol.
		currency_symbol = 20, ///< Sc, Currency_Symbol.
		modifier_symbol = 21, ///< Sk, Modifier_Symbol.
		other_symbol = 22, ///< So, Other_Symbol.
		space_separator = 23, ///< Zs, Space_Separator.
		line_separator = 24, ///< Zl, Line_Separator.
		paragraph_separator = 25, ///< Zp, Paragraph_Separator.
		control = 26, ///< Cc, Control.
		format = 27, ///< Cf, Format.
		surrogate = 28, ///< Cs, Surrogate.
		private_use = 29, ///< Co, Private_Use.
		unassigned = 30, ///< Cn, Unassigned.

		num_categories = 31 ///< The number of categories.
	};
	/// Converts a \ref general_category_index to a \ref general_category.
	[[nodiscard]] constexpr inline general_category general_category_index_to_cateogry(general_category_index id) {
		return static_cast<general_category>(1u << static_cast<std::uint32_t>(id));
	}


	/// Bidirectional layout class.
	enum class bidi_class {
		left_to_right, ///< L, Left_To_Right.
		right_to_left, ///< R, Right_To_Left.
		arabic_letter, ///< AL, Arabic_Letter.

		european_number, ///< EN, European_Number.
		european_separator, ///< ES, European_Separator.
		european_terminator, ///< ET, European_Terminator.
		arabic_number, ///< AN, Arabic_Number.
		common_separator, ///< CS, Common_Separator.
		nonspacing_mark, ///< NSM, Nonspacing_Mark.
		boundary_neutral, ///< BN, Boundary_Neutral.

		paragraph_separator, ///< B, Paragraph_Separator.
		segment_separator, ///< S, Segment_Separator.
		white_space, ///< WS, White_Space.
		other_neutral, ///< ON, Other_Neutral.

		left_to_right_embedding, ///< LRE, Left_To_Right_Embedding.
		left_to_right_override, ///< LRO, Left_To_Right_Override.
		right_to_left_embedding, ///< RLE, Right_To_Left_Embedding.
		right_to_left_override, ///< RLO, Right_To_Left_Override.
		pop_directional_format, ///< PDF, Pop_Directional_Format.
		left_to_right_isolate, ///< LRI, Left_To_Right_Isolate.
		right_to_left_isolate, ///< RLI, Right_To_Left_Isolate.
		first_strong_isolate, ///< FSI, First_Strong_Isolate.
		pop_directional_isolate, ///< PDI, Pop_Directional_Isolate.

		unknown, ///< Unknown class.
	};
}
namespace codepad {
	/// Parser for \ref unicode::bidi_class.
	template <> struct enum_parser<unicode::bidi_class> {
		/// The parser interface.
		static std::optional<unicode::bidi_class> parse(std::u8string_view);
	};
}

namespace codepad::unicode {
	/// Database contained in UnicodeData.txt.
	struct unicode_data {
		/// An entry in the database.
		struct entry {
			codepoint_range value; ///< The value of the codepoint or range.
			std::u8string name; ///< The name of this codepoint.
			general_category category = general_category::unknown; ///< The category of this character.
			std::size_t canonical_combining_class = 0; ///< Canonical combining class.
			bidi_class bidi_cls = bidi_class::unknown; ///< Bidi class.
			// TODO

			/// Parses a data entry from the given fields separated by semicolons.
			[[nodiscard]] static entry parse(std::vector<std::u8string>);
		};

		/// Cached Unicode data.
		struct cache {
			/// Returns the Unicode database, loading it if necessary.
			[[nodiscard]] static const unicode_data &get_database();
			/// Returns all codepoints in the given category, computing it if necessary.
			[[nodiscard]] static const codepoint_range_list &get_codepoints_in_category(general_category_index);
			/// Returns all codepoints in the given category or categories, computing it if necessary.
			[[nodiscard]] static const codepoint_range_list &get_codepoints_in_category(general_category);
		};

		std::vector<entry> entries; ///< Entries in this database.

		/// Returns a list of all codepoints in the given category.
		[[nodiscard]] codepoint_range_list get_codepoints_in_category(general_category) const;

		/// Parses the entire UnicodeData.txt, handling character ranges.
		[[nodiscard]] static unicode_data parse(const std::filesystem::path&);
	};

	/// Lists of codepoints with specific properties contained in PropList.txt.
	struct property_list {
		/// Spaces, separator characters and other control characters which should be treated by programming
		/// languages as `white space' for the purpose of parsing elements.
		codepoint_range_list white_space;
		// TODO

		/// Parses the entire PropList.txt.
		[[nodiscard]] static property_list parse(const std::filesystem::path&);
		/// Returns the global property list, loading it if necessary.
		[[nodiscard]] static const property_list &get_cached();
	};

	/// The case folding database in CaseFolding.txt.
	struct case_folding {
		std::unordered_map<codepoint, codepoint> simple; ///< Simple case folding.
		std::unordered_map<codepoint, codepoint_string> full; ///< Full case folding.

		/// Folds the given codepoint with only the simple folding rules.
		[[nodiscard]] codepoint fold_simple(codepoint cp) const {
			if (auto it = simple.find(cp); it != simple.end()) {
				return it->second;
			}
			return cp;
		}
		/// Folds the given codepoint with the full folding rules.
		[[nodiscard]] codepoint_string fold_full(codepoint cp) const {
			if (auto it = full.find(cp); it != full.end()) {
				return it->second;
			}
			return codepoint_string{ fold_simple(cp) };
		}

		/// Parses the entire CaseFolding.txt.
		[[nodiscard]] static case_folding parse(const std::filesystem::path&);
		/// Returns the global case folding, loading it if necessary.
		[[nodiscard]] static const case_folding &get_cached();
	};
}
