// Copyright(c) the Codepad contributors.All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// A thin wrapper around RapidJSON, and utility types and functions.

#include <vector>
#include <map>
#include <string>
#include <variant>
#include <fstream>

#include <rapidjson/document.h>

#include "encodings.h"
#include "misc.h"

namespace codepad::json {
	/// Default encoding used by RapidJSON.
	using encoding = rapidjson::UTF8<char>;

	/// RapidJSON type that holds a JSON object.
	using node_t = rapidjson::GenericValue<encoding>;
	/// RapidJSON type that holds a JSON object, and can be used to parse JSON text.
	using root_node_t = rapidjson::GenericDocument<encoding>;
	/// Dummy struct used to represent the value of \p null.
	struct null_t {
	};
	/// Stores a JSON value.
	struct value_t {
		using object = std::map<str_t, value_t>; ///< A JSON object.
		using array = std::vector<value_t>; ///< A JSON array.
		/// The \p std::variant type used to store any object.
		using storage = std::variant<null_t, bool, std::int64_t, double, str_t, object, array>;

		/// Default constructor.
		value_t() = default;
		/// Initializes \ref value with the given arguments.
		template <typename T, typename ...Args> explicit value_t(std::in_place_type_t<T>, Args &&...args) :
			value(std::in_place_type<T>, std::forward<Args>(args)...) {
		}

		storage value; ///< The value.
	};
	/// Constructs a \ref value_t that holds a value of type \p T, initialized by \ref args.
	template <typename T, typename ...Args> inline value_t make_value(Args &&...args) {
		return value_t(std::in_place_type<T>, std::forward<Args>(args)...);
	}

	/// Parses a JSON file.
	inline root_node_t parse_file(const std::filesystem::path &path) {
		std::ifstream fin(path, std::ios::binary | std::ios::ate);
		std::ifstream::pos_type sz = fin.tellg();
		fin.seekg(0, std::ios::beg);
		root_node_t root;
		char *mem = static_cast<char*>(std::malloc(sz));
		fin.read(mem, sz);
		assert_true_sys(fin.good(), "cannot load JSON file");
		root.Parse(mem, sz);
		std::free(mem);
		return root;
	}

	/// Returns a STL string for a JSON string object. The caller is responsible of checking if the object is
	/// actually a string.
	///
	/// \param v A JSON object. The caller must guarantee that the object is a string.
	/// \return A STL string whose content is the same as the JSON object.
	/// \remark This is the preferred way to get a string object's contents
	///         since it may contain null characters.
	inline str_t get_as_string(const node_t &v) {
		return str_t(v.GetString(), v.GetStringLength());
	}
	/// Returns a view of the given JSON string object. The caller is responsible of checking if the object is
	/// actually a string.
	inline str_view_t get_as_string_view(const node_t &v) {
		return str_view_t(v.GetString(), v.GetStringLength());
	}

	/// Returns the \ref value_t corresponding to the given \ref node_t.
	///
	/// \todo Rid of recursion?
	inline value_t extract_value(const node_t &node) {
		if (node.IsNull()) {
			return make_value<null_t>();
		}
		if (node.IsBool()) {
			return make_value<bool>(node.GetBool());
		}
		if (node.IsInt64()) {
			return make_value<std::int64_t>(node.GetInt64());
		}
		if (node.IsDouble()) {
			return make_value<double>(node.GetDouble());
		}
		if (node.IsString()) {
			return make_value<str_t>(get_as_string(node));
		}
		if (node.IsObject()) {
			value_t val(std::in_place_type<value_t::object>);
			auto &dict = std::get<value_t::object>(val.value);
			for (auto it = node.MemberBegin(); it != node.MemberEnd(); ++it) {
				dict.try_emplace(get_as_string(it->name), extract_value(it->value));
			}
			return val;
		}
		if (node.IsArray()) {
			value_t val(std::in_place_type<value_t::array>);
			auto &list = std::get<value_t::array>(val.value);
			for (const node_t &n : node.GetArray()) {
				list.emplace_back(extract_value(n));
			}
			return val;
		}
		assert_true_logical(false, "JSON node with invalid type");
		return value_t();
	}

	namespace _details {
		/// Implementation of \ref json::try_get().
		template <typename Res, typename Obj, bool(Obj::*TypeVerify)() const, Res(Obj::*Getter)() const> inline bool try_get(
			const Obj &v, const str_t &s, Res &ret
		) {
			auto found = v.FindMember(s.c_str());
			if (found != v.MemberEnd() && (found->value.*TypeVerify)()) {
				ret = (found->value.*Getter)();
				return true;
			}
			return false;
		}
	}
	/// Checks if the object has a member with a certain name which is of type \p T,
	/// then returns the value of the member if there is.
	///
	/// \param val A JSON object.
	/// \param name The desired name of the member.
	/// \param ret If the function returns true, then \p ret holds the value of the member.
	/// \return \p true if it has a member of type \p T.
	template <typename T> bool try_get(const node_t &val, const str_t &name, T &ret);
	template <> inline bool try_get<bool>(const node_t &val, const str_t &name, bool &ret) {
		return _details::try_get<bool, node_t, &node_t::IsBool, &node_t::GetBool>(val, name, ret);
	}
	template <> inline bool try_get<double>(const node_t &val, const str_t &name, double &ret) {
		return _details::try_get<double, node_t, &node_t::IsNumber, &node_t::GetDouble>(val, name, ret);
	}
	template <> inline bool try_get<str_t>(const node_t &val, const str_t &name, str_t &ret) {
		auto found = val.FindMember(name.c_str());
		if (found != val.MemberEnd() && found->value.IsString()) {
			ret = get_as_string(found->value);
			return true;
		}
		return false;
	}

	/// \ref try_get with a default value.
	///
	/// \param v A JSON object.
	/// \param s The desired name of the member.
	/// \param def The default value if no such member is found.
	/// \return The value if one exists, or the default otherwise.
	template <typename T> inline T get_or_default(const node_t &v, const str_t &s, const T &def) {
		T res;
		if (try_get(v, s, res)) {
			return res;
		}
		return def;
	}
}
