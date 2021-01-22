// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Declaration and implementation of manager classes.

#include <stack>

#include <codepad/os/filesystem.h>

#include "buffer.h"
#include "code/interpretation.h"
#include "code/caret_set.h"
#include "code/interpretation.h"
#include "binary/contents_region.h"

namespace codepad::editors {
	/// Used to identify a \ref buffer in certain events.
	struct buffer_info {
		/// Initializes \ref buf.
		explicit buffer_info(buffer &b) : buf(b) {
		}

		buffer &buf; ///< Reference to the \ref buffer.
	};
	/// Used to identify a \ref code::interpretation in certain events.
	struct interpretation_info {
		/// Initializes \ref interp.
		explicit interpretation_info(code::interpretation &i) : interp(i) {
		}

		code::interpretation &interp; ///< The \ref code::interpretation.
	};
	/// Manager of all \ref buffer "buffers". All \ref buffer instances should be created with the instance
	/// returned by \ref buffer_manager::get.
	///
	/// \todo Per-buffer tag and per-interpretation tag?
	class buffer_manager {
		friend buffer;
	public:
		/// Token for a tag.
		template <typename T> struct tag_token {
			friend buffer_manager;
		public:
			/// Default constructor.
			tag_token() = default;

			/// Resets this token.
			void reset() {
				_index = std::numeric_limits<std::size_t>::max();
			}

			/// Retrieves the value of this tag associated with the given object.
			std::any &get_for(T &object) const {
				assert_true_usage(!empty(), "empty token");
				return object._tags[_index];
			}

			/// Returns whether this token is empty.
			[[nodiscard]] bool empty() const {
				return _index == std::numeric_limits<std::size_t>::max();
			}
		private:
			/// Constructor.
			explicit tag_token(std::size_t i) : _index(i) {
			}

			/// Index of the tag in \ref buffer::_tags.
			std::size_t _index = std::numeric_limits<std::size_t>::max();
		};
		using buffer_tag_token = tag_token<buffer>; ///< Tag token for buffers.
		using interpretation_tag_token = tag_token<code::interpretation>; ///< Tag token for interpretations.

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
			auto res = std::make_shared<buffer>(path, this);
			ins.first->second.buf = res;
			res->_tags.resize(_buffer_tag_alloc_max); // allocate space for tags
			buffer_created.invoke_noret(*res);
			return res;
		}
		/// Creates a new file not yet associated with a path, identified by a \p std::size_t.
		std::shared_ptr<buffer> new_file() {
			std::shared_ptr<buffer> buf;
			if (_noname_alloc.empty()) {
				buf = std::make_shared<buffer>(_noname_map.size(), this);
				_noname_map.emplace_back(buf);
			} else {
				buf = std::make_shared<buffer>(_noname_alloc.top(), this);
				_noname_map[_noname_alloc.top()] = _buffer_data(buf);
				_noname_alloc.pop();
			}
			buf->_tags.resize(_buffer_tag_alloc_max); // allocate space for tags
			buffer_created.invoke_noret(*buf);
			return buf;
		}

		/// Returns the \ref code::interpretation of the given \ref buffer corresponding to the given
		/// \ref code::buffer_encoding, creating a new one if none is found.
		///
		/// \todo Asynchronous loading.
		std::shared_ptr<code::interpretation> open_interpretation(
			const std::shared_ptr<buffer> &buf, const code::buffer_encoding &encoding
		) {
			_buffer_data &data = _get_data_of(*buf);
			std::u8string encoding_name(encoding.get_name());
			auto it = data.interpretations.find(encoding_name);
			if (it != data.interpretations.end()) {
				auto ptr = it->second.lock();
				if (ptr) {
					return ptr;
				}
			} else {
				it = data.interpretations.try_emplace(std::move(encoding_name)).first;
			}
			auto ptr = std::make_shared<code::interpretation>(buf, encoding);
			ptr->_tags.resize(_interpretation_tag_alloc_max); // allocate space for tags
			it->second = ptr;
			interpretation_created.invoke_noret(*ptr);
			return ptr;
		}


		/// Allocates a buffer tag slot and returns the \ref buffer_tag_token. This function tries to enlarge
		/// \ref buffer::_tags of all open buffers if no previously deallocated slot is available.
		[[nodiscard]] buffer_tag_token allocate_buffer_tag() {
			std::size_t res = 0;
			if (_buffer_tag_alloc.empty()) {
				res = _buffer_tag_alloc_max++;
				for_each_buffer([this](std::shared_ptr<buffer> buf) {
					buf->_tags.resize(_buffer_tag_alloc_max);
				});
			} else {
				res = _buffer_tag_alloc.top();
				_buffer_tag_alloc.pop();
			}
			return buffer_tag_token(res);
		}
		/// Deallocates a buffer tag slot, and clears the corresponding entries in \ref buffer::_tags for all open
		/// documents.
		void deallocate_buffer_tag(buffer_tag_token &token) {
			for_each_buffer([index = token._index](std::shared_ptr<buffer> buf) {
				buf->_tags[index].reset();
			});
			_buffer_tag_alloc.emplace(token._index);
			token.reset();
		}

		/// Allocates an interpretation tag slot and returns the \ref interpretation_tag_token.
		[[nodiscard]] interpretation_tag_token allocate_interpretation_tag() {
			std::size_t res = 0;
			if (_interpretation_tag_alloc.empty()) {
				res = _interpretation_tag_alloc_max++;
				for_each_interpretation([this](std::shared_ptr<code::interpretation> interp) {
					interp->_tags.resize(_interpretation_tag_alloc_max);
				});
			} else {
				res = _interpretation_tag_alloc.top();
				_interpretation_tag_alloc.pop();
			}
			return interpretation_tag_token(res);
		}
		/// Deallocates a buffer tag slot, and clears the corresponding entries in \ref code::interpretation::_tags
		/// for all open interpretations.
		void deallocate_interpretation_tag(interpretation_tag_token &token) {
			for_each_interpretation([index = token._index](std::shared_ptr<code::interpretation> interp) {
				interp->_tags[index].reset();
			});
			_interpretation_tag_alloc.emplace(token._index);
			token.reset();
		}


