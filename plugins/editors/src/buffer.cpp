// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/buffer.h"

/// \file
/// Implementation of certain functions related to the \ref codepad::editors::buffer class.

#include "codepad/editors/manager.h"

namespace codepad::editors {
	buffer::modifier::modifier(buffer &buf, ui::element *src) : _buf(buf), _src(src) {
	}

	void buffer::modifier::modify_nofixup(std::size_t pos, std::size_t eraselen, byte_string insert) {
		modification mod;
		_buf.begin_modify.construct_info_and_invoke(pos, eraselen, insert);
		mod.position = pos;
		if (eraselen > 0) {
			const_iterator posit = _buf.at(pos), endit = _buf.at(pos + eraselen);
			mod.removed_content = _buf.get_clip(posit, endit);
			_buf._erase(posit, endit);
		}
		if (!insert.empty()) {
			_buf._insert(_buf.at(pos), insert.begin(), insert.end());
			mod.added_content = std::move(insert);
		}
		_buf.end_modify.construct_info_and_invoke(pos, mod.removed_content, mod.added_content);
		_diff += mod.added_content.size() - mod.removed_content.size();
		_pos.emplace_back(mod.position, mod.removed_content.size(), mod.added_content.size());
		_edt.emplace_back(std::move(mod));
	}

	void buffer::modifier::_undo_modification(const modification &mod) {
		std::size_t pos = mod.position + _diff;
		_buf.begin_modify.construct_info_and_invoke(pos, mod.added_content.size(), mod.removed_content);
		if (!mod.added_content.empty()) {
			_buf._erase(_buf.at(pos), _buf.at(pos + mod.added_content.size()));
		}
		if (!mod.removed_content.empty()) {
			_buf._insert(_buf.at(pos), mod.removed_content.begin(), mod.removed_content.end());
		}
		_buf.end_modify.construct_info_and_invoke(pos, mod.added_content, mod.removed_content);
		_diff += mod.removed_content.size() - mod.added_content.size();
		_pos.emplace_back(pos, mod.added_content.size(), mod.removed_content.size());
	}

	void buffer::modifier::_redo_modification(const modification &mod) {
		// the modification already stores adjusted positions
		_buf.begin_modify.construct_info_and_invoke(mod.position, mod.removed_content.size(), mod.added_content);
		if (!mod.removed_content.empty()) {
			_buf._erase(_buf.at(mod.position), _buf.at(mod.position + mod.removed_content.size()));
		}
		if (!mod.added_content.empty()) {
			_buf._insert(_buf.at(mod.position), mod.added_content.begin(), mod.added_content.end());
		}
		_buf.end_modify.construct_info_and_invoke(mod.position, mod.removed_content, mod.added_content);
		_diff += mod.added_content.size() - mod.removed_content.size();
		_pos.emplace_back(mod.position, mod.removed_content.size(), mod.added_content.size());
	}


	buffer::buffer(const std::filesystem::path &filename, buffer_manager &man) :
		_fileid(std::in_place_type<std::filesystem::path>, filename), _buf_manager(man) {

		performance_monitor mon(u8"load file", performance_monitor::log_condition::always);

		logger::get().log_debug() << "opening file " << filename;

		// read version
		if (auto f = os::file::open(filename, os::access_rights::read, os::open_mode::open)) {
			std::vector<chunk_data> chunks;
			while (true) {
				auto &chk = chunks.emplace_back();
				chk.data.resize(maximum_bytes_per_chunk);
				if (auto res = f->read(maximum_bytes_per_chunk, chk.data.data())) {
					auto bytes_read = static_cast<std::size_t>(res.value());
					if (bytes_read < maximum_bytes_per_chunk) {
						if (bytes_read > 0) {
							chk.data.resize(bytes_read);
						} else {
							chunks.pop_back();
						}
						break;
					}
				} // TODO read() failed
			}
			// TODO build the tree directly
			for (chunk_data &cd : chunks) {
				_t.emplace_before(_t.end(), std::move(cd));
			}
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
		_buf_manager._on_deleting_buffer(*this);
	}

	buffer::const_iterator buffer::at(std::size_t bytepos) const {
		std::size_t chkpos = bytepos;
		auto t = _t.find(_byte_index_finder(), chkpos);
		if (t == _t.end()) {
			return const_iterator(t, byte_array::const_iterator(), length());
		}
		return const_iterator(t, t->data.begin() + chkpos, bytepos - chkpos);
	}

	byte_string buffer::get_clip(const const_iterator &beg, const const_iterator &end) const {
		if (beg._it == _t.end()) {
			return byte_string();
		}
		if (beg._it == end._it) { // in the same chunk
			return byte_string(beg._s, end._s);
		}
		byte_string result(beg._s, beg._it->data.end()); // insert the part in the first chunk
		tree_type::const_iterator it = beg._it;
		for (++it; it != end._it; ++it) { // insert full chunks
			result.append(it->data.begin(), it->data.end());
		}
		if (end._it != _t.end()) {
			result.append(end._it->data.begin(), end._s); // insert the part in the last chunk
		}
		return result;
	}

	void buffer::_erase(const_iterator beg, const_iterator end) {
		if (beg._it == _t.end()) {
			return;
		}
		if (beg._it == end._it) { // same chunk
			_t.get_modifier_for(beg._it.get_node())->data.erase(beg._s, end._s);
			_try_merge_small_nodes(beg._it);
			return;
		}
		// erase full chunks
		if (beg._s == beg._it->data.begin()) { // the first chunk is fully deleted
			_t.erase(beg._it, end._it);
		} else {
			tree_type::const_iterator erase_beg = beg._it;
			++erase_beg;
			_t.erase(erase_beg, end._it);
			// erase the part in the first chunk
			_t.get_modifier_for(beg._it.get_node())->data.erase(beg._s, beg._it->data.end());
		}
		if (end._it != _t.end()) {
			// erase the part in the last chunk
			_t.get_modifier_for(end._it.get_node())->data.erase(end._it->data.begin(), end._s);
			_try_merge_small_nodes(end._it);
		} else if (!_t.empty()) {
			_try_merge_small_nodes(--_t.end());
		}
	}

	void buffer::_try_merge_small_nodes(const tree_type::const_iterator &it) {
		if (it == _t.end()) {
			return;
		}
		std::size_t nvl = it->data.size();
		if (nvl * 2 > maximum_bytes_per_chunk) {
			return;
		}
		if (it != _t.begin()) {
			tree_type::const_iterator prev = it;
			--prev;
			if (prev->data.size() + nvl < maximum_bytes_per_chunk) {
				_t.get_modifier_for(prev.get_node())->data.insert(
					prev->data.end(), it->data.begin(), it->data.end()
				);
				_t.erase(it);
				return;
			}
		}
		tree_type::const_iterator next = it;
		++next;
		if (next != _t.end() && next->data.size() + nvl < maximum_bytes_per_chunk) {
			_t.get_modifier_for(it.get_node())->data.insert(
				it->data.end(), next->data.begin(), next->data.end()
			);
			_t.erase(next);
			return;
		}
	}
}
