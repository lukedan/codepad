// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/core/unicode/database.h"

/// \file
/// Implementation of Unicode Database parsers.

#include <fstream>
#include <map>

namespace codepad {
	namespace unicode::database {
		codepoint_range finalize_line(std::vector<std::u8string> &fields) {
			// strip fields
			for (auto &f : fields) {
				while (!f.empty() && (f.back() == ' ' || f.back() == '\t')) {
					f.pop_back();
				}
				std::size_t i = 0;
				for (; i < f.size() && (f[i] == ' ' || f[i] == '\t'); ++i) {
				}
				f.erase(0, i);
			}
			// parse codepoint range
			auto *str_data = reinterpret_cast<const char*>(fields[0].data());
			for (std::size_t i = 0; i < fields[0].size(); ++i) {
				if (fields[0][i] == '.') {
					assert_true_sys(fields[0][i + 1] == '.', "invalid codepoint range format");
					codepoint_range result;
					std::from_chars(str_data, str_data + i, result.first, 16);
					std::from_chars(str_data + i + 2, str_data + fields[0].size(), result.last, 16);
					return result;
				}
			}
			codepoint result;
			std::from_chars(str_data, str_data + fields[0].size(), result, 16);
			return codepoint_range(result);
		}

		codepoint_string parse_codepoint_sequence(std::u8string_view seq) {
			const auto *seq_data = reinterpret_cast<const char*>(seq.data());
			const auto *end = seq_data + seq.size();
			codepoint_string res;
			while (seq_data != end) {
				codepoint cp;
				auto from_chars_res = std::from_chars(seq_data, end, cp, 16);
				assert_true_sys(from_chars_res.ptr != seq_data, "invalid codepoint sequence");
				res.push_back(cp);
				seq_data = from_chars_res.ptr;
				while (seq_data != end && *seq_data == ' ') {
					++seq_data;
				}
			}
			return res;
		}
	}


	std::optional<unicode::general_category> enum_parser<unicode::general_category>::parse(std::u8string_view str) {
		static std::unordered_map<std::u8string_view, unicode::general_category> _mapping{
			{ u8"Lu", unicode::general_category::uppercase_letter },
			{ u8"Ll", unicode::general_category::lowercase_letter },
			{ u8"Lt", unicode::general_category::titlecase_letter },
			{ u8"LC", unicode::general_category::cased_letter },
			{ u8"Lm", unicode::general_category::modifier_letter },
			{ u8"Lo", unicode::general_category::other_letter },
			{ u8"L", unicode::general_category::letter },
			{ u8"Mn", unicode::general_category::nonspacing_mark },
			{ u8"Mc", unicode::general_category::spacing_mark },
			{ u8"Me", unicode::general_category::enclosing_mark },
			{ u8"M", unicode::general_category::mark },
			{ u8"Nd", unicode::general_category::decimal_number },
			{ u8"Nl", unicode::general_category::letter_number },
			{ u8"No", unicode::general_category::other_number },
			{ u8"N", unicode::general_category::number },
			{ u8"Pc", unicode::general_category::connector_punctuation },
			{ u8"Pd", unicode::general_category::dash_punctuation },
			{ u8"Ps", unicode::general_category::open_punctuation },
			{ u8"Pe", unicode::general_category::close_punctuation },
			{ u8"Pi", unicode::general_category::initial_punctuation },
			{ u8"Pf", unicode::general_category::final_punctuation },
			{ u8"Po", unicode::general_category::other_punctuation },
			{ u8"P", unicode::general_category::punctuation },
			{ u8"Sm", unicode::general_category::math_symbol },
			{ u8"Sc", unicode::general_category::currency_symbol },
			{ u8"Sk", unicode::general_category::modifier_symbol },
			{ u8"So", unicode::general_category::other_symbol },
			{ u8"S", unicode::general_category::symbol },
			{ u8"Zs", unicode::general_category::space_separator },
			{ u8"Zl", unicode::general_category::line_separator },
			{ u8"Zp", unicode::general_category::paragraph_separator },
			{ u8"Z", unicode::general_category::separator },
			{ u8"Cc", unicode::general_category::control },
			{ u8"Cf", unicode::general_category::format },
			{ u8"Cs", unicode::general_category::surrogate },
			{ u8"Co", unicode::general_category::private_use },
			{ u8"Cn", unicode::general_category::unassigned },
			{ u8"C", unicode::general_category::other }
		};

		if (auto it = _mapping.find(str); it != _mapping.end()) {
			return it->second;
		}
		return std::nullopt;
	}