		/// Iterates over all open buffers.
		template <typename Cb> void for_each_buffer(Cb &&cb) {
			for (auto &pair : _file_map) {
				auto buf = pair.second.buf.lock();
				// should not be nullptr since all disposed documents are removed from the map
				assert_true_logical(buf != nullptr, "corrupted document registry");
				cb(std::move(buf));
			}
			for (auto &wptr : _noname_map) {
				if (auto buf = wptr.buf.lock()) {
					cb(std::move(buf));
				}
			}
		}
		/// Iterates over all open interpretations.
		template <typename Cb> void for_each_interpretation(Cb &&cb) {
			for_each_buffer([this, callback = std::forward<Cb>(cb)](std::shared_ptr<buffer> buf) {
				for (auto &pair : _get_data_of(*buf).interpretations) {
					if (auto interp_ptr = pair.second.lock()) {
						callback(std::move(interp_ptr));
					}
				}
			});
		}
		/// Iterates over all interpretations of the \ref buffer that corresponds to the given
		/// \p std::filesystem::path.
		template <typename Cb> void for_each_interpretation_of_buffer(Cb &&cb, const std::filesystem::path &path) {
			auto it = _file_map.find(path);
			if (it != _file_map.end()) {
				if (auto buf = it->second.buf.lock()) {
					for (auto &pair : it->second.interpretations) {
						if (auto interp_ptr = pair.second.lock()) {
							cb(pair.first, std::move(interp_ptr));
						}
					}
				}
			}
		}


		info_event<buffer_info>
			/// Invoked when a buffer has been created. Components and plugins that make use of per-buffer tags can
			/// handle this event to initialize them. Note that tags are automatically disposed when the buffer is
			/// disposed.
			buffer_created,
			/// Invoked when a buffer is about to be disposed. Handlers should be careful not to modify the buffer.
			buffer_disposing;
		info_event<interpretation_info>
			/// Invoked when an interpretation has been created. Components and plugins that make use of
			/// per-interpretation tags can handle this event to initialize them. Note that tags are automatically
			/// disposed when the interpretation is disposed.
			interpretation_created;
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
			/// Default move constructor.
			_buffer_data(_buffer_data&&) = default;
			/// No copy assignment.
			_buffer_data &operator=(const _buffer_data&) = delete;
			/// Default move assignment.
			_buffer_data &operator=(_buffer_data&&) = default;

			std::weak_ptr<buffer> buf; ///< Pointer to the buffer.
			/// All \ref code::interpretation "interpretations" of this \ref buffer.
			std::unordered_map<std::u8string, std::weak_ptr<code::interpretation>> interpretations;
		};

		/// Stores all \p buffer "buffers" that correspond to files and their <tt>std::filesystem::path</tt>s.
		std::unordered_map<std::filesystem::path, _buffer_data> _file_map;
		/// Stores all \p buffer "buffers" that don't correspond to files.
		std::vector<_buffer_data> _noname_map;
		/// Stores indices of disposed buffers in \ref _noname_map for more efficient allocation of indices.
		std::stack<std::size_t> _noname_alloc;

		std::stack<std::size_t>
			_buffer_tag_alloc, ///< Stores deallocated buffer tag indices.
			_interpretation_tag_alloc; ///< Stores deallocated interpretation tag indices.
		std::size_t
			/// Stores the next index to allocate for a tag if \ref _buffer_tag_alloc is empty.
			_buffer_tag_alloc_max = 0,
			/// Stores the next index to allocate for a tag if \ref _interpretation_tag_alloc is empty.
			_interpretation_tag_alloc_max = 0;

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
				std::size_t index = std::get<std::size_t>(buf._fileid);
				_noname_map[index] = _buffer_data();
				_noname_alloc.emplace(index);
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
				// TODO merge buffers? it probably makes sense to swap them
			} else {
				target = _buffer_data(); // keep _buffer_data::buf valid
			}
		}
	};

	/// Manages everything related to editors. Essentially a hub for \ref buffer_manager, \ref encoding_manager,
	class manager {
	public:
		/// Constructor. Registers interaction modes for interaction mode registries.
		manager() {
			code_interactions.mapping.emplace(
				u8"prepare_drag", []() {
					return std::make_unique<
						interaction_modes::mouse_prepare_drag_mode_activator<code::caret_set>
					>();
				}
			);
			code_interactions.mapping.emplace(
				u8"single_selection", []() {
					return std::make_unique<
						interaction_modes::mouse_single_selection_mode_activator<code::caret_set>
					>();
				}
			);

			binary_interactions.mapping.emplace(
				u8"prepare_drag", []() {
					return std::make_unique<
						interaction_modes::mouse_prepare_drag_mode_activator<binary::caret_set>
					>();
				}
			);
			binary_interactions.mapping.emplace(
				u8"single_selection", []() {
					return std::make_unique<
						interaction_modes::mouse_single_selection_mode_activator<binary::caret_set>
					>();
				}
			);
		}

		buffer_manager buffers; ///< Main manager of all buffers.
		code::encoding_manager encodings; ///< Manager of all encodings.
		interaction_mode_registry<code::caret_set> code_interactions; ///< \ref interaction_manager for code editors.
		interaction_mode_registry<binary::caret_set> binary_interactions; ///< \ref interaction_manager for binary editors.
	};
}
