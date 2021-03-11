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
	/// Iterates through highlighted regions in a given piece of code.
	class highlight_iterator {
	public:
		/// Number of iterations between cancellation checks.
		constexpr static std::size_t cancellation_check_interval = 100;

		/// Used to store an event during highlighting, indicating a highlight boundary.
		struct event {
			/// Default constructor.
			event() = default;
			/// Initializes all fields of this struct.
			explicit event(std::size_t pos, std::size_t hl = editors::theme_configuration::no_associated_theme)
				: position(pos), highlight(hl) {
			}

			std::size_t
				position = 0, ///< The position of this event.
				/// The highlight index, or \ref language_configuration::no_associated_theme.
				highlight = editors::theme_configuration::no_associated_theme;
		};

		/// Creates a new iterator for the given interpretation.
		highlight_iterator(
			const TSInput &input, const editors::code::interpretation &interp,
			const parser_ptr &parser, const language_configuration &lang,
			std::function<const language_configuration*(std::u8string_view)> lang_callback,
			std::size_t *cancellation_token
		) : _lang_callback(std::move(lang_callback)), _interp(interp), _cancellation_token(cancellation_token) {
			_layers = highlight_layer_iterator::process_layers(
				{}, input, _interp, parser, lang, _lang_callback, 0, cancellation_token
			);
		}

		/// Produces the next event.
		[[nodiscard]] std::optional<event> next(const TSInput&, const parser_ptr&);
	protected:
		/// Returns the key used for sorting layers. The first element is the position of the next event, the second
		/// indicates if the event is the end of a highlight region, and the last element is the negative depth of
		/// the layer.
		[[nodiscard]] inline static std::tuple<uint32_t, bool, int> _get_sort_key(
			highlight_layer_iterator&, const editors::code::interpretation&
		);

		std::vector<highlight_layer_iterator> _layers; ///< Iterators over highlight layers.
		/// A function that returns the \ref language_configuration that corresponds to a given language name.
		std::function<const language_configuration*(std::u8string_view)> _lang_callback;
		/// The interpretation associated with this iterator.
		const editors::code::interpretation &_interp;
		std::size_t
			/// Starting position of the previous highlight region.
			_last_highlight_begin = std::numeric_limits<std::size_t>::max(),
			/// Past-the-end position of the previous highlight region.
			_last_highlight_end = std::numeric_limits<std::size_t>::max(),
			/// Depth of the previous highlight region.
			_last_highlight_depth = std::numeric_limits<std::size_t>::max(),
			_byte_position = 0, ///< Current position.
			_iterations = 0; ///< The number of iterations. Used when checking whether to cancel the operation.
		std::size_t *_cancellation_token = nullptr; ///< Cancellation token.
	};
}
