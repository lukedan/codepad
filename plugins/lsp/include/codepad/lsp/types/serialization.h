// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Seralizer and deserializer.

#include <stack>

#include <rapidjson/writer.h>

#include "codepad/core/json/storage.h"
#include "codepad/core/json/default_engine.h"

#include "common.h"

namespace codepad::lsp::types {
	/// Serializer. Uses the RapidJSON SAX writer.
	template <typename Stream> class serializer : public visitor_base {
	public:
		/// Initializes \ref _writer.
		explicit serializer(rapidjson::Writer<Stream> &writer) : _writer(&writer) {
		}

		/// Writes a \ref null.
		void visit(null&) override {
			_writer->Null();
		}
		/// Writes a \ref boolean.
		void visit(boolean &b) override {
			_writer->Bool(b);
		}
		/// Writes a \ref integer.
		void visit(integer &i) override {
			_writer->Int(i);
		}
		/// Writes a \ref uinteger.
		void visit(uinteger &i) override {
			_writer->Uint(i);
		}
		/// Writes a \ref decimal.
		void visit(decimal &d) override {
			_writer->Double(d);
		}
		/// Writes a \ref string.
		void visit(string &s) override {
			_writer->String(reinterpret_cast<const char*>(s.c_str()), s.size(), true);
		}
		/// Writes a \ref any.
		void visit(any &a) override {
			_visit_any(json::storage::value_t(a));
		}

		/// Writes a \ref object.
		void visit(object &obj) override {
			_writer->StartObject();
			obj.visit_fields(*this);
			_writer->EndObject();
		}
		/// Writes the result of \ref numerical_enum_base::get_value().
		void visit(numerical_enum_base &e) override {
			_writer->Int(e.get_value());
		}
		/// Writes the result of \ref string_enum_base::get_value().
		void visit(string_enum_base &e) override {
			std::u8string_view str = e.get_value();
			_writer->String(reinterpret_cast<const char*>(str.data()), str.size(), true);
		}
		/// Writes a \ref array_base.
		void visit(array_base &arr) override {
			_writer->StartArray();
			std::size_t len = arr.get_length();
			for (std::size_t i = 0; i < len; ++i) {
				arr.visit_element_at(i, *this);
			}
			_writer->EndArray();
		}
		/// Invokes \ref primitive_variant_base::visit_value().
		void visit(primitive_variant_base &var) override {
			var.visit_value(*this);
		}
		/// Invokes \ref custom_variant_base::visit_value().
		void visit(custom_variant_base &var) override {
			var.visit_value(*this);
		}
		/// Writes a \ref map_base.
		void visit(map_base &m) override {
			_writer->StartObject();
			m.visit_entries(*this);
			_writer->EndObject();
		}

		/// Serializes the field if it's not empty.
		void visit_field(std::u8string_view name, optional_base &opt) override {
			if (opt.has_value()) {
				_start_field(name);
				opt.visit_value(*this);
				_end_field();
			}
		}
	protected:
		rapidjson::Writer<Stream> *_writer = nullptr; ///< The SAX writer.

		/// Invokes \p rapidjson::Writer::Key().
		void _start_field(std::u8string_view name) override {
			_writer->Key(reinterpret_cast<const char*>(name.data()), name.size(), true);
		}
		/// Nothing to do.
		void _end_field() override {
		}

		/// Visits the given \ref json::storage::value_t to write it to the stream recursively.
		void _visit_any(const json::storage::value_t &val) {
			if (val.is<json::null_t>()) {
				_writer->Null();
			} else if (auto b = val.try_cast<bool>()) {
				_writer->Bool(b.value());
			} else if (auto i64 = val.try_cast<std::int64_t>()) {
				_writer->Int64(i64.value());
			} else if (auto u64 = val.try_cast<std::uint64_t>()) {
				_writer->Uint64(u64.value());
			} else if (auto d = val.try_cast<double>()) {
				_writer->Double(d.value());
			} else if (auto s = val.try_cast<std::u8string_view>()) {
				_writer->String(reinterpret_cast<const char*>(s.value().data()), s.value().size(), true);
			} else if (auto arr = val.try_cast<json::storage::array_t>()) {
				_writer->StartArray();
				for (auto it = arr->begin(); it != arr->end(); ++it) {
					_visit_any(*it);
				}
				_writer->EndArray();
			} else if (auto obj = val.try_cast<json::storage::object_t>()) {
				_writer->StartObject();
				for (auto it = obj->member_begin(); it != obj->member_end(); ++it) {
					_start_field(it.name());
					_visit_any(it.value());
					_end_field();
				}
				_writer->EndObject();
			} else {
				assert_true_logical(false, "invalid JSON storage object");
			}
		}
	};

	/// Deserializer.
	class deserializer : public visitor_base {
	public:
		/// Initializes this deserializer using the given JSON value.
		explicit deserializer(json::value_t val) {
			_stack.emplace(val);
		}

