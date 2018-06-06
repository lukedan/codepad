#pragma once

/// \file
/// Declaration and implementation of manager class for documents.

#include "document.h"

namespace codepad::editor::code {
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
			ins.first->second = res;
			return res;
		}
		/// Creates a new file, identified by a \p size_t.
		std::shared_ptr<document> new_file() {
			std::shared_ptr<document> ctx;
			if (_noname_alloc.size() > 0) {
				ctx = std::make_shared<document>(_noname_alloc.back());
				_noname_map[_noname_alloc.back()] = ctx;
				_noname_alloc.pop_back();
			} else {
				ctx = std::make_shared<document>(_noname_map.size());
				_noname_map.push_back(ctx);
			}
			return ctx;
		}

		/// Returns the global \ref document_manager.
		static document_manager &get();
	protected:
		/// Stores all \p document "documents" that correspond to files and their <tt>std::filesystem::path</tt>s.
		std::unordered_map<std::filesystem::path, std::weak_ptr<document>> _file_map;
		/// Stores all \p document "documents" that don't correspond to files.
		std::vector<std::weak_ptr<document>> _noname_map;
		/// Stores indices of disposed documents in \ref _noname_map for more efficient allocation of indices.
		std::vector<size_t> _noname_alloc;

		/// Called when a document is being disposed, to remove the corresponding entry in \ref _file_map or add its
		/// index to \ref _noname_map.
		void _on_deleting_document(const std::variant<size_t, std::filesystem::path> &id) {
			if (std::holds_alternative<size_t>(id)) {
				_noname_alloc.push_back(std::get<size_t>(id));
			} else {
				assert_true_logical(
					_file_map.erase(std::get<std::filesystem::path>(id)) == 1,
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
			_noname_alloc.push_back(id);
			auto insres = _file_map.insert({f, _noname_map[id]});
			if (!insres.second) {
				// TODO merge documents?
			}
		}
	};
}
