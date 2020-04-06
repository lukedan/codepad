// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "settings.h"

/// \file
/// Implementation of settings system.

namespace codepad {
	settings::profile *settings::profile::find_child(std::u8string_view name) {
		// return the already separated profile
		if (auto it = _children.find(name); it != _children.end()) {
			return it->second.get();
		}
		// try to find & separate the profile
		std::u8string key;
		key.reserve(name.length() + 2);
		key += '[';
		key += name;
		key += ']';
		if (auto fmem = _value.find_member(key); fmem != _value.member_end()) { // yes
			if (auto obj = fmem.value().cast<json::storage::object_t>()) {
				auto result = _children.emplace(std::u8string(name), std::make_unique<profile>(this, obj.value()));
				assert_true_logical(result.second, "map insert should have succeeded");
				return result.first->second.get();
			}
		}
		return nullptr; // nope
	}


	settings::profile &settings::get_main_profile() {
		if (!_main_profile) {
			if (!_storage.get_value().is<json::storage::object_t>()) {
				_storage.value.emplace<json::value_storage::object>();
			}
			_main_profile = std::make_unique<profile>(
				nullptr, _storage.get_value().get<json::storage::object_t>()
				);
		}
		return *_main_profile;
	}

	void settings::set(json::value_storage val) {
		++_timestamp;
		_main_profile = nullptr;
		_storage = std::move(val);
		changed.invoke();
	}

	void settings::load(const std::filesystem::path &path) {
		set(json::store(json::parse_file<json::rapidjson::document_t>(path).root()));
	}
}
