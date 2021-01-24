// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/code/interpretation.h"

/// \file
/// Implementation of interpretations.

namespace codepad::editors::code {
	std::size_t interpretation::codepoint_position_converter::codepoint_to_byte(std::size_t pos) {
		if (_chunk_iter == _interp._chunks.end()) {
			// if _chunk_iter is at the end, then all following queries can only query about the end
			return _interp._buf->length();
		}
		if (_firstcp + _chunk_iter->num_codepoints > pos) {
			pos -= _firstcp;
		} else {
			// fast-forward to the chunk that contains the given codepoint
			_firstcp = pos;
			_chunk_codepoint_offset = 0;
			_codepoint_pos_converter finder;
			_chunk_iter = _interp._chunks.find(finder, pos);
			_firstcp -= pos;
			_firstbyte = finder.total_bytes;
			_byte_iter = _interp.get_buffer()->at(_firstbyte);
		}
		for (; _chunk_codepoint_offset < pos; ++_chunk_codepoint_offset) {
			_interp.get_encoding()->next_codepoint(_byte_iter, _interp.get_buffer()->end());
		}
		return _byte_iter.get_position();
	}

	std::pair<std::size_t, std::size_t> interpretation::codepoint_position_converter::byte_to_codepoint(
		std::size_t pos
	) {
		if (_chunk_iter == _interp._chunks.end()) {
			// if _chunk_iter is at the end, then all following queries can only query about the end
			return { _firstcp, _interp._buf->length() };
		}
		if (_firstbyte + _chunk_iter->num_bytes <= pos) {
			// fast-forward to the chunk that contains the given byte
			_chunk_codepoint_offset = 0;
			_byte_pos_converter finder;
			std::size_t offset_within_chunk = pos;
			_chunk_iter = _interp._chunks.find(finder, offset_within_chunk);
			_firstbyte = pos - offset_within_chunk;
			_firstcp = finder.total_codepoints;
			_byte_iter = _interp.get_buffer()->at(_firstbyte);
			_codepoint_start = _firstbyte;
		}
		for (; _byte_iter.get_position() < pos; ++_chunk_codepoint_offset) {
			_codepoint_start = _byte_iter.get_position();
			_interp.get_encoding()->next_codepoint(_byte_iter, _interp.get_buffer()->end());
		}
		std::size_t codepoint = _firstcp + _chunk_codepoint_offset;
		std::size_t byte_position = pos;
		if (_byte_iter.get_position() != pos) {
			--codepoint;
			byte_position = _codepoint_start;
		}
		return { codepoint, byte_position };
	}


