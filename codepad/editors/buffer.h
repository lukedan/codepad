// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Structs used to store the contents of a file.

#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>

#include "../core/bst.h"
#include "../core/profiling.h"
#include "../ui/element.h"
#include "../os/filesystem.h"

namespace codepad::editors {
	class buffer_manager;

	/// A \p std::basic_string whose elements are of type \p std::byte. This differs from \ref byte_array in that
	/// this may contain optimizations designed for strings, e.g., short string optimization.
	using byte_string = std::basic_string<std::byte>;
	/// A \p std::vector whose elements are of type \p std::byte.
	using byte_array = std::vector<std::byte>;

	/// Indicates the specific type of a \ref edit.
	enum class edit_type : unsigned char {
		normal, ///< A normal edit made by the user through an editor.
		undo, ///< This edit is made to revert a previous edit.
		redo, ///< This edit is made to restore a previous edit.
		external ///< This edit is made externally.
	};

	/// Stores the contents of a file as binary data. The contents is split into chunks and stored in a binary tree.
	/// Each node contains one chunk and some additional data to help navigate to a certain position.
	class buffer {
		friend buffer_manager;
	public:
		/// The maximum number of bytes there can be in a single chunk.
		constexpr static size_t maximum_bytes_per_chunk = 4096;

		/// Stores the contents of a chunk.
		using chunk_data = byte_array;

		/// Stores additional data of a node in the tree.
		struct node_data {
		public:
			/// A node of the tree.
			using node = binary_tree_node<chunk_data, node_data>;

			size_t total_length = 0; ///< The total number of bytes in this subtree.

			using length_property = sum_synthesizer::compact_property<
				synthesization_helper::func_value_property<&chunk_data::size>,
				&node_data::total_length
			>; ///< Property used to obtain the total number of bytes in a subtree.

			/// Refreshes the data of the given node.
			inline static void synthesize(node &tn) {
				sum_synthesizer::synthesize<length_property>(tn);
			}
		};
		/// The type of a tree used to store the chunks.
		using tree_type = binary_tree<chunk_data, node_data>;
		/// The type of the tree's nodes.
		using node_type = tree_type::node;

		/// An iterator over the bytes in the buffer. Declared as a template class to avoid <tt>const_cast</tt>s.
		///
		/// \tparam TIt The type of the tree's iterators.
		/// \tparam SIt The type of the chunk's iterators.
		template <typename TIt, typename SIt> struct iterator_base {
			friend buffer;
		public:
			using value_type = std::byte; ///< Type of values pointed to by these iterators.
			using pointer = typename SIt::pointer; ///< Pointers to underlying elements.
			using reference = typename SIt::reference; ///< References to underlying elements.

			/// Default constructor.
			iterator_base() = default;
			/// Copy constructor for non-const iterators, and converting constructor for const iterators.
			iterator_base(const iterator_base<tree_type::iterator, chunk_data::iterator> &it) :
				_it(it._it), _s(it._s), _chunkpos(it._chunkpos) {
			}

			/// Prefix increment.
			iterator_base &operator++() {
				if (++_s == _it->end()) {
					_chunkpos += _it->size();
					++_it;
					_s = (_it != _it.get_container()->end() ? _it.get_value_rawmod().begin() : SIt());
				}
				return *this;
			}
			/// Postfix increment.
			const iterator_base operator++(int) {
				iterator_base ov = *this;
				++*this;
				return ov;
			}
			/// Prefix decrement.
			iterator_base &operator--() {
				if (_it == _it.get_container()->end() || _s == _it->begin()) {
					--_it;
					_chunkpos -= _it->size();
					_s = _it.get_value_rawmod().end();
				}
				--_s;
				return *this;
			}
			/// Postfix decrement.
			const iterator_base operator--(int) {
				iterator_base ov = *this;
				--*this;
				return ov;
			}

			/// Equality.
			friend bool operator==(const iterator_base &lhs, const iterator_base &rhs) {
				return lhs._it == rhs._it && lhs._s == rhs._s;
			}
			/// Inequality.
			friend bool operator!=(const iterator_base &lhs, const iterator_base &rhs) {
				return !(lhs == rhs);
			}

			/// Returns the position of the byte to which this iterator points.
			size_t get_position() const {
				if (_it != _it.get_container()->end()) {
					return _chunkpos + (_s - _it->begin());
				}
				return _chunkpos;
			}

			/// Returns the byte this iterator points to.
			reference operator*() const {
				return *_s;
			}
		protected:
			/// Constructs an iterator that points to the given position.
			iterator_base(const TIt &t, const SIt &s, size_t chkpos) : _it(t), _s(s), _chunkpos(chkpos) {
			}

			TIt _it; ///< The tree's iterator.
			SIt _s{}; ///< The chunk's iterator.
			size_t _chunkpos = 0; ///< The position of the first byte of \ref _it in the \ref buffer.
		};
		/// Iterator type.
		using iterator = iterator_base<tree_type::iterator, chunk_data::iterator>;
		/// Const iterator type.
		using const_iterator = iterator_base<tree_type::const_iterator, chunk_data::const_iterator>;

