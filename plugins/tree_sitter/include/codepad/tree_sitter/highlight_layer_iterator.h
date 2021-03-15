// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Iterators used for highlighting a clip of code in the same language. This is basically a transcript of the Rust
/// version.

#include <vector>
#include <deque>

#include <tree_sitter/api.h>

#include <codepad/editors/code/interpretation.h>

#include "wrappers.h"
#include "language_configuration.h"

namespace codepad::tree_sitter {
	/// Returns the range for the node.
	[[nodiscard]] inline TSRange get_range_for_node(const TSNode &node) {
		return TSRange{
			.start_point = ts_node_start_point(node),
			.end_point = ts_node_end_point(node),
			.start_byte = ts_node_start_byte(node),
			.end_byte = ts_node_end_byte(node)
		};
	}

	/// Returns the source code corresponding to the given byte range, decoded and re-encoded as UTF-8, with all
	/// invalid codepoints replaced by the replacement character.
	[[nodiscard]] inline std::u8string get_source_for_range(
		uint32_t first_byte, uint32_t past_last_byte, const codepad::editors::code::interpretation &interp
	) {
		codepad::editors::byte_string raw_str = interp.get_buffer().get_clip(
			interp.get_buffer().at(static_cast<std::size_t>(first_byte)),
			interp.get_buffer().at(static_cast<std::size_t>(past_last_byte))
		);
		// decode & re-encode using utf8
		std::u8string result;
		// since UTF-8 is relatively space efficient, this should be a good guess
		result.reserve(raw_str.size());
		const std::byte *it = raw_str.data(), *end = raw_str.data() + raw_str.size();
		while (it != end) {
			codepad::codepoint cp;
			if (!interp.get_encoding()->next_codepoint(it, end, cp)) {
				cp = codepad::encodings::replacement_character;
			}
			auto str = interp.get_encoding()->encode_codepoint(cp);
			result.append(reinterpret_cast<const char8_t*>(str.data()), str.size());
		}
		return result;
	}
	/// Returns the source code corresponding to the given node using \ref get_source_for_range().
	[[nodiscard]] inline std::u8string get_source_for_node(
		const TSNode &node, const codepad::editors::code::interpretation &interp
	) {
		return get_source_for_range(ts_node_start_byte(node), ts_node_end_byte(node), interp);
	}

	/// Stores information about an injection - a piece of code in a file that uses a different language.
	struct injection {
		std::u8string language; ///< The language of this injection.
		std::optional<TSNode> node; ///< The node that corresponds to this injection.
		/// Whether the \p injection.include-children capture is present.
		/// \sa https://tree-sitter.github.io/tree-sitter/syntax-highlighting#language-injection
		bool include_children = false;

		/// Extracts injection information from a \p TSQueryMatch.
		[[nodiscard]] static injection from_match(
			const TSQueryMatch&, const language_configuration&,
			const query&, const codepad::editors::code::interpretation&
		);
	};

	/// An injection that is composed of multiple regions in a source file.
	struct combined_injection {
		std::u8string language; ///< The language of this combined injection.
		std::vector<TSNode> nodes; ///< Nodes that correspond to this injection.
		bool include_children = false; ///< \sa injection::include_children

		/// Adds the given injection to this combined injection.
		void append(injection inj) {
			if (!inj.language.empty()) {
				if (language.empty()) {
					language = std::move(inj.language);
				} else {
					if (language != inj.language) {
						codepad::logger::get().log_warning(CP_HERE) <<
							"languages of combined injections don't agree; got " <<
							inj.language << " and " << language;
					}
				}
			}
			if (inj.node) {
				nodes.emplace_back(inj.node.value());
			}
			include_children = inj.include_children;
		}
	};

	/// Stores information about a local definition.
	struct local_definition {
		std::u8string name; ///< The name of this definition.
		uint32_t
			value_range_begin = 0, ///< Beginning byte of the value.
			value_range_end = 0; ///< Past-the-end position of the value.
		std::size_t highlight; ///< Index of the highlight applied to this definition.
	};

	/// Stores information about a local scope.
	struct local_scope {
		/// Default constructor.
		local_scope() = default;
		/// Initializes the range of this scope.
		local_scope(uint32_t rbeg, uint32_t rend) : range_begin(rbeg), range_end(rend) {
		}

		/// Returns the global scope.
		[[nodiscard]] inline static local_scope global() {
			return local_scope(0, std::numeric_limits<uint32_t>::max());
		}

		std::vector<local_definition> locals; ///< All local definitions.
		uint32_t
			range_begin = 0, ///< Beginning position of this scope.
			range_end = 0; ///< Past-the-end position of this scope.
		bool scope_inherits = false; ///< Indicates whether <tt>local.scope-inherits</tt> is \p true.
	};