	std::optional<unicode::bidi_class> enum_parser<unicode::bidi_class>::parse(std::u8string_view str) {
		static std::unordered_map<std::u8string_view, unicode::bidi_class> _mapping{
			{ u8"L", unicode::bidi_class::left_to_right },
			{ u8"R", unicode::bidi_class::right_to_left },
			{ u8"AL", unicode::bidi_class::arabic_letter },
			{ u8"EN", unicode::bidi_class::european_number },
			{ u8"ES", unicode::bidi_class::european_separator },
			{ u8"ET", unicode::bidi_class::european_terminator },
			{ u8"AN", unicode::bidi_class::arabic_number },
			{ u8"CS", unicode::bidi_class::common_separator },
			{ u8"NSM", unicode::bidi_class::nonspacing_mark },
			{ u8"BN", unicode::bidi_class::boundary_neutral },
			{ u8"B", unicode::bidi_class::paragraph_separator },
			{ u8"S", unicode::bidi_class::segment_separator },
			{ u8"WS", unicode::bidi_class::white_space },
			{ u8"ON", unicode::bidi_class::other_neutral },
			{ u8"LRE", unicode::bidi_class::left_to_right_embedding },
			{ u8"LRO", unicode::bidi_class::left_to_right_override },
			{ u8"RLE", unicode::bidi_class::right_to_left_embedding },
			{ u8"RLO", unicode::bidi_class::right_to_left_override },
			{ u8"PDF", unicode::bidi_class::pop_directional_format },
			{ u8"LRI", unicode::bidi_class::left_to_right_isolate },
			{ u8"RLI", unicode::bidi_class::right_to_left_isolate },
			{ u8"FSI", unicode::bidi_class::first_strong_isolate },
			{ u8"PDI", unicode::bidi_class::pop_directional_isolate }
		};

		if (auto it = _mapping.find(str); it != _mapping.end()) {
			return it->second;
		}
		return std::nullopt;
	}


	namespace unicode {
		unicode_data::entry unicode_data::entry::parse(std::vector<std::u8string> data_entry) {
			assert_true_sys(data_entry.size() == 15, "incorrect number of fields");
			unicode_data::entry result;
			{
				auto *val_str = reinterpret_cast<const char*>(data_entry[0].c_str());
				std::from_chars(val_str, val_str + data_entry[0].size(), result.value.first, 16);
				result.value.last = result.value.first;
			}
			result.name = std::move(data_entry[1]);
			result.category = enum_parser<unicode::general_category>::parse(data_entry[2]).value();
			assert_true_sys(
				result.category != general_category::letter &&
				result.category != general_category::mark &&
				result.category != general_category::number &&
				result.category != general_category::punctuation &&
				result.category != general_category::symbol &&
				result.category != general_category::separator &&
				result.category != general_category::other,
				"database contains generic categories"
			);
			{
				auto *ccc_str = reinterpret_cast<const char*>(data_entry[3].c_str());
				std::from_chars(ccc_str, ccc_str + data_entry[3].size(), result.canonical_combining_class);
			}
			result.bidi_cls = enum_parser<unicode::bidi_class>::parse(data_entry[4]).value();

			return result;
		}


		const unicode_data &unicode_data::cache::get_database() {
			static unicode_data _database;

			if (_database.entries.empty()) {
				_database = parse("thirdparty/ucd/UnicodeData.txt");
			}
			return _database;
		}

		const codepoint_range_list &unicode_data::cache::get_codepoints_in_category(
			general_category_index category
		) {
			static std::array<
				codepoint_range_list, static_cast<std::size_t>(general_category_index::num_categories)
			> _cached;

			auto &result = _cached[static_cast<std::size_t>(category)];
			if (result.ranges.empty()) {
				result = get_database().get_codepoints_in_category(general_category_index_to_cateogry(category));
			}
			return result;
		}

		const codepoint_range_list &unicode_data::cache::get_codepoints_in_category(general_category category) {
			static std::unordered_map<general_category, codepoint_range_list> _cached;

			auto [it, inserted] = _cached.try_emplace(category);
			if (inserted) {
				it->second = get_database().get_codepoints_in_category(category);
			}
			return it->second;
		}


		codepoint_range_list unicode_data::get_codepoints_in_category(general_category category) const {
			codepoint_range_list result;
			for (const auto &entry : entries) {
				if ((entry.category & category) != general_category::unknown) {
					if (!result.ranges.empty() && result.ranges.back().last + 1 == entry.value.first) {
						result.ranges.back().last = entry.value.last;
					} else {
						result.ranges.emplace_back(entry.value);
					}
				}
			}
			return result;
		}