		/// The position information of a modification.
		///
		/// \sa modification
		struct modification_position {
			/// Default constructor.
			modification_position() = default;
			/// Initializes all field of this struct.
			modification_position(size_t p, size_t rem, size_t add) :
				position(p), removed_range(rem), added_range(add) {
			}

			size_t
				/// The position where the modification takes place. If multiple modifications are made simultaneously
				/// by multiple carets, this position is obtained after all previous modifications have completed.
				position = 0,
				removed_range = 0, ///< The length of the removed byte sequence.
				added_range = 0; ///< The length of the added byte sequence.
		};
		/// A single modification made to the buffer at a single location. A sequence of bytes (optionally empty)
		/// starting from a certain position is removed, then another sequence of bytes (also optionally empty) is
		/// inserted at the same position.
		///
		/// \todo Maybe replace the bools with bit fields when C++20 drops?
		struct modification {
			/// Returns a \ref modification_position containing position information about this modification.
			modification_position get_position_info() const {
				return modification_position(position, removed_content.size(), added_content.size());
			}

			byte_string
				removed_content, ///< Bytes removed by this modification.
				added_content; ///< Bytes inserted by this modification.
			/// The position where this modification took place, after all previous modifications have been made.
			size_t position = 0;
		};
		/// A list of modifications made by multiple carets at the same time.
		using edit = std::vector<modification>;
		/// A list of positions of an \ref edit.
		using edit_positions = std::vector<modification_position>;
		/// For an ordered sequence of positions in a \ref buffer, this struct can be used to adjust them after an
		/// \ref edit has been made to ensure they're still on similar positions.
		struct position_patcher {
		public:
			/// Indicates how the position should be adjusted if it lies in a removed region.
			enum class strategy {
				front, ///< Moves the position to the front of the removed region.
				back, ///< Moves the position to the back of the added region.
				try_keep ///< Tries to keep the position stationary.
			};

			/// Initializes this struct with the associated \ref edit_positions.
			position_patcher(const edit_positions &pos) : _pos(pos) {
				reset();
			}

			/// Resets this patcher so that a new series of patches can be made.
			void reset() {
				_next = _pos.begin();
				_diff = 0;
			}

			/// Returns the patched position. Modifications with no removed content receive special treatment: a
			/// position is patched if it lies exactly at the position.
			template <strategy Strat> size_t patch(size_t pos) {
				pos += _diff;
				while (_next != _pos.end() && pos >= _next->position + std::max<size_t>(_next->removed_range, 1)) {
					size_t ndiff = _next->added_range - _next->removed_range;
					pos += ndiff;
					_diff += ndiff;
					++_next;
				}
				if (_next != _pos.end() && pos >= _next->position + std::min<size_t>(_next->removed_range, 1)) {
					if constexpr (Strat == strategy::front) {
						pos = _next->position;
					} else if constexpr (Strat == strategy::back) {
						pos = _next->position + _next->added_range;
					} else if constexpr (Strat == strategy::try_keep) {
						pos = std::min(pos, _next->position + _next->added_range);
					}
				}
				return pos;
			}
		protected:
			edit_positions::const_iterator _next; ///< Next modification to check.
			const edit_positions &_pos; ///< Position info of the \ref edit.
			/// The difference between old and new positions so far. It may overflow but will still work as intended.
			size_t _diff = 0;
		};