		/// Does nothing for \p null's.
		void visit(null&) override {
			if (_stack.top().empty()) {
				logger::get().log_error(CP_HERE) << "invalid value";
				return;
			}
			if (!_stack.top().is<json::null_t>()) {
				logger::get().log_error(CP_HERE) << "value is not null";
				return;
			}
		}
		/// Deserializes a \ref boolean.
		void visit(boolean &b) override {
			_get_value(b);
		}
		/// Deserializes a \ref integer.
		void visit(integer &i) override {
			_get_value(i);
		}
		/// Deserializes a \ref uinteger.
		void visit(uinteger &u) override {
			_get_value(u);
		}
		/// Deserializes a \ref decimal.
		void visit(decimal &d) override {
			_get_value(d);
		}
		/// Deserializes a \ref string.
		void visit(string &s) override {
			_get_value(s);
		}
		/// Deserializes a \ref any.
		void visit(any &a) override {
			if (_stack.top().empty()) {
				logger::get().log_error(CP_HERE) << "invalid value";
				return;
			}
			a = json::store(_stack.top());
		}

		/// Deserializes a \ref object.
		void visit(object &obj) override {
			obj.visit_fields(*this);
		}
		/// Deserializes a \ref numerical_enum_base.
		void visit(numerical_enum_base &e) override {
			integer i;
			if (_get_value(i)) {
				e.set_value(i);
			}
		}
		/// Deserializes a \ref string_enum_base.
		void visit(string_enum_base &e) override {
			std::u8string_view str;
			if (_get_value(str)) {
				e.set_value(str);
			}
		}
		/// Deserializes an \ref array_base.
		void visit(array_base &arr) override {
			if (_stack.top().empty()) {
				logger::get().log_error(CP_HERE) << "invalid value";
				return;
			}
			if (auto json_arr = _stack.top().try_cast<json::array_t>()) {
				std::size_t length = json_arr->size();
				arr.set_length(length);
				for (std::size_t i = 0; i < length; ++i) {
					_stack.emplace(json_arr.value()[i]);
					arr.visit_element_at(i, *this);
					_stack.pop();
				}
				return;
			}
			logger::get().log_error(CP_HERE) << "invalid value type: expected array";
		}
		/// Deserializes a \ref primitive_variant_base.
		void visit(primitive_variant_base &var) override {
			if (_stack.top().empty()) {
				logger::get().log_error(CP_HERE) << "invalid value";
				return;
			}
			if (_stack.top().is<json::null_t>()) {
				var.set_null();
				return;
			}
			if (auto obj = _stack.top().try_cast<bool>()) {
				var.set_boolean(obj.value());
				return;
			}
			if (auto obj = _stack.top().try_cast<std::int64_t>()) {
				var.set_int64(obj.value());
				return;
			}
			if (auto obj = _stack.top().try_cast<double>()) {
				var.set_decimal(obj.value());
				return;
			}
			if (auto obj = _stack.top().try_cast<std::u8string_view>()) {
				var.set_string(obj.value());
				return;
			}
			if (_stack.top().is<json::array_t>()) {
				var.set_array_and_visit(*this);
				return;
			}
			if (_stack.top().is<json::object_t>()) {
				var.set_object_and_visit(*this);
				return;
			}
			assert_true_sys(false, "failed to detect type of JSON object");
		}
		/// Deserializes a \ref custom_variant_base.
		void visit(custom_variant_base &var) override {
			if (_stack.top().empty()) {
				logger::get().log_error(CP_HERE) << "invalid value";
				return;
			}
			var.deduce_type_and_visit(*this, _stack.top());
		}
		/// Deserializes a \ref map_base.
		void visit(map_base &map) override {
			if (_stack.top().empty()) {
				logger::get().log_error(CP_HERE) << "invalid value";
				return;
			}
			if (auto obj = _stack.top().try_cast<json::object_t>()) {
				for (auto it = obj->member_begin(); it != obj->member_end(); ++it) {
					_stack.emplace(it.value());
					map.insert_visit_entry(*this, it.name());
					_stack.pop();
				}
				return;
			}
			logger::get().log_error(CP_HERE) << "invalid value type: expected object";
		}

		/// Finds a field with the given name, and deserializes it if one exists.
		void visit_field(std::u8string_view name, optional_base &opt) override {
			if (_stack.top().empty()) {
				logger::get().log_error(CP_HERE) << "invalid value";
				return;
			}
			if (auto obj = _stack.top().try_cast<json::object_t>()) {
				auto it = obj->find_member(name);
				if (it != obj->member_end()) {
					opt.emplace_value();
					_stack.push(it.value());
					opt.visit_value(*this);
					_stack.pop();
				} else {
					opt.clear_value();
				}
				return;
			}
			logger::get().log_error(CP_HERE) << "current value is not an object";
			return;
		}
	protected:
		std::stack<json::value_t> _stack; ///< The stack of JSON values that are being visited.


