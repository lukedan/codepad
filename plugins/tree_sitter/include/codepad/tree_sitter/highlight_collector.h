// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Declaration of iterators used to go through highlighted regions. This is basically a transcript of the Rust
/// version.

#include <optional>
#include <queue>

#include <tree_sitter/api.h>

#include "highlight_layer_iterator.h"

namespace codepad::tree_sitter {
	/// Collects highlight information from parsed syntax trees of a file, using \ref highlight_layer_iterator
	/// objects.
	class highlight_collector {
	public:
		/// Number of iterations between cancellation checks.
		constexpr static std::size_t cancellation_check_interval = 100;

		/// Stores a \ref editors::code::document_theme and a list of strings corresponding to the capture names for
		/// debugging.
		struct document_highlight_data {
			editors::code::document_theme theme; ///< Document theme data.
			std::vector<std::u8string> capture_names; ///< Capture names for debugging.
		};

		/// Creates a new iterator for the given interpretation.
		highlight_collector(
			const TSInput &input, const editors::code::interpretation &interp,
			const parser_ptr &parser, const language_configuration &lang,
			std::function<const language_configuration*(std::u8string_view)> lang_callback,
			std::size_t *cancellation_token
		) :
			_lang_callback(std::move(lang_callback)),
			_input(input), _interp(interp), _cancellation_token(cancellation_token) {

			_layers = highlight_layer_iterator::process_layers(
				{}, input, _interp, parser, lang, _lang_callback, cancellation_token
			);
		}

		/// Computes highlight data.
		[[nodiscard]] document_highlight_data compute(const parser_ptr&);

		/// Computes highlight data for the given layer and adds the results to the given
		/// \ref document_highlight_data.
		void compute_for_layer(document_highlight_data&, highlight_layer_iterator, const parser_ptr&);
	protected:
		std::deque<highlight_layer_iterator> _layers; ///< Queue of highlight layers to be handled next.
		/// A function that returns the \ref language_configuration that corresponds to a given language name.
		std::function<const language_configuration*(std::u8string_view)> _lang_callback;
		TSInput _input; ///< Input associated with \ref _interp.
		/// The interpretation associated with this iterator.
		const editors::code::interpretation &_interp;
		std::size_t _iterations = 0; ///< The number of iterations. Used when checking whether to cancel the operation.
		std::size_t *_cancellation_token = nullptr; ///< Cancellation token.

		/// Checks if the cancellation token is set. This function only really checks every
		/// \ref cancellation_check_interval calls.
		///
		/// \return \p true if \ref _cancellation_token is set.
		[[nodiscard]] bool _check_cancel() {
			if (_cancellation_token) {
				if (++_iterations >= cancellation_check_interval) {
					_iterations = 0;
					std::atomic_ref<std::size_t> cancel(*_cancellation_token);
					if (cancel != 0) {
						return true;
					}
				}
			}
			return false;
		}
	};
}