	/// Information about a capture.
	struct capture {
		TSQueryMatch match; ///< The pattern match that contains this capture.
		uint32_t capture_index = 0; ///< The index the capture in \ref match.
	};

	class highlight_iterator;
	/// Used to iterate though highlights in a particular layer.
	class highlight_layer_iterator {
		friend highlight_iterator;
	protected:
		/// Information about a combined injection.
		struct _layer_info {
			std::vector<TSRange> ranges; ///< Ranges of this combined injection.
			const language_configuration *lang_config = nullptr; ///< The language configuration.
			std::size_t depth = 0; ///< Depth of this injection.
		};
	public:
		/// Removes the given match.
		void remove_match(const TSQueryMatch &match) {
			ts_query_cursor_remove_match(_cursor.get(), match.id);
		}
		/// Returns the next capture and advances the iterator.
		std::optional<capture> next_capture(const codepad::editors::code::interpretation &interp) {
			if (_peek) {
				capture cp = std::move(_peek.value());
				_peek.reset();
				return cp;
			}
			return _next_capture_impl(interp);
		}
		/// Returns the next capture without advancing the iterator.
		[[nodiscard]] const std::optional<capture> &peek_capture(
			const codepad::editors::code::interpretation &interp
		) {
			if (!_peek) {
				_peek = _next_capture_impl(interp);
			}
			return _peek;
		}

		/// Computes ranges of an injection given the parent ranges, nodes of the injection, and whether to include
		/// the children of the nodes. See the rust implementation for more details.
		[[nodiscard]] static std::vector<TSRange> intersect_ranges(
			const std::vector<TSRange> parent_ranges, const std::vector<TSNode> &nodes, bool include_children
		);

		/// Processes the given source code. If there are any combined injections in the source code, this function
		/// eagerly produces layer iterators for them as well. Normal injections are not handled here.
		[[nodiscard]] static std::vector<highlight_layer_iterator> process_layers(
			std::vector<TSRange> ranges, const TSInput&, const codepad::editors::code::interpretation&,
			const parser_ptr&, const language_configuration&,
			const std::function<const language_configuration*(std::u8string_view)> &lang_callback,
			std::size_t depth, const std::size_t *cancellation_token
		);

		/// Returns whether this layer has ended, i.e., whether there are no more captures and
		/// \ref _highlight_end_stack is empty.
		[[nodiscard]] bool has_ended(const codepad::editors::code::interpretation &interp) {
			return _highlight_end_stack.empty() && !peek_capture(interp).has_value();
		}

		/// Returns the ranges of this layer.
		[[nodiscard]] const std::vector<TSRange> &get_ranges() const {
			return _ranges;
		}
		/// Returns the depth of this layer.
		[[nodiscard]] std::size_t get_depth() const {
			return _depth;
		}
		/// Returns the associated \ref language_configuration.
		[[nodiscard]] const language_configuration &get_language() const {
			return *_language;
		}
	protected:
		std::vector<local_scope> _scope_stack{local_scope::global()}; ///< A stack of local scopes.
		std::vector<uint32_t> _highlight_end_stack; ///< Stack of highlight end boundaries.
		std::vector<TSRange> _ranges; ///< Ranges that are in this layer.
		std::optional<capture> _peek; ///< The next capture for peeking.
		query_cursor_ptr _cursor; ///< The cursor used to execute queries.
		tree_ptr _tree; ///< The syntax tree of this layer.
		const language_configuration *_language = nullptr; ///< The language configuration.
		std::size_t _depth = 0; ///< The depth of this layer.

		/// Initializes all fields of this class, and sets up \ref _cursor to iterate through all captures.
		highlight_layer_iterator(
			std::vector<TSRange> ranges, query_cursor_ptr cursor, tree_ptr tree,
			const language_configuration *lang, std::size_t depth
		) :
			_ranges(std::move(ranges)), _cursor(std::move(cursor)), _tree(std::move(tree)),
			_language(lang), _depth(depth) {

			ts_query_cursor_exec(
				_cursor.get(), _language->get_query().get_query().get(), ts_tree_root_node(_tree.get())
			);
		}

		/// Returns the next capture and advances this iterator. Does not handle anything peek-related.
		[[nodiscard]] std::optional<capture> _next_capture_impl(
			const codepad::editors::code::interpretation &interp
		) const {
			while (true) {
				capture result;
				if (ts_query_cursor_next_capture(_cursor.get(), &result.match, &result.capture_index)) {
					bool good = _language->get_query().satisfies_text_predicates(
						result.match,
						[&interp](const TSNode &node) {
							return get_source_for_node(node, interp);
						}
					);
					if (good) {
						return result;
					} else {
						ts_query_cursor_remove_match(_cursor.get(), result.match.id);
					}
				} else {
					return std::nullopt;
				}
			}
		}
	};
}