		unicode_data unicode_data::parse(const std::filesystem::path &path) {
			unicode_data result;
			std::ifstream fin(path);
			while (true) {
				auto line = database::parse_line(fin);
				if (line.empty()) {
					break;
				}
				entry cur = entry::parse(std::move(line));
				assert_true_sys(
					result.entries.empty() || result.entries.back().value.first < cur.value.first,
					"unicode values are not monotonically increasing"
				);
				if (cur.name.ends_with(u8", Last>")) {
					assert_true_sys(
						cur.name.starts_with(u8"<"),
						"codepoint name not wrapped in angle brackets"
					);
					constexpr std::u8string_view _suffix = u8", First>";
					auto &prev = result.entries.back();
					assert_true_sys(
						prev.name.starts_with(u8"<") && prev.name.ends_with(_suffix),
						"previous entry does not start a range"
					);
					prev.name.erase(prev.name.size() - _suffix.size());
					prev.name.erase(prev.name.begin());
					prev.value.last = cur.value.first;
				} else {
					result.entries.emplace_back(std::move(cur));
				}
			}
			return result;
		}


		property_list property_list::parse(const std::filesystem::path &path) {
			property_list result;
			std::ifstream fin(path);
			while (true) {
				auto line = database::parse_line(fin);
				if (line.empty()) {
					break;
				}
				auto range = database::finalize_line(line);
				assert_true_sys(line.size() >= 2, "invalid property");
				if (line[1] == u8"White_Space") {
					result.white_space.ranges.emplace_back(range);
				} else {
					// TODO error when all properties are handled
				}
			}
			return result;
		}

		const property_list &property_list::get_cached() {
			static property_list _list;

			if (_list.white_space.ranges.empty()) {
				_list = parse("thirdparty/ucd/PropList.txt");
			}
			return _list;
		}


		case_folding case_folding::parse(const std::filesystem::path &path) {
			case_folding result;
			std::ifstream fin(path);
			while (true) {
				auto line = database::parse_line(fin);
				if (line.empty()) {
					break;
				}
				auto range = database::finalize_line(line);
				assert_true_sys(range.first == range.last, "case folding only operates on single characters");
				assert_true_sys(line.size() >= 3, "invalid case folding entry");
				assert_true_sys(line[1].size() == 1, "invalid case folding category");
				codepoint_string replacement = database::parse_codepoint_sequence(line[2]);
				bool inserted = false;
				switch (line[1][0]) {
				case 'S':
					assert_true_logical(
						result.full.contains(range.first),
						"simple case folding does not have corresponding full folding"
					);
					[[fallthrough]];
				case 'C':
					assert_true_sys(replacement.size() == 1, "too many replacement characters");
					inserted = result.simple.emplace(range.first, replacement[0]).second;
					break;
				case 'F':
					inserted = result.full.emplace(range.first, std::move(replacement)).second;
					break;
				case 'T':
					// ignored
					inserted = true;
					break;
				}
				assert_true_logical(inserted, "duplicate case folding entries");
			}
			// compute inverse simple mapping
			for (const auto &[from, to] : result.simple) {
				result._simple_inverse_info.try_emplace(to).first->second.count++;
			}
			std::vector<std::size_t> count(result._simple_inverse_info.size(), 0);
			result._simple_inverse_data.resize(result.simple.size());
			std::size_t index = 0, total = 0;
			for (
				auto it = result._simple_inverse_info.begin();
				it != result._simple_inverse_info.end();
				++it, ++index
			) {
				it->second.begin = total;
				total += it->second.count;
				count[index] = it->second.count;
			}
			for (const auto &[from, to] : result.simple) {
				auto found = result._simple_inverse_info.find(to);
				--found->second.count;
				result._simple_inverse_data[found->second.begin + found->second.count] = from;
			}
			auto count_it = count.begin();
			for (
				auto it = result._simple_inverse_info.begin();
				it != result._simple_inverse_info.end();
				++it, ++count_it
			) {
				assert_true_logical(it->second.count == 0, "incorrect inverse simple case folding mappping");
				it->second.count = *count_it;
			}
			return result;
		}

		const case_folding &case_folding::get_cached() {
			static case_folding _res;

			if (_res.simple.empty()) {
				_res = parse("thirdparty/ucd/CaseFolding.txt");
			}
			return _res;
		}
	}
}
