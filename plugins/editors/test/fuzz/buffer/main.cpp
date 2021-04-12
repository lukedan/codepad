// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Stress-test for the buffer. This file randomly inserts and removes bytes and characters from the buffer and then
/// decodes it to check that the contents haven't been corrupted.

#include <random>
#include <atomic>
#include <csignal>
#include <chrono>

#include <codepad/core/fuzz_test.h>
#include <codepad/editors/manager.h>
#include <codepad/editors/code/interpretation.h>

namespace cp = codepad;

std::pair<std::size_t, std::size_t>
	caret_count_range{ 1, 100 }, ///< Possible range of the number of inserted clips.
	clip_length_range{ 0,3000 }; ///< Possible range of the length of inserted clips.

/// Buffer fuzz test.
class buffer_test : public cp::fuzz_test {
public:
	/// Returns the name of this test.
	std::u8string_view get_name() const override {
		return u8"buffer_test";
	}

	/// Logs the current length of the document.
	void log_status(cp::logger::log_entry &entry) const override {
		entry <<
			"Document length: " <<
			_buffer->length() << " bytes, " <<
			_interp->get_linebreaks().num_chars() << " chars";
	}

	/// Initializes the buffers and registers for events.
	void initialize() override {
		_buffer = _manager.new_file();
		_interp = _manager.open_interpretation(*_buffer, *_encodings.get_encoding(u8"UTF-8"));

		// stored values from `modification_decoded`
		_interp->modification_decoded +=
			[&](cp::editors::code::interpretation::modification_decoded_info &info) {
				_start_char_beforemod = info.start_character;
				_past_end_char_beforemod = info.past_end_character;
				_start_cp = info.start_codepoint;
				_past_end_cp_aftermod = info.past_last_inserted_codepoint;
				_start_byte = info.start_byte;
				_past_end_byte_aftermod = info.past_end_byte;
				if (
					(info.buffer_info.bytes_erased.length() > 0 || info.buffer_info.bytes_inserted.length() > 0) &&
					info.past_end_line_column.position_in_line > info.past_end_line_column.line_iterator->nonbreak_chars
				) {
					++_past_end_char_beforemod;
				}
				_num_chars_beforemod = _interp->get_linebreaks().num_chars();
				_old_linebreaks = _interp->get_linebreaks();
			};
		_interp->end_modification +=
			[&](cp::editors::code::interpretation::end_modification_info &info) {
				// validate the character indices in `info`
				auto [begin_line_col, begin_char] =
					_interp->get_linebreaks().get_line_and_column_and_char_of_codepoint(_start_cp);
				auto [end_line_col, end_char] =
					_interp->get_linebreaks().get_line_and_column_and_char_of_codepoint(_past_end_cp_aftermod);
				if (
					(info.buffer_info.bytes_erased.length() > 0 || info.buffer_info.bytes_inserted.length() > 0) &&
					end_line_col.position_in_line > end_line_col.line_iterator->nonbreak_chars
				) {
					++end_char;
				}
				std::size_t num_chars = _interp->get_linebreaks().num_chars();
				std::size_t
					after_end_beforemod = _num_chars_beforemod - _past_end_char_beforemod,
					after_end = num_chars - end_char;
				std::size_t
					expected_beg = std::min(begin_char, _start_char_beforemod),
					expected_after_end = std::min(after_end_beforemod, after_end);
				std::size_t
					expected_removed_chars = _num_chars_beforemod - (expected_after_end + expected_beg),
					expected_inserted_chars = num_chars - (expected_after_end + expected_beg);

				cp::assert_true_logical(info.start_character == expected_beg, "incorrect start character position");
				cp::assert_true_logical(
					info.removed_characters == expected_removed_chars, "incorrect erased character count"
				);
				cp::assert_true_logical(
					info.inserted_characters == expected_inserted_chars, "incorrect inserted character count"
				);

				auto linecol_info = _old_linebreaks.get_line_and_column_of_char(
					info.start_character + info.removed_characters
				);
				cp::assert_true_logical(
					linecol_info.line == info.erase_end_line && linecol_info.position_in_line == info.erase_end_column,
					"invalid line/column information for one-past last erased character"
				);

				_check_byte_position(_start_byte, _start_cp);
				_check_byte_position(_past_end_byte_aftermod, _past_end_cp_aftermod);
			};

		cp::editors::code::caret_set cset;
		cset.reset();
		_interp->on_insert(cset, generate_random_string(1000000), nullptr);
		cp::assert_true_logical(_interp->check_integrity());
	}

	/// Performs one random operation.
	void iterate() override {
		// generate random positions and strings for the edit
		std::vector<std::pair<std::size_t, std::size_t>> positions;
		if (random_bool()) {
			positions = get_modify_positions_random(random_int(caret_count_range));
		} else {
			positions = get_modify_positions_boundary(random_int(caret_count_range));
		}
		std::vector<cp::editors::byte_string> inserts;
		for (std::size_t i = 0; i < positions.size(); ++i) {
			double r = random_double();
			if (r < 0.1) { // 10% chance: don't insert anything
				inserts.emplace_back();
			} else if (r < 0.55) { // 45% chance: insert ranodm string
				inserts.emplace_back(generate_random_string(random_int(clip_length_range)));
			} else { // insert encoded string
				inserts.emplace_back(generate_random_encoded_string(random_int(clip_length_range)));
			}
		}

		// perform the edit
		{
			cp::editors::buffer::modifier mod(*_buffer, nullptr);
			mod.begin();
			for (std::size_t i = 0; i < positions.size(); ++i) {
				mod.modify(positions[i].first, positions[i].second - positions[i].first, inserts[i]);
			}
			cp::editors::buffer::edit dummy;
			mod.end_custom(dummy); // no history; otherwise all memory will be eaten
		}

		// validate everything
		cp::assert_true_logical(_interp->check_integrity());
	}


