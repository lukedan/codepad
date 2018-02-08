#pragma once

#include "context.h"

namespace codepad::editor::code {
	// TODO can either
	//  - use std::filesystem::equivalent to completely avoid duplicate files,
	//    but have to check all open files when opening one
	// or
	//  - use hashmap and paths, may have duplicate files and lead to inconsistency
	//    between open files that are actually the same file
	class context_manager {
		friend class text_context;
	public:
		std::shared_ptr<text_context> open_file(const std::filesystem::path &path) {
			auto ins = _file_map.insert({path, std::weak_ptr<text_context>()});
			if (!ins.second) {
				auto ptr = ins.first->second.lock();
				assert_true_logical(ptr != nullptr, "context destruction not notified");
				return ptr;
			}
			auto res = std::make_shared<text_context>(path);
			ins.first->second = res;
			return res;
		}
		std::shared_ptr<text_context> new_file() {
			std::shared_ptr<text_context> ctx;
			if (_noname_alloc.size() > 0) {
				ctx = std::make_shared<text_context>(_noname_alloc.back());
				_noname_map[_noname_alloc.back()] = ctx;
				_noname_alloc.pop_back();
			} else {
				ctx = std::make_shared<text_context>(_noname_map.size());
				_noname_map.push_back(ctx);
			}
			return ctx;
		}

		static context_manager &get();
	protected:
		std::unordered_map<std::filesystem::path, std::weak_ptr<text_context>> _file_map;
		std::vector<std::weak_ptr<text_context>> _noname_map;
		std::vector<size_t> _noname_alloc;

		void _on_deleting_context(const std::variant<size_t, std::filesystem::path> &id) {
			if (std::holds_alternative<size_t>(id)) {
				_noname_alloc.push_back(std::get<size_t>(id));
			} else {
				assert_true_logical(
					_file_map.erase(std::get<std::filesystem::path>(id)) == 1,
					"deleting invalid context"
				);
			}
		}
		void _on_saved_new_context(size_t id, const std::filesystem::path &f) {
			_noname_alloc.push_back(id);
			auto insres = _file_map.insert({f, _noname_map[id]});
			if (!insres.second) {
				// TODO merge contexts
			}
		}
	};
}