		/// Adds a new entry to \ref _stack.
		void _start_field(std::u8string_view name) override {
			if (!_stack.top().empty()) {
				if (auto obj = _stack.top().try_cast<json::object_t>()) {
					auto it = obj->find_member(name);
					if (it != obj->member_end()) {
						_stack.push(it.value());
						return;
					} else {
						logger::get().log_error(CP_HERE) << "member " << name << " not found";
					}
				} else {
					logger::get().log_error(CP_HERE) << "current value is not an object";
				}
			} else {
				logger::get().log_error(CP_HERE) << "invalid value";
			}
			_stack.push(json::value_t());
		}
		/// Removes an element from \ref _stack.
		void _end_field() override {
			_stack.pop();
		}

		/// Checks that the current value is of the specified type, and retrieves its value.
		///
		/// \return \p false if failed to obtain the value.
		template <typename T> bool _get_value(T &val) {
			if (_stack.top().empty()) {
				logger::get().log_error(CP_HERE) << "invalid value";
				return false;
			}
			if (!_stack.top().is<T>()) {
				logger::get().log_error(CP_HERE) << "invalid value type: expected " << demangle(typeid(T).name());
				return false;
			}
			val = _stack.top().get<T>();
			return true;
		}
	};

	/// Serializes an object in a more human-readable way to the logger.
	class logger_serializer : public visitor_base {
	public:
		/// Initializes \ref _entry.
		explicit logger_serializer(logger::log_entry ent) : _entry(std::move(ent)) {
		}

		/// Visits a \ref null object.
		void visit(null&) override {
			_entry << "(null)";
		}
		/// Visits a \ref boolean.
		void visit(boolean &v) override {
			_entry << "(bool) " << (v ? "true" : "false");
		}
		/// Visits a \ref integer.
		void visit(integer &i) override {
			_entry << "(int) " << i;
		}
		/// Visits a \ref uinteger.
		void visit(uinteger &u) override {
			_entry << "(uint) " << u;
		}
		/// Visits a \ref decimal.
		void visit(decimal &d) override {
			_entry << "(float) " << d;
		}
		/// Visits a \ref string.
		void visit(string &s) override {
			_entry << "(string) `" << s << "`";
		}
		/// Visits a \ref any.
		void visit(any &v) override {
			_entry << "(any)"; // TODO
		}

		/// Visits a \ref object.
		void visit(object &obj) override {
			_entry << "(object) {\n";
			++_indent;
			obj.visit_fields(*this);
			--_indent;
			_write_indent();
			_entry << "}";
		}
		/// Visits a \ref numerical_enum_base.
		void visit(numerical_enum_base &e) override {
			_entry << "(num_enum) " << e.get_value();
		}
		/// Visits a \ref string_enum_base.
		void visit(string_enum_base &e) override {
			_entry << "(str_enum) " << e.get_value();
		}
		/// Visits an \ref array_base.
		void visit(array_base &arr) override {
			_entry << "(arr) [\n";
			++_indent;
			for (std::size_t i = 0; i < arr.get_length(); ++i) {
				_write_indent();
				arr.visit_element_at(i, *this);
				_entry << "\n";
			}
			--_indent;
			_write_indent();
			_entry << "]";
		}
		/// Visits a \ref primitive_variant_base.
		void visit(primitive_variant_base &variant) override {
			_entry << "(primitive variant) ";
			variant.visit_value(*this);
		}
		/// Deserializes a \ref custom_variant_base.
		void visit(custom_variant_base &var) override {
			_entry << "(custom variant) ";
			var.visit_value(*this);
		}
		/// Visits a \ref map_base.
		void visit(map_base &map) override {
			_entry << "(map) {\n";
			++_indent;
			map.visit_entries(*this);
			--_indent;
			_write_indent();
			_entry << "}";
		}


		/// Visits a field that is optional.
		void visit_field(std::u8string_view name, optional_base &opt) override {
			_write_indent();
			_entry << name << ": (optional) ";
			if (opt.has_value()) {
				opt.visit_value(*this);
			} else {
				_entry << "(empty)";
			}
			_entry << "\n";
		}
	protected:
		logger::log_entry _entry; ///< The entry to write the log to.
		std::size_t _indent = 0; ///< Current indent.

		/// Writes the appropriate indent to \ref _entry.
		void _write_indent() {
			for (std::size_t i = 0; i < _indent; ++i) {
				_entry << "  ";
			}
		}

		/// Starts visiting a field.
		void _start_field(std::u8string_view name) override {
			_write_indent();
			_entry << name << ": ";
		}
		/// End visiting a field.
		void _end_field() override {
			_entry << "\n";
		}
	};
}
