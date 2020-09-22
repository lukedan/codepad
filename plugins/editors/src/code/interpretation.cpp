// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/code/interpretation.h"

/// \file
/// Implementation of interpretations.

namespace codepad::editors::code {
	interpretation::interpretation(std::shared_ptr<buffer> buf, const buffer_encoding &encoding) :
		_buf(std::move(buf)), _encoding(&encoding) {

		_begin_edit_tok = _buf->begin_edit += [this](buffer::begin_edit_info &info) {
			_on_begin_edit(info);
		};
		_end_edit_tok = _buf->end_edit += [this](buffer::end_edit_info &info) {
			_on_end_edit(info);
		};

		performance_monitor mon(u8"full_decode", performance_monitor::log_condition::always);

		// TODO maybe get rid of this intermediate buffer and add the lines to _lbs directly
		std::vector<linebreak_registry::line_info> lines;
		ui::linebreak_analyzer line_analyzer(
			[&lines](std::size_t len, ui::line_ending ending) {
			lines.emplace_back(len, ending);
			}
		);
		std::size_t
			chkbegbytes = 0, chkbegcps = 0, curcp = 0,
			splitcp = maximum_codepoints_per_chunk; // where to split the next chunk
		for (buffer::const_iterator cur = _buf->begin(); cur != _buf->end(); ++curcp) {
			if (curcp >= splitcp) {
				// break chunk before this codepoint
				std::size_t bytepos = cur.get_position();
				_chks.emplace_before(_chks.end(), chunk_data(bytepos - chkbegbytes, curcp - chkbegcps));
				chkbegbytes = bytepos;
				chkbegcps = curcp;
				splitcp = curcp + maximum_codepoints_per_chunk;
			}
			// decode codepoint
			codepoint curc = 0;
			if (!_encoding->next_codepoint(cur, _buf->end(), curc)) { // invalid codepoint?
				curc = 0; // disable linebreak detection
			}
			line_analyzer.put(curc);
		}
		// process the last chunk
		line_analyzer.finish();
		if (_buf->length() > chkbegbytes) {
			_chks.emplace_before(_chks.end(), chunk_data(_buf->length() - chkbegbytes, curcp - chkbegcps));
		}
		_lbs.insert_chars(_lbs.begin(), 0, lines);
	}

	bool interpretation::check_integrity() const {
		_buf->check_integrity();

		bool error = false;

		_chks.check_integrity();
		for (const chunk_data &chk : _chks) {
			if (chk.num_codepoints == 0 || chk.num_bytes == 0) {
				error = true;
				logger::get().log_error(CP_HERE) << "empty chunk encountered";
			}
		}

		buffer::const_iterator it = _buf->begin();
		auto chk = _chks.begin();
		std::size_t bytesbefore = 0;
		std::vector<linebreak_registry::line_info> lines;
		ui::linebreak_analyzer linebreaks(
			[&lines](std::size_t len, ui::line_ending ending) {
			lines.emplace_back(len, ending);
			}
		);
		while (it != _buf->end() && chk != _chks.end()) {
			codepoint cp;
			if (!_encoding->next_codepoint(it, _buf->end(), cp)) {
				cp = 0;
			}
			linebreaks.put(cp);
			while (chk != _chks.end()) {
				std::size_t chkend = chk->num_bytes + bytesbefore;
				if (it.get_position() < chkend) {
					break;
				}
				if (it.get_position() > chkend) {
					error = true;
					logger::get().log_error(CP_HERE) <<
						"codepoint boundary mismatch at byte " << chkend <<
						": expected " << it.get_position();
				}
				bytesbefore = chkend;
				++chk;
			}
		}
		if (it != _buf->end() || chk != _chks.end()) {
			error = true;
			if (it != _buf->end()) {
				logger::get().log_error(CP_HERE) <<
					"document length mismatch: chunks ended abruptly at byte " << it.get_position() <<
					", expected " << _buf->length();
			} else {
				logger::get().log_error(CP_HERE) <<
					"document length mismatch: got " <<
					(_chks.empty() ? 0 : _chks.root()->synth_data.total_bytes) << " bytes, expected " <<
					_buf->length() << " bytes";
			}
		}
		linebreaks.finish();

		auto explineit = lines.begin();
		auto gotlineit = _lbs.begin();
		std::size_t line = 0;
		while (explineit != lines.end() && gotlineit != _lbs.end()) {
			if (gotlineit->nonbreak_chars != explineit->nonbreak_chars) {
				error = true;
				logger::get().log_error(CP_HERE) <<
					"line length mismatch at line " << line << ", starting at codepoint " <<
					_lbs.get_beginning_codepoint_of(gotlineit) << ": expected " << explineit->nonbreak_chars <<
					", got " << gotlineit->nonbreak_chars;
			}
			if (gotlineit->ending != explineit->ending) {
				error = true;
				logger::get().log_error(CP_HERE) <<
					"linebreak type mismatch at line " << line << ", starting at codepoint " <<
					_lbs.get_beginning_codepoint_of(gotlineit) << ": expected " <<
					static_cast<int>(explineit->ending) << ", got " << static_cast<int>(gotlineit->ending);
			}
			++explineit;
			++gotlineit;
			++line;
		}
		if (_lbs.num_linebreaks() + 1 != lines.size()) {
			error = true;
			logger::get().log_error(CP_HERE) <<
				"number of lines mismatch: got " << _lbs.num_linebreaks() + 1 <<
				", expected " << lines.size();
		}

		return !error;
	}
}
