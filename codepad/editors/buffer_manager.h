// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Declaration and implementation of manager class for \ref codepad::editors::buffer.

#include <stack>

#include "../os/filesystem.h"
#include "buffer.h"
#include "code/interpretation.h"

namespace codepad::editors {
	/// Used to identify a \ref buffer in certain events.
	struct buffer_info {
		/// Assigns the specified \ref buffer to \ref buf.
		explicit buffer_info(buffer &b) : buf(b) {
		}

		buffer &buf; ///< Reference to the \ref buffer.
	};
	/// Manager of all \ref buffer "buffers". All \ref buffer instances should be created with the instance
	/// returned by \ref buffer_manager::get.
	///
	/// \todo Per-buffer tag and per-interpretation tag?
	class buffer_manager {
		friend buffer;
	public:
		/// Returns a \p std::shared_ptr<buffer> to the file specified by the given file name. If the file has not
		/// been opened, this function opens the file; otherwise, it returns the pointer returned by previous calls
		/// to this function. The file must exist.
		std::shared_ptr<buffer> open_file(std::filesystem::path path) {
			// check for existing file
			path = std::filesystem::canonical(path);
			auto ins = _file_map.try_emplace(path);
			if (!ins.second) {
				auto ptr = ins.first->second.buf.lock();
				assert_true_logical(ptr != nullptr, "context destruction not notified");
				return ptr;
			}
			// create new one
			auto res = std::make_shared<buffer>(path);
			ins.first->second.buf = res;
			/*res->_tags.resize(_tag_alloc_max);*/ // allocate space for tags
			buffer_created.invoke_noret(*res);
			return res;
		}
		/// Creates a new file not yet associated with a path, identified by a \p std::size_t.
		std::shared_ptr<buffer> new_file() {
			std::shared_ptr<buffer> ctx;
			if (_noname_alloc.empty()) {
				ctx = std::make_shared<buffer>(_noname_map.size());
				_noname_map.emplace_back(ctx);
			} else {
				ctx = std::make_shared<buffer>(_noname_alloc.top());
				_noname_map[_noname_alloc.top()] = _buffer_data(ctx);
				_noname_alloc.pop();
			}
			// ctx->_tags.resize(_tag_alloc_max); // allocate space for tags
			buffer_created.invoke_noret(*ctx);
			return ctx;
		}

		/// Returns the \ref code::interpretation of the given \ref buffer corresponding to the given
		/// \ref code::buffer_encoding, creating a new one if none is found.
		///
		/// \todo Asynchronous loading.
		std::shared_ptr<code::interpretation> open_interpretation(
			const std::shared_ptr<buffer> &buf, const code::buffer_encoding &encoding
		) {
			_buffer_data &data = _get_data_of(*buf);
			auto it = data.interpretations.find(encoding.get_name());
			if (it != data.interpretations.end()) {
				auto ptr = it->second.lock();
				if (ptr) {
					return ptr;
				}
			} else {
				it = data.interpretations.try_emplace(str_t(encoding.get_name())).first;
			}
			auto ptr = std::make_shared<code::interpretation>(buf, encoding);
			it->second = ptr;
			return ptr;
		}
		/// Finds the \ref code::buffer_encoding with the specified name, then calls
		/// \ref open_interpretation(const std::shared_ptr<buffer>&, const code::buffer_encoding&) and returns its
		/// result. If no such encoding is found, an empty \p std::shared_ptr is returned.
		std::shared_ptr<code::interpretation> open_interpretation(
			const std::shared_ptr<buffer> &buf, const str_t &encname
		) {
			const code::buffer_encoding *enc = code::encoding_manager::get().get_encoding(encname);
			if (enc == nullptr) {
				return std::shared_ptr<code::interpretation>();
			}
			return open_interpretation(buf, *enc);
		}

		// TODO
		/*
		/// Allocates a tag slot and returns the index.
		///
		/// \remark This function tries to enlarge \ref buffer::_tags of all open buffers if no previously
		///         deallocated slot is available.
		std::size_t allocate_tag() {
			std::size_t res;
			if (_tag_alloc.empty()) {
				res = _tag_alloc_max++;
				for_each_buffer([this](std::shared_ptr<document> &doc) {
					doc->_tags.resize(_tag_alloc_max);
					});
			} else {
				res = _tag_alloc.top();
				_tag_alloc.pop();
			}
			return res;
		}
		/// Deallocates a tag slot, and clears the corresponding entries in \ref document::_tags for all open
		/// documents.
		void deallocate_tag(std::size_t tag) {
			for_each_buffer([tag](std::shared_ptr<document> &doc) {
				doc->_tags[tag].reset();
				});
			_tag_alloc.emplace(tag);
		}
		*/