	interpretation::interpretation(std::shared_ptr<buffer> buf, const buffer_encoding &encoding) :
		_buf(std::move(buf)), _encoding(&encoding) {

		_begin_edit_tok = _buf->begin_edit += [this](buffer::begin_edit_info &info) {
			_on_begin_edit(info);
		};
		_begin_modify_tok = _buf->begin_modify += [this](buffer::begin_modification_info &info) {
			_on_begin_modify(info);
		};
		_end_modify_tok = _buf->end_modify += [this](buffer::end_modification_info &info) {
			_on_end_modify(info);
		};
		_end_edit_tok = _buf->end_edit += [this](buffer::end_edit_info &info) {
			_on_end_edit(info);
		};

		performance_monitor mon(u8"full_decode", performance_monitor::log_condition::always);

		// TODO maybe get rid of this intermediate buffer and add the lines to _linebreaks directly
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
				_chunks.emplace_before(_chunks.end(), chunk_data(bytepos - chkbegbytes, curcp - chkbegcps));
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
			_chunks.emplace_before(_chunks.end(), chunk_data(_buf->length() - chkbegbytes, curcp - chkbegcps));
		}
		_linebreaks.insert_chars(_linebreaks.begin(), 0, lines);
	}

	bool interpretation::check_integrity() const {
		_buf->check_integrity();

		bool error = false;

		_chunks.check_integrity();
		for (const chunk_data &chk : _chunks) {
			if (chk.num_codepoints == 0 || chk.num_bytes == 0) {
				error = true;
				logger::get().log_error(CP_HERE) << "empty chunk encountered";
			}
		}

		buffer::const_iterator it = _buf->begin();
		auto chk = _chunks.begin();
		std::size_t bytesbefore = 0;
		std::vector<linebreak_registry::line_info> lines;
		ui::linebreak_analyzer linebreaks(
			[&lines](std::size_t len, ui::line_ending ending) {
				lines.emplace_back(len, ending);
			}
		);
		while (it != _buf->end() && chk != _chunks.end()) {
			codepoint cp;
			if (!_encoding->next_codepoint(it, _buf->end(), cp)) {
				cp = 0;
			}
			linebreaks.put(cp);
			while (chk != _chunks.end()) {
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
		if (it != _buf->end() || chk != _chunks.end()) {
			error = true;
			if (it != _buf->end()) {
				logger::get().log_error(CP_HERE) <<
					"document length mismatch: chunks ended abruptly at byte " << it.get_position() <<
					", expected " << _buf->length();
			} else {
				logger::get().log_error(CP_HERE) <<
					"document length mismatch: got " <<
					(_chunks.empty() ? 0 : _chunks.root()->synth_data.total_bytes) << " bytes, expected " <<
					_buf->length() << " bytes";
			}
		}
		linebreaks.finish();

		auto explineit = lines.begin();
		auto gotlineit = _linebreaks.begin();
		std::size_t line = 0;
		while (explineit != lines.end() && gotlineit != _linebreaks.end()) {
			if (gotlineit->nonbreak_chars != explineit->nonbreak_chars) {
				error = true;
				logger::get().log_error(CP_HERE) <<
					"line length mismatch at line " << line << ", starting at codepoint " <<
					_linebreaks.get_beginning_codepoint_of(gotlineit) << ": expected " << explineit->nonbreak_chars <<
					", got " << gotlineit->nonbreak_chars;
			}
			if (gotlineit->ending != explineit->ending) {
				error = true;
				logger::get().log_error(CP_HERE) <<
					"linebreak type mismatch at line " << line << ", starting at codepoint " <<
					_linebreaks.get_beginning_codepoint_of(gotlineit) << ": expected " <<
					static_cast<int>(explineit->ending) << ", got " << static_cast<int>(gotlineit->ending);
			}
			++explineit;
			++gotlineit;
			++line;
		}
		if (_linebreaks.num_linebreaks() + 1 != lines.size()) {
			error = true;
			logger::get().log_error(CP_HERE) <<
				"number of lines mismatch: got " << _linebreaks.num_linebreaks() + 1 <<
				", expected " << lines.size();
		}

		return !error;
	}

	void interpretation::_on_begin_modify(buffer::begin_modification_info &info) {
		// here we do the following things:
		// - find the codepoint that starts at least `maximum_codepoint_length` bytes before (inclusive) the
		//   starting position of the erased clip, and cache that position; codepoints before that codepoint are
		//   assumed to have not changed. in `_on_end_modify()` we'll start decoding from there.
		// - find all codepoint that start after the erased region, within `maximum_codepoint_length` bytes
		//   (inclusive). these starting positions are also cached, and are used in `_on_end_modify()` for
		//   terminating decoding as early as possible. it's still possible though that decoding will continue until
		//   the next old codepoint barrier or even the end of the document in the worst case (e.g., when adding a
		//   single byte in a UTF-16 document).

		std::size_t max_codepoint_length = _encoding->get_maximum_codepoint_length();

		codepoint_position_converter conv(*this);
		// first step: beginning of the removed region
		// codepoints that start at or before this byte are assumed to not be affected by this modification
		std::size_t lookahead = _encoding->get_maximum_codepoint_length() - 1;
		std::size_t first_checked_byte = std::max(info.position, lookahead) - lookahead;
		auto [start_codepoint, start_byte] = conv.byte_to_codepoint(first_checked_byte);
		_mod_cache.start_decoding_byte = start_byte;
		_mod_cache.start_decoding_codepoint = start_codepoint;
		_mod_cache.start_decoding_chunk = conv.get_chunk_iterator();
		_mod_cache.chunk_codepoint_offset = conv.get_chunk_codepoint_position();
		_mod_cache.chunk_byte_offset = conv.get_chunk_byte_position();

		// second step: after the removed region
		// these are actually past-end indices
		std::size_t
			erased_region_end = info.position + info.bytes_to_erase,
			last_checked_byte = erased_region_end + max_codepoint_length;
		auto [end_codepoint, end_byte] = conv.byte_to_codepoint(erased_region_end);
		auto iter = conv.get_buffer_iterator();
		if (end_byte != erased_region_end) { // which should imply end_byte < erased_region_end
			assert_true_logical(end_byte < erased_region_end, "invalid codepad conversion result");
			++end_codepoint;
		}
		_mod_cache.post_erase_boundaries.clear();
		_mod_cache.post_erase_codepoint_index = end_codepoint;
		auto end_iter = _buf->end();
		std::size_t current_pos = iter.get_position();
		do {
			_mod_cache.post_erase_boundaries.emplace_back(current_pos);
			if (iter == end_iter) {
				break;
			}
			_encoding->next_codepoint(iter, end_iter);
			current_pos = iter.get_position();
		} while (current_pos <= last_checked_byte);
	}

	void interpretation::_on_end_modify(buffer::end_modification_info &info) {
		// when this function is called, the underlying binary data has been modified, but `_chunks` and
		// `_linebreaks` have not been updated yet. here we start decoding from the byte position cached in
		// `_on_begin_modify()`, past the end of added content, then until we find a codepoint boundary that overlaps
		// with either old codepoint boundaries or previously cached codepoint boundaries after the erased bytes.
		// then, using these information, we find the range of codepoints and characters affected in the unmodified
		// document using the old data in `_chunks` and `_linebreaks`, and update `_chunks` and `_linebreaks`
		// accordingly

		// first, start decoding from `_mod_cache.start_decoding_byte`
		// data about the new text
		std::vector<linebreak_registry::line_info> lines;
		std::vector<chunk_data> chunks;
		// state
		ui::linebreak_analyzer linebreaks(
			[&lines](std::size_t len, ui::line_ending ending) {
				lines.emplace_back(len, ending);
			}
		);
		buffer::const_iterator
			byte_iter = _buf->at(_mod_cache.start_decoding_byte),
			end_iter = _buf->end();
		std::size_t
			current_codepoint = _mod_cache.start_decoding_codepoint,
			// starting positions of newly created chunks
			chunk_first_codepoint = _mod_cache.chunk_codepoint_offset,
			chunk_first_byte = _mod_cache.chunk_byte_offset;
		auto target_pos_iter = _mod_cache.post_erase_boundaries.begin();
		// add this to a byte position in the old document after the erased region to convert it to the same position
		// in the new document; similarly, subtract this to convert from new document to old document. this may
		// overflow but will still work correctly
		std::size_t old_to_new_doc_byte = info.bytes_inserted.size() - info.bytes_erased.size();
		std::size_t
			// target position in the new document
			current_target_byte = *target_pos_iter + old_to_new_doc_byte,
			// codepoint index corresponding to the target position IN THE OLD DOCUMENT
			current_target_codepoint = _mod_cache.post_erase_codepoint_index;
		// this chunk is the last chunk that has been modified; end means it hasn't been computed yet
		tree_type::iterator end_chunk = _chunks.end();
		while (true) {
			// decode until the next target
			while (byte_iter.get_position() < current_target_byte) {
				codepoint cp = 0;
				if (!_encoding->next_codepoint(byte_iter, end_iter, cp)) {
					cp = 0;
				}
				++current_codepoint;
				// also keep track of lines and split decoded text into chunks
				linebreaks.put(cp);
				std::size_t num_chunk_codepoints = current_codepoint - chunk_first_codepoint;
				if (num_chunk_codepoints >= maximum_codepoints_per_chunk) {
					// split chunk
					std::size_t current_byte = byte_iter.get_position();
					chunks.emplace_back(current_byte - chunk_first_byte, num_chunk_codepoints);
					// update chunk start
					chunk_first_codepoint = current_codepoint;
					chunk_first_byte = current_byte;
				}
			}
			if (byte_iter.get_position() == current_target_byte) {
				// codepoint boundary found; stop
				break;
			}
			// find the next target byte
			if (target_pos_iter != _mod_cache.post_erase_boundaries.end()) {
				++target_pos_iter;
				if (target_pos_iter == _mod_cache.post_erase_boundaries.end()) {
					// we've just run out of fast boundaries; find the chunk the next byte is in instead
					// adjust byte position from new-doc to old-doc
					std::size_t byte_position = byte_iter.get_position();
					std::size_t adjusted_byte_position = byte_position - old_to_new_doc_byte;
					std::size_t byte_offset = adjusted_byte_position;
					// here if somehow we're exactly at a boundary, we treat it as being in the previous chunk and
					// the logic flows to the next iteration and finishes normally
					// additionally we don't need to deal with end iterators this way
					_byte_pos_converter<std::less_equal> conv;
					end_chunk = _chunks.find(conv, byte_offset);
					// this may over/underflow, but will work correctly nevertheless
					current_target_byte = (byte_position - byte_offset) + end_chunk->num_bytes;
					current_target_codepoint = conv.total_codepoints + end_chunk->num_codepoints;
				} else {
					// target next fast boundary
					current_target_byte = *target_pos_iter + old_to_new_doc_byte;
					++current_target_codepoint;
				}
			} else {
				++end_chunk;
				current_target_byte += end_chunk->num_bytes;
				current_target_codepoint += end_chunk->num_codepoints;
			}
		}
		// register the last remaining chunk
		{
			std::size_t
				remaining_bytes = byte_iter.get_position() - chunk_first_byte,
				remaining_codepoints = current_codepoint - chunk_first_codepoint;
			std::size_t old_chunk_bytes = 0, old_chunk_codepoints = 0;

			if (end_chunk == _chunks.end()) { // finished before reaching a old codepoint barrier
				// find the chunk that `current_target_byte - old_to_new_doc_byte` is in
				std::size_t byte_position = current_target_byte - old_to_new_doc_byte;
				_byte_pos_converter<std::less_equal> conv;
				end_chunk = _chunks.find(conv, byte_position);

				// if the document is empty, end_chunk could still be end
				if (end_chunk != _chunks.end()) {
					old_chunk_bytes = end_chunk->num_bytes - byte_position;
					old_chunk_codepoints =
						conv.total_codepoints + end_chunk->num_codepoints - current_target_codepoint;
				}
			}

			std::size_t total_codepoints = remaining_codepoints + old_chunk_codepoints;
			if (total_codepoints > maximum_codepoints_per_chunk) {
				chunks.emplace_back(remaining_bytes, remaining_codepoints);
				chunks.emplace_back(old_chunk_bytes, old_chunk_codepoints);
			} else if (total_codepoints > 0) {
				chunks.emplace_back(remaining_bytes + old_chunk_bytes, total_codepoints);
			}
		}
		// finish collecting lines
		linebreaks.finish();
		// decoding is done

		linebreak_registry::line_column_info
			start_info = _linebreaks.get_line_and_column_of_codepoint(_mod_cache.start_decoding_codepoint),
			end_info = _linebreaks.get_line_and_column_of_codepoint(current_target_codepoint);
		modification_decoded.invoke_noret(
			start_info, end_info, _mod_cache.start_decoding_codepoint, current_target_codepoint, info
		);

		// update line breaks
		_linebreaks.erase_codepoints(
			start_info.line_iterator, start_info.position_in_line, end_info.line_iterator, end_info.position_in_line
		);
		_linebreaks.insert_codepoints(_mod_cache.start_decoding_codepoint, lines);
		// update codepoint boundaries
		if (end_chunk != _chunks.end()) {
			++end_chunk;
		}
		_chunks.erase(_mod_cache.start_decoding_chunk, end_chunk);
		for (chunk_data chk : chunks) {
			_chunks.emplace_before(end_chunk, chk);
		}

		end_modification.invoke_noret(_mod_cache.start_decoding_codepoint, current_codepoint, info);
	}
}
