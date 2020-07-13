// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/buffer.h"

/// \file
/// Implementation of certain functions related to the \ref codepad::editors::buffer class.

#include "codepad/editors/manager.h"

namespace codepad::editors {
	buffer::buffer(const std::filesystem::path &filename, buffer_manager *man) :
		_fileid(std::in_place_type<std::filesystem::path>, filename), _bufman(man) {

		performance_monitor mon(u8"load file", performance_monitor::log_condition::always);

		logger::get().log_debug(CP_HERE) << "opening file " << filename;

		// read version
		if (auto f = os::file::open(filename, os::access_rights::read, os::open_mode::open)) {
			std::vector<chunk_data> chunks;
			while (true) {
				auto &chk = chunks.emplace_back();
				chk.resize(maximum_bytes_per_chunk);
				auto res = static_cast<std::size_t>(f->read(maximum_bytes_per_chunk, chk.data()));
				if (res < maximum_bytes_per_chunk) {
					if (res > 0) {
						chk.resize(res);
					} else {
						chunks.pop_back();
					}
					break;
				}
			}
			_t.insert_range_before_move(_t.end(), chunks.begin(), chunks.end());
		} // TODO failed to open file

		/*// STL version
		std::ifstream fin(filename, std::ios::binary);
		if (fin) {
			std::vector<chunk_data> chunks;
			while (!fin.eof()) {
				auto &chk = chunks.emplace_back();
				chk.resize(maximum_bytes_per_chunk);
				fin.read(reinterpret_cast<char*>(chk.data()), maximum_bytes_per_chunk);
			}
			if (auto tail = static_cast<std::size_t>(fin.gcount()); tail > 0) {
				chunks.back().resize(tail);
			} else {
				chunks.pop_back();
			}
			if (!fin.bad()) {
				_t.insert_range_before_move(_t.end(), chunks.begin(), chunks.end());
			} // TODO failed to read file
		} // TODO failed to open file*/

		/*
		// memory-mapped version
		// about 30% faster than STL version
		os::file f(filename, os::access_rights::read, os::open_mode::open);
		if (f.valid()) {
			os::file_mapping mp(f, os::access_rights::read);
			if (mp.valid()) {
				const std::byte
					*ptr = static_cast<const std::byte*>(mp.get_mapped_pointer()),
					*end = ptr + f.get_size();
				std::vector<chunk_data> chunks;
				for (
					const std::byte *next = ptr + maximum_bytes_per_chunk;
					next < end;
					ptr = next, next += maximum_bytes_per_chunk
					) {
					chunks.emplace_back(ptr, next);
				}
				chunks.emplace_back(ptr, end);
				_t.insert_range_before_move(_t.end(), chunks.begin(), chunks.end());
			}
			// TODO failed to map file
		}
		// TODO failed to open file
		*/
	}

	buffer::~buffer() {
		if (_bufman) {
			_bufman->_on_deleting_buffer(*this);
		}
	}

	void buffer::_erase(const_iterator beg, const_iterator end) {
		if (beg._it == _t.end()) {
			return;
		}
		if (beg._it == end._it) { // same chunk
			_t.get_modifier_for(beg._it.get_node())->erase(beg._s, end._s);
			_try_merge_small_nodes(beg._it);
			return;
		}
		// erase full chunks
		if (beg._s == beg._it->begin()) { // the first chunk is fully deleted
			_t.erase(beg._it, end._it);
		} else {
			_t.erase(beg._it.get_node()->next(), end._it.get_node());
			// erase the part in the first chunk
			_t.get_modifier_for(beg._it.get_node())->erase(beg._s, beg._it->end());
		}
		if (end._it != _t.end()) {
			// erase the part in the last chunk
			_t.get_modifier_for(end._it.get_node())->erase(end._it->begin(), end._s);
			_try_merge_small_nodes(end._it);
		} else if (!_t.empty()) {
			_try_merge_small_nodes(--_t.end());
		}
	}
}