	/// Generates and returns a series of codepoints and encodes them using the given encoding.
	[[nodiscard]] cp::editors::byte_string generate_random_encoded_string(std::size_t length) {
		constexpr std::pair<cp::codepoint, cp::codepoint> _codepoint_dist(0, 0x10FFFF - 0x800);

		std::basic_string<cp::codepoint> str;
		for (std::size_t i = 0; i < length; ++i) {
			// 2% possibility to generate a line break character
			if (random_double() < 0.02) {
				double x = random_double();
				if (x < 0.3) {
					str.push_back(U'\r');
				} else if (x < 0.6) {
					str.push_back(U'\n');
				} else {
					// this means that `length` is just a suggestion, but it doesn't break anything
					str.push_back(U'\r');
					str.push_back(U'\n');
				}
			} else {
				cp::codepoint cp = random_int(_codepoint_dist);
				if (cp >= 0xD800) {
					cp += 0x800;
				}
				str.push_back(cp);
			}
		}
		cp::editors::byte_string res;
		for (cp::codepoint cp : str) {
			res.append(_interp->get_encoding()->encode_codepoint(cp));
		}
		return res;
	}
	/// Generates a random series of bytes.
	cp::editors::byte_string generate_random_string(std::size_t length) {
		constexpr std::pair<int, int> _byte_dist{ 0, 255 };

		cp::editors::byte_string res;
		for (std::size_t i = 0; i < length; ++i) {
			res.push_back(static_cast<std::byte>(random_int(_byte_dist)));
		}
		return res;
	}

	/// Generates a series of positions in the text used for modifications. All positions are guaranteed to be at
	/// character boundaries.
	[[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>> get_modify_positions_boundary(std::size_t count) {
		std::pair<std::size_t, std::size_t> caretpos_range(0, _interp->get_linebreaks().num_chars());
		// create carets
		std::vector<std::size_t> carets;
		for (std::size_t i = 0; i < count; ++i) {
			carets.push_back(random_int(caretpos_range));
			carets.push_back(random_int(caretpos_range));
		}
		sort(carets.begin(), carets.end());
		cp::editors::code::caret_set cset;
		for (std::size_t i = 0; i < carets.size(); i += 2) {
			// 10% chance: don't erase anything
			if (random_double() < 0.1) {
				carets[i + 1] = carets[i];
			}
			cset.add(cp::ui::caret_selection(carets[i], carets[i + 1] - carets[i], 0), cp::editors::code::caret_data());
		}

		std::vector<std::pair<std::size_t, std::size_t>> cs;
		cp::editors::code::interpretation::character_position_converter cvt(*_interp);
		for (auto it = cset.begin(); it.get_iterator() != cset.carets.end(); it.move_next()) {
			auto caret_sel = it.get_caret_selection();
			std::size_t
				p1 = cvt.character_to_byte(caret_sel.selection_begin),
				p2 = cvt.character_to_byte(caret_sel.get_selection_end());
			cs.emplace_back(p1, p2);
		}
		return cs;
	}
	/// Generates a series of completely random positions in the text used for modifications.
	[[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>> get_modify_positions_random(std::size_t count) {
		std::pair<std::size_t, std::size_t> caretpos_range(0, _buffer->length());
		// create carets
		std::vector<std::size_t> carets;
		for (std::size_t i = 0; i < count; ++i) {
			carets.push_back(random_int(caretpos_range));
			carets.push_back(random_int(caretpos_range));
		}
		sort(carets.begin(), carets.end());
		std::vector<std::pair<std::size_t, std::size_t>> cs;
		for (std::size_t i = 0; i < carets.size(); i += 2) {
			// 10% chance: don't erase anything
			if (random_double() < 0.1) {
				carets[i + 1] = carets[i];
			}
			cs.emplace_back(carets[i], carets[i + 1]);
		}
		return cs;
	}
protected:
	cp::editors::buffer_manager _manager; ///< The buffer manager.
	cp::editors::code::encoding_registry _encodings; ///< Encodings.

	std::shared_ptr<cp::editors::buffer> _buffer; ///< The buffer used for testing.
	std::shared_ptr<cp::editors::code::interpretation> _interp; ///< The interpretation used for testing.

	std::size_t
		_start_char_beforemod, ///< Value used to verify computed modification position information.
		_past_end_char_beforemod, ///< Value used to verify computed modification position information.
		_num_chars_beforemod, ///< Value used to verify computed modification position information.
		_start_cp, ///< Value used to verify computed modification position information.
		_past_end_cp_aftermod, ///< Value used to verify computed modification position information.
		_start_byte, ///< Value used to verify computed modification position information.
		_past_end_byte_aftermod; ///< Value used to verify computed modification position information.
	cp::editors::code::linebreak_registry _old_linebreaks; ///< Linebreaks before a modification.


	/// Checks that the byte and codepoint positions match.
	void _check_byte_position(std::size_t byte, std::size_t cp) {
		cp::assert_true_logical(
			_interp->codepoint_at(cp).get_raw().get_position() == byte, "incorrect byte position"
		);
	};
};

/// Entry point of the test.
int main(int argc, char **argv) {
	return cp::fuzz_test::main(argc, argv, std::make_unique<buffer_test>());
}
