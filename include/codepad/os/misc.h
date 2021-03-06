// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Miscellaneous platform-seecific types and function definitions.
/// These functions must be implemented on a per-platform basis.

#include <optional>
#include <system_error>

#include "../core/misc.h"
#include "../ui/misc.h"
#include "../ui/window.h"

namespace codepad::os {
	/// Contains a return value of type \p T and a \p std::error_code.
	template <typename T> struct result {
	public:
		/// Initializes \ref _value using the given error code.
		result(std::error_code err) : _value(std::in_place_type<std::error_code>, err) {
		}
		/// Initializes \ref _value using the given value upon success.
		result(T res) : _value(std::in_place_type<T>, std::move(res)) {
		}

		/// Returns a reference to the value. This result must not be an error.
		[[nodiscard]] T &value() {
			return std::get<T>(_value);
		}
		/// \overload
		[[nodiscard]] const T &value() const {
			return std::get<T>(_value);
		}

		/// Takes and returns the value.
		[[nodiscard]] std::optional<T> take() {
			if (std::holds_alternative<T>(_value)) {
				return std::move(std::get<T>(value));
			}
			return std::nullopt;
		}

		/// Accesses the underlying object. This must not be an error.
		[[nodiscard]] T *operator->() {
			return &std::get<T>(_value);
		}
		/// \overload
		[[nodiscard]] const T *operator->() const {
			return &std::get<T>(_value);
		}

		/// Converts the underlying value, if it's not an error. This version does not modify this object.
		template <typename Cb> auto into(Cb &&cb) const {
			using _into_t = std::invoke_result_t<Cb&&, const T&>;

			if (std::holds_alternative<T>(_value)) {
				return result<_into_t>(cb(std::get<T>(_value)));
			}
			return result<_into_t>(std::get<std::error_code>(_value));
		}
		/// Converts the underlying value, if it's not an error. This version allows the caller to take the value.
		template <typename Cb> auto into_move(Cb &&cb) {
			using _into_t = std::invoke_result_t<Cb&&, T&&>;

			if (std::holds_alternative<T>(_value)) {
				return result<_into_t>(cb(std::move(std::get<T>(_value))));
			}
			return result<_into_t>(std::get<std::error_code>(_value));
		}

		/// Returns the error code. If the result is not an error, returns a default-constructed \p std::error_code.
		[[nodiscard]] std::error_code error_code() const {
			if (std::holds_alternative<std::error_code>(_value)) {
				return std::get<std::error_code>(_value);
			}
			return std::error_code();
		}

		/// Returns whether this result is an error.
		[[nodiscard]] bool is_error() const {
			return std::holds_alternative<std::error_code>(_value);
		}
		/// Returns whether this result holds a valid value.
		[[nodiscard]] explicit operator bool() const {
			return !is_error();
		}
	protected:
		std::variant<std::error_code, T> _value; ///< The value.
	};

	/// Initializes platform-specific aspects of the program with the given command line arguments.
	void initialize(int, char**);

	/// Contains functions related to file dialogs.
	class APIGEN_EXPORT_RECURSIVE file_dialog {
	public:
		/// Specifies the type of a file dialog.
		enum class type {
			single_selection, ///< Only one file can be selected.
			multiple_selection ///< Multiple files can be selected.
		};

		/// Shows an open file dialog that asks the user to select one or multiple files.
		///
		/// \return The list of files that the user selects, or empty if the user cancels the operation.
		[[nodiscard]] static std::vector<std::filesystem::path> show_open_dialog(const ui::window*, type);
	};

	/// Contains functions associated with clipboards.
	class APIGEN_EXPORT_RECURSIVE clipboard {
	public:
		/// Copies the given text to the clipboard.
		static void set_text(std::u8string_view);
		/// Retrieves text from the clipboard.
		[[nodiscard]] static std::optional<std::u8string> get_text();
		/// Returns if there's text in the clipboard.
		static bool is_text_available();
	};

	/// Getters for various platform-dependent system parameters.
	class system_parameters {
	public:
		/// Returns the radius of the zone where a drag operation is ready but does not start.
		[[nodiscard]] static double get_drag_deadzone_radius();

		/// Returns the width of the console window.
		[[nodiscard]] static std::size_t get_console_width();
	};
}