		/// Information about the starting of an edit to a \ref buffer.
		struct begin_edit_info {
			/// Default constructor.
			begin_edit_info() = default;
			/// Initializes all fields of this struct.
			begin_edit_info(edit_type t, ui::element *src) : type(t), source_element(src) {
			}

			const edit_type type = edit_type::normal; ///< The type of this edit.
			ui::element *const source_element = nullptr; ///< The source that made this edit.
		};
		/// Information about an edit to a \ref buffer.
		struct end_edit_info {
			/// Initializes all fields of this struct.
			end_edit_info(edit_type t, ui::element *source, const edit &edt, edit_positions pos) :
				positions(std::move(pos)), type(t), source_element(source), contents(edt) {
			}

			/// The positions of this edit. Unlike \ref contents, this is guaranteed to be accurate.
			const edit_positions positions;
			const edit_type type = edit_type::normal; ///< The type of this edit.
			ui::element *const source_element = nullptr; ///< The source that made this edit.
			/// The contents of this edit. Note that this may not correspond to the actual modification made to the
			/// \ref buffer, depending on \ref type.
			///
			/// \sa buffer::modifier
			const edit &contents;
		};
		/// Used to modify a \ref buffer. Users should call \ref begin() to start editing, make modifications with
		/// increasing starting positions, and then, after having finished all modifications, call \ref end() or
		/// \ref end_custom() to finish editing.
		struct modifier {
		public:
			/// Starts editing the given \ref buffer, with the given source and \ref edit_type. Invokes
			/// \ref buffer::begin_edit.
			void begin(buffer &buf, ui::element *src, edit_type type = edit_type::normal) {
				_buf = &buf;
				_src = src;
				_type = type;
				_buf->begin_edit.invoke_noret(_type, _src);
			}

			/// Appends accumulated modifications to the \ref buffer's history, and invokes \ref buffer::end_edit.
			/// Normally this should be used when \ref edit_type::normal is given to \ref begin().
			void end() {
				_buf->_append_edit(std::move(_edt));
				_buf->end_edit.invoke_noret(_type, _src, _buf->history()[_buf->current_edit() - 1], std::move(_pos));
				_buf = nullptr;
			}
			/// Finishes the edit with the specified edit contents by invoking \ref buffer::end_edit only, normally
			/// used for redoing or undoing.
			void end_custom(const edit &edt) {
				_buf->end_edit.invoke_noret(_type, _src, edt, std::move(_pos));
				_buf = nullptr;
			}

			/// Erases a sequence of bytes starting from \p pos with length \p eraselen, and inserts \p insert at
			/// \p pos. The position \p pos is supposed to be the value after all previous modifications have been
			/// made.
			void modify_nofixup(size_t pos, size_t eraselen, byte_string insert) {
				modification mod;
				mod.position = pos;
				if (eraselen > 0) {
					const_iterator posit = _buf->at(pos), endit = _buf->at(pos + eraselen);
					mod.removed_content = _buf->get_clip(posit, endit);
					_buf->_erase(posit, endit);
				}
				if (!insert.empty()) {
					_buf->_insert(_buf->at(pos), insert.begin(), insert.end());
					mod.added_content = std::move(insert);
				}
				_diff += mod.added_content.size() - mod.removed_content.size();
				_pos.emplace_back(mod.position, mod.removed_content.size(), mod.added_content.size());
				_edt.emplace_back(std::move(mod));
			}
			/// Similar to \ref modify_nofixup, but \p pos is obtained before modifications have been made, and is
			/// automatically adjusted with \ref _diff.
			void modify(size_t pos, size_t eraselen, byte_string insert) {
				pos += _diff;
				modify_nofixup(pos, eraselen, std::move(insert));
			}

