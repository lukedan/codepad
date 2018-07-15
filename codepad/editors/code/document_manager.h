#pragma once

/// \file
/// Declaration and implementation of manager class for documents.

#include <stack>

#include "document.h"

namespace codepad::editor::code {
	/// Used to identify a \ref document in events that involve certain documents.
	struct document_info {
		/// Default constructor.
		document_info() = default;
		/// Assigns the specified document to \ref doc.
		explicit document_info(document &ptr) : doc(ptr) {
		}

		document &doc; ///< Reference to the document.
	};
	/// Manager of all \ref document "documents". All \ref document instances should be created with the instance
	/// returned by \ref document_manager::get.
	class document_manager {
		friend class document;
	public:
		/// Returns a \p std::shared_ptr<document> to the file specified by the given \p std::filesystem::path.
		/// If the file has not been opened, this function opens the file; otherwise, it returns the pointer
		/// returned by previous calls to this function. The file must exist.
		///
		/// \tparam Encoding The encoding used to open the document. If the document has previously been opened,
		///                  this parameter is ignored.
		template <typename Encoding> std::shared_ptr<document> open_file(std::filesystem::path path) {
			path = std::filesystem::canonical(path);
			auto ins = _file_map.insert({path, std::weak_ptr<document>()});
			if (!ins.second) {
				auto ptr = ins.first->second.lock();
				assert_true_logical(ptr != nullptr, "context destruction not notified");
				return ptr;
			}
			auto res = std::make_shared<document>(path, document::encoding_tag_t<Encoding>());
			res->_tags.resize(_tag_alloc_max); // allocate space for tags
			ins.first->second = res;
			document_created.invoke_noret(*res);
			return res;
		}
		/// Creates a new file not yet associated with a path, identified by a \p size_t.
		std::shared_ptr<document> new_file() {
			std::shared_ptr<document> ctx;
			if (_noname_alloc.empty()) {
				ctx = std::make_shared<document>(_noname_map.size());
				_noname_map.emplace_back(ctx);
			} else {
				ctx = std::make_shared<document>(_noname_alloc.top());
				_noname_map[_noname_alloc.top()] = ctx;
				_noname_alloc.pop();
			}
			ctx->_tags.resize(_tag_alloc_max); // allocate space for tags
			document_created.invoke_noret(*ctx);
			return ctx;
		}

		/// Allocates a tag slot and returns the index.
		///
		/// \remark This function tries to enlarge \ref document::_tags of all open documents if no previously
		///         deallocated slot is available.
		size_t allocate_tag() {
			size_t res;
			if (_tag_alloc.empty()) {
				res = _tag_alloc_max++;
				for_each_open_document([this](std::shared_ptr<document> &doc) {
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
		void deallocate_tag(size_t tag) {
			for_each_open_document([tag](std::shared_ptr<document> &doc) {
				doc->_tags[tag].reset();
				});
			_tag_alloc.emplace(tag);
		}

		/// Iterates through all open documents.
		///
		/// \param cb A callback function invoked for each open document.
		template <typename Cb> void for_each_open_document(Cb &&cb) {
			for (auto &pair : _file_map) {
				auto doc = pair.second.lock();
				// should not be nullptr since all disposed documents are removed from the map
				assert_true_logical(doc != nullptr, "corrupted document registry");
				cb(doc);
			}
			for (auto &wptr : _noname_map) {
				auto doc = wptr.lock();
				if (doc) {
					cb(doc);
				}
			}
		}

		event<document_info>
			/// Invoked when a document has been created. Components and plugins that make use of per-document tags
			/// may have to handle this (and only this, as the tags are automatically disposed) event to initialize
			/// them.
			document_created,
			/// Invoked when a document is about to be disposed. Handlers should be careful not to modify the
			/// document.
			document_disposing;

		/// Returns the global \ref document_manager.
		static document_manager &get();
	protected:
		/// Stores all \p document "documents" that correspond to files and their <tt>std::filesystem::path</tt>s.
		std::unordered_map<std::filesystem::path, std::weak_ptr<document>> _file_map;
		/// Stores all \p document "documents" that don't correspond to files.
		std::vector<std::weak_ptr<document>> _noname_map;
		/// Stores indices of disposed documents in \ref _noname_map for more efficient allocation of indices.
		std::stack<size_t> _noname_alloc;

		std::stack<size_t> _tag_alloc; ///< Stores deallocated tag indices.
		size_t _tag_alloc_max = 0; ///< Stores the next index to allocate for a tag if \ref _tag_alloc is empty.

		/// Called when a document is being disposed, to remove the corresponding entry in \ref _file_map or add its
		/// index to \ref _noname_map.
		void _on_deleting_document(document &doc) {
			document_disposing.invoke_noret(doc);
			if (std::holds_alternative<size_t>(doc._fileid)) {
				_noname_alloc.emplace(std::get<size_t>(doc._fileid));
			} else {
				assert_true_logical(
					_file_map.erase(std::get<std::filesystem::path>(doc._fileid)) == 1,
					"deleting invalid document"
				);
			}
		}
		/// Called when a newly-created document is being saved to move its entry into \ref _file_map. The file
		/// must have already been saved (i.e., the file must exist).
		///
		/// \todo Try to merge two documents when one is being saved to an already opened document.
		void _on_saved_new_document(size_t id, std::filesystem::path f) {
			f = std::filesystem::canonical(f);
			_noname_alloc.emplace(id);
			auto insres = _file_map.insert({f, _noname_map[id]});
			if (!insres.second) {
				// TODO merge documents?
			}
		}
	};
}