		/// Iterates through all open buffers.
		///
		/// \param cb A callback function invoked for each open buffer.
		template <typename Cb> void for_each_buffer(Cb &&cb) {
			for (auto &pair : _file_map) {
				auto doc = pair.second.buf.lock();
				// should not be nullptr since all disposed documents are removed from the map
				assert_true_logical(doc != nullptr, "corrupted document registry");
				cb(doc);
			}
			for (auto &wptr : _noname_map) {
				auto doc = wptr.buf.lock();
				if (doc) {
					cb(doc);
				}
			}
		}

		info_event<buffer_info>
			/// Invoked when a buffer has been created. Components and plugins that make use of per-buffer tags
			/// may have to handle this (and only this, as the tags are automatically disposed) event to initialize
			/// them.
			buffer_created,
			/// Invoked when a buffer is about to be disposed. Handlers should be careful not to modify the buffer.
			buffer_disposing;

		/// Returns the global \ref buffer_manager.
		static buffer_manager &get();
	protected:
		/// Stores a \p std::weak_ptr to a \ref buffer, and pointers to all its
		/// \ref code::interpretation "interpretations".
		struct _buffer_data {
			/// Default constructor.
			_buffer_data() = default;
			/// Initializes this struct with the given \ref buffer.
			explicit _buffer_data(std::weak_ptr<buffer> b) : buf(std::move(b)) {
			}
			/// No copy construction.
			_buffer_data(const _buffer_data&) = delete;
			/// Move constructor.
			_buffer_data(_buffer_data &&src) :
				buf(std::move(src.buf)), interpretations(std::move(src.interpretations)) {
			}
			/// No copy assignment.
			_buffer_data &operator=(const _buffer_data&) = delete;
			/// Move assignment.
			_buffer_data &operator=(_buffer_data &&src) {
				std::swap(buf, src.buf);
				std::swap(interpretations, src.interpretations);
				return *this;
			}

			std::weak_ptr<buffer> buf; ///< Pointer to the buffer.
			/// All \ref code::interpretation "interpretations" of this \ref buffer.
			std::map<str_t, std::weak_ptr<code::interpretation>, std::less<>> interpretations;
		};

		/// Stores all \p buffer "buffers" that correspond to files and their <tt>std::filesystem::path</tt>s.
		std::map<std::filesystem::path, _buffer_data> _file_map;
		/// Stores all \p buffer "buffers" that don't correspond to files.
		std::vector<_buffer_data> _noname_map;
		/// Stores indices of disposed buffers in \ref _noname_map for more efficient allocation of indices.
		std::stack<std::size_t> _noname_alloc;

		std::stack<std::size_t> _tag_alloc; ///< Stores deallocated tag indices.
		std::size_t _tag_alloc_max = 0; ///< Stores the next index to allocate for a tag if \ref _tag_alloc is empty.

		/// Returns the \ref _buffer_data associated with the given \ref buffer.
		_buffer_data &_get_data_of(const buffer &buf) {
			if (std::holds_alternative<std::size_t>(buf._fileid)) {
				return _noname_map[std::get<std::size_t>(buf._fileid)];
			}
			auto found = _file_map.find(std::get<std::filesystem::path>(buf._fileid));
			assert_true_logical(found != _file_map.end(), "getting data of invalid buffer");
			return found->second;
		}

		/// Called when a buffer is being disposed, to remove the corresponding entry in \ref _file_map or add its
		/// index to \ref _noname_map.
		void _on_deleting_buffer(buffer &buf) {
			buffer_disposing.invoke_noret(buf);
			if (std::holds_alternative<std::size_t>(buf._fileid)) {
				_noname_alloc.emplace(std::get<std::size_t>(buf._fileid));
			} else {
				assert_true_logical(
					_file_map.erase(std::get<std::filesystem::path>(buf._fileid)) == 1,
					"deleting invalid buffer"
				);
			}
		}
		/// Called when a newly-created buffer is being saved to move its entry into \ref _file_map. The file
		/// must have already been saved (i.e., the file must exist).
		///
		/// \todo Try to merge two buffers when one is being saved to an already opened buffer.
		void _on_saved_new_buffer(std::size_t id, std::filesystem::path f) {
			f = std::filesystem::canonical(f);
			_noname_alloc.emplace(id);
			_buffer_data &target = _noname_map[id];
			auto insres = _file_map.try_emplace(f, std::move(target));
			if (!insres.second) {
				// TODO merge buffers?
			} else {
				target = _buffer_data(); // keep _buffer_data::buf valid
			}
		}
	};
}