			/// Reverts a modification made previously. This operation is not recorded, and is intended to be used
			/// solely throughout the duration of an edit.
			void undo(const modification &mod) {
				size_t pos = mod.position + _diff;
				if (!mod.added_content.empty()) {
					_buf->_erase(_buf->at(pos), _buf->at(pos + mod.added_content.size()));
				}
				if (!mod.removed_content.empty()) {
					_buf->_insert(_buf->at(pos), mod.removed_content.begin(), mod.removed_content.end());
				}
				_diff += mod.removed_content.size() - mod.added_content.size();
				_pos.emplace_back(pos, mod.added_content.size(), mod.removed_content.size());
			}
			/// Restores a previously reverted modification. This operation is not recorded, and is intended to be
			/// used solely throughout the duration of an edit.
			void redo(const modification &mod) {
				// the modification already stores adjusted positions
				if (!mod.removed_content.empty()) {
					_buf->_erase(_buf->at(mod.position), _buf->at(mod.position + mod.removed_content.size()));
				}
				if (!mod.added_content.empty()) {
					_buf->_insert(_buf->at(mod.position), mod.added_content.begin(), mod.added_content.end());
				}
				_diff += mod.added_content.size() - mod.removed_content.size();
				_pos.emplace_back(mod.position, mod.removed_content.size(), mod.added_content.size());
			}
		protected:
			edit_positions _pos; ///< Records the actual positions of each \ref modification.
			edit _edt; ///< Modifications made so far.
			buffer *_buf = nullptr; ///< The buffer being modified.
			ui::element *_src = nullptr; ///< The source of this edit.
			edit_type _type = edit_type::normal; ///< The type of this edit.
			/// Used to adjust positions obtained before modifications are made. Note that although its value may
			/// overflow, it'll still work as intended.
			size_t _diff = 0;
		};
		/// A wrapper for \ref modifier that automatically calls \ref modifier::begin() upon construction and
		/// \ref modifier::end() upon destruction. The edit type can only be \ref edit_type::normal.
		struct scoped_normal_modifier {
		public:
			/// Calls \ref modifier::begin() to start editing the buffer.
			scoped_normal_modifier(buffer &buf, ui::element *src) {
				_mod.begin(buf, src, edit_type::normal);
			}
			/// Calls \ref modifier::end().
			~scoped_normal_modifier() {
				_mod.end();
			}

			/// Calls \ref modifier::modify().
			void modify(size_t pos, size_t eraselen, byte_string insert) {
				_mod.modify(pos, eraselen, std::move(insert));
			}
			/// Calls \ref modifier::modify_nofixup().
			void modify_nofixup(size_t pos, size_t eraselen, byte_string insert) {
				_mod.modify_nofixup(pos, eraselen, std::move(insert));
			}
		protected:
			modifier _mod; ///< The underlying \ref modifier.
		};

		/// Constructs this \ref buffer with the given buffer index.
		explicit buffer(size_t id) : _fileid(std::in_place_type<size_t>, id) {
		}
		/// Constructs this \ref buffer with the given file name, and loads that file's contents.
		explicit buffer(const std::filesystem::path &filename) :
			_fileid(std::in_place_type<std::filesystem::path>, filename) {

			performance_monitor mon(CP_STRLIT("load file"), performance_monitor::log_condition::always);

			// read version
			os::file f(filename, os::access_rights::read, os::open_mode::open);
			if (f.valid()) {
				std::vector<chunk_data> chunks;
				while (true) {
					auto &chk = chunks.emplace_back();
					chk.resize(maximum_bytes_per_chunk);
					os::file::pos_type res = f.read(maximum_bytes_per_chunk, chk.data());
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
				if (auto tail = static_cast<size_t>(fin.gcount()); tail > 0) {
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
		~buffer();

		/// Returns an iterator to the first byte of the buffer.
		iterator begin() {
			auto it = _t.begin();
			return iterator(it, it == _t.end() ? chunk_data::iterator() : it.get_value_rawmod().begin(), 0);
		}
		/// Const version of begin().
		const_iterator begin() const {
			auto it = _t.begin();
			return const_iterator(it, it == _t.end() ? chunk_data::const_iterator() : it->begin(), 0);
		}
		/// Returns an iterator past the last byte of the buffer.
		iterator end() {
			return iterator(_t.end(), chunk_data::iterator(), length());
		}
		/// Const version of end().
		const_iterator end() const {
			return const_iterator(_t.end(), chunk_data::const_iterator(), length());
		}

		/// Returns an iterator to the first chunk of the buffer.
		tree_type::const_iterator node_begin() const {
			return _t.begin();
		}
		/// Returns an iterator past the last chunk of the buffer.
		tree_type::const_iterator node_end() const {
			return _t.end();
		}

		/// Returns an iterator to the byte at the given position of the buffer.
		iterator at(size_t bytepos) {
			size_t chkpos = bytepos;
			auto t = _t.find_custom(_byte_index_finder(), chkpos);
			if (t == _t.end()) {
				return iterator(t, chunk_data::iterator(), length());
			}
			return iterator(t, t.get_value_rawmod().begin() + chkpos, bytepos - chkpos);
		}
		/// Const version of at().
		const_iterator at(size_t bytepos) const {
			size_t chkpos = bytepos;
			auto t = _t.find_custom(_byte_index_finder(), chkpos);
			if (t == _t.end()) {
				return const_iterator(t, chunk_data::const_iterator(), length());
			}
			return const_iterator(t, t->begin() + bytepos, bytepos - chkpos);
		}

		/// Given a \ref const_iterator, returns the position of the byte it points to.
		size_t get_position(const const_iterator &it) const {
			size_t res = 0;
			sum_synthesizer::sum_before<node_data::length_property>(it._it, res);
			if (it._it != _t.end()) {
				res += it._s - it._it->begin();
			}
			return res;
		}

		/// Returns a clip of the buffer.
		byte_string get_clip(const const_iterator &beg, const const_iterator &end) const {
			if (beg._it == _t.end()) {
				return byte_string();
			}
			if (beg._it == end._it) { // in the same chunk
				return byte_string(beg._s, end._s);
			}
			byte_string result(beg._s, beg._it->end()); // insert the part in the first chunk
			tree_type::const_iterator it = beg._it;
			for (++it; it != end._it; ++it) { // insert full chunks
				result.append(it->begin(), it->end());
			}
			if (end._it != _t.end()) {
				result.append(end._it->begin(), end._s); // insert the part in the last chunk
			}
			return result;
		}

		/// Returns whether this buffer has been modified.
		bool can_undo() const {
			return _curedit > 0;
		}
		/// Returns whether there are undone edits after which no new edits have been made.
		bool can_redo() const {
			return _curedit < _history.size();
		}
		/// Reverts a previously made edit.
		void undo(ui::element *source) {
			modifier mod;
			mod.begin(*this, source, edit_type::undo);
			--_curedit;
			for (const modification &cmod : _history[_curedit]) {
				mod.undo(cmod);
			}
			mod.end_custom(_history[_curedit]);
		}
		/// Restores a previously undone edit.
		void redo(ui::element *source) {
			modifier mod;
			mod.begin(*this, source, edit_type::redo);
			for (const modification &cmod : _history[_curedit]) {
				mod.redo(cmod);
			}
			++_curedit;
			mod.end_custom(_history[_curedit - 1]);
		}

		/// Returns the recorded list of edits made to this buffer.
		const std::vector<edit> &history() const {
			return _history;
		}
		/// Returns the index after the last edit made to this buffer, potentially after redoing or undoing.
		size_t current_edit() const {
			return _curedit;
		}
		/// Returns the number of bytes in this buffer.
		size_t length() const {
			node_type *n = _t.root();
			return n == nullptr ? 0 : n->synth_data.total_length;
		}

		/// Clears the contents of this buffer.
		void clear() {
			_t.clear();
		}

		info_event<begin_edit_info> begin_edit; ///< Invoked when this \ref buffer is about to be modified.
		info_event<end_edit_info> end_edit; ///< Invoked when this \ref buffer has been modified.
	protected:
		/// Used to find the chunk in which the byte at the given index lies.
		using _byte_index_finder = sum_synthesizer::index_finder<node_data::length_property>;

		/// Erases a subsequence from the buffer.
		void _erase(const_iterator beg, const_iterator end) {
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
		/// Inserts an array of bytes at the given position.
		template <typename It1, typename It2> void _insert(
			const_iterator pos, const It1 &beg, const It2 &end
		) {
			if (beg == end) {
				return;
			}
			tree_type::const_iterator insit = pos._it, updit = insit;
			chunk_data afterstr, *curstr;
			std::vector<chunk_data> strs; // the buffer for (not all) inserted bytes
			if (pos == begin()) { // insert at the very beginning, no need to split or update
				updit = _t.end();
				chunk_data st;
				st.reserve(maximum_bytes_per_chunk);
				strs.push_back(std::move(st));
				curstr = &strs.back();
			} else if (pos._it == _t.end() || pos._s == pos._it->begin()) {
				// insert at the beginning of a chunk, which is not the first chunk
				--updit;
				curstr = &updit.get_value_rawmod();
			} else { // insert at the middle of a chunk
				// save the second part & truncate the chunk
				afterstr = chunk_data(pos._s, pos._it->end());
				pos._it.get_value_rawmod().erase(pos._s, pos._it->end());
				++insit;
				curstr = &updit.get_value_rawmod();
			}
			for (auto it = beg; it != end; ++it) { // insert codepoints
				if (curstr->size() == maximum_bytes_per_chunk) { // curstr would be too long, add a new chunk
					strs.emplace_back();
					curstr = &strs.back();
					curstr->reserve(maximum_bytes_per_chunk);
				}
				curstr->emplace_back(*it); // append byte to curstr
			}
			if (!afterstr.empty()) { // at the middle of a chunk, add the second part to the strings
				if (curstr->size() + afterstr.size() <= maximum_bytes_per_chunk) {
					curstr->insert(curstr->end(), afterstr.begin(), afterstr.end());
				} else {
					strs.push_back(std::move(afterstr)); // curstr is not changed
				}
			}
			_t.refresh_synthesized_result(updit.get_node()); // this function checks if updit is end
			_t.insert_range_before_move(insit, strs.begin(), strs.end()); // insert the strings
			// try to merge small nodes
			_try_merge_small_nodes(insit);
		}

		/// Merges a node with one or more of its neighboring nodes if their total length are smaller than the
		/// maximum value. Note that this function does not ensure the validity of any iterator after this operation.
		///
		/// \todo Find a better merging strategy.
		void _try_merge_small_nodes(const tree_type::const_iterator &it) {
			if (it == _t.end()) {
				return;
			}
			size_t nvl = it->size();
			if (nvl * 2 > maximum_bytes_per_chunk) {
				return;
			}
			if (it != _t.begin()) {
				tree_type::const_iterator prev = it;
				--prev;
				if (prev->size() + nvl < maximum_bytes_per_chunk) {
					_t.get_modifier_for(prev.get_node())->insert(prev->end(), it->begin(), it->end());
					_t.erase(it);
					return;
				}
			}
			tree_type::const_iterator next = it;
			++next;
			if (next != _t.end() && next->size() + nvl < maximum_bytes_per_chunk) {
				_t.get_modifier_for(it.get_node())->insert(it->end(), next->begin(), next->end());
				_t.erase(next);
				return;
			}
		}
		/// Adds an \ref edit to the history of this buffer.
		void _append_edit(edit edt) {
			if (_curedit < _history.size()) {
				_history.erase(_history.begin() + _curedit, _history.end());
			}
			_history.emplace_back(std::move(edt));
			++_curedit;
		}

		tree_type _t; ///< The underlying binary tree that stores all the chunks.
		std::vector<edit> _history; ///< Records undoable or redoable edits made to this \ref buffer.
		/// Used to identify this buffer. Also stores the path to the associated file, if one exists.
		std::variant<size_t, std::filesystem::path> _fileid;
		size_t _curedit = 0; ///< The index of the edit that's to be redone next should the need arise.
	};
}
