// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of iterators used to go through highlighted regions.

#include <optional>
#include <queue>

#include <tree_sitter/api.h>

#include "highlight_layer_iterator.h"

namespace codepad::tree_sitter {
	/// Iterates through highlighted regions in a given piece of code.
	class highlight_iterator {
	public:
		/// Used to store an event during highlighting, indicating a highlight boundary.
		struct event {
			/// Default constructor.
			event() = default;
			explicit event(std::size_t pos, std::size_t hl = highlight_configuration::no_associated_theme)
				: position(pos), highlight(hl) {
			}

			std::size_t
				position = 0, ///< The position of this event.
				/// The highlight index, or \ref language_configuration::no_associated_theme.
				highlight = highlight_configuration::no_associated_theme;
		};

		/// Creates a new iterator for the given interpretation.
		explicit highlight_iterator(
			const TSInput &input, const codepad::editors::code::interpretation &interp, const parser_ptr &parser,
			const language_configuration &lang,
			std::function<const language_configuration*(std::u8string_view)> lang_callback
		) : _lang_callback(std::move(lang_callback)), _interp(&interp) {
			_layers = highlight_layer_iterator::process_layers({}, input, *_interp, parser, lang, 0, _lang_callback);
		}

		/// Produces the next event.
		std::optional<event> next(
			const TSInput &input, const parser_ptr &parser
		) {
			while (true) {
				// remove empty layers
				for (std::size_t i = 0; i < _layers.size(); ) {
					if (_layers[i].has_ended(*_interp)) {
						if (i + 1 < _layers.size()) {
							std::swap(_layers[i], _layers.back());
						}
						_layers.pop_back();
					} else {
						++i;
					}
				}

				if (_layers.empty()) {
					return std::nullopt;
				}

				// find the layer with the smallest sort key
				std::size_t min_index = 0;
				{
					auto min_key = _get_sort_key(_layers[0], *_interp);
					for (std::size_t i = 1; i < _layers.size(); ++i) {
						auto key = _get_sort_key(_layers[i], *_interp);
						if (key < min_key) {
							min_index = i;
							min_key = key;
						}
					}
				}

				auto &layer = _layers[min_index];
				auto &layer_lang = layer.get_language();

				uint32_t range_begin, range_end;
				if (auto &match = layer.peek_capture(*_interp)) {
					auto &cur_capture = match->match.captures[match->capture_index];
					range_begin = ts_node_start_byte(cur_capture.node);
					range_end = ts_node_end_byte(cur_capture.node);

					if (!layer._highlight_end_stack.empty()) {
						std::size_t end_byte = layer._highlight_end_stack.back();
						if (end_byte <= range_begin) {
							layer._highlight_end_stack.pop_back();
							_byte_position = end_byte;
							return event(_byte_position);
						}
					}
				} else {
					if (!layer._highlight_end_stack.empty()) {
						_byte_position = layer._highlight_end_stack.back();
						layer._highlight_end_stack.pop_back();
						return event(_byte_position);
					}
					return std::nullopt;
				}

				capture match = layer.next_capture(*_interp).value();
				TSQueryCapture cur_capture = match.match.captures[match.capture_index];

				// handle injections
				if (match.match.pattern_index < layer_lang.get_locals_pattern_index()) {
					auto inj = injection::from_match(
						match.match, layer_lang, layer_lang.get_query(), *_interp
					);
					layer.remove_match(match.match);

					if (!inj.language.empty() && inj.node.has_value()) {
						if (const language_configuration *new_lang = _lang_callback(inj.language)) {
							std::vector<TSNode> nodes{ inj.node.value() };
							std::vector<TSRange> ranges = highlight_layer_iterator::intersect_ranges(
								layer.get_ranges(), nodes, inj.include_children
							);
							if (!ranges.empty()) {
								auto new_layers = highlight_layer_iterator::process_layers(
									std::move(ranges), input, *_interp, parser,
									*new_lang, layer.get_depth() + 1, _lang_callback
								);
								for (auto &l : new_layers) {
									_layers.emplace_back(std::move(l));
								}
							}
						}
					}

					continue;
				}

				std::size_t reference_highlight_index = highlight_configuration::no_associated_theme;
				std::size_t *definition_highlight_index = nullptr;
				// pop ended ranges
				while (range_begin > layer._scope_stack.back().range_end) {
					layer._scope_stack.pop_back();
				}

				// process ranges for tracking local variables
				bool continue_main = false;
				while (match.match.pattern_index < layer_lang.get_highlights_pattern_index()) {
					if (cur_capture.index == layer_lang.get_local_scope_capture_index()) {
						// the capture is a local scope

						definition_highlight_index = nullptr;

						local_scope scope(range_begin, range_end);
						auto &props = layer_lang.get_query().get_property_settings()[
							match.match.pattern_index
						];
						for (auto &prop : props) {
							if (prop.key == u8"local.scope-inherits") {
								if (prop.value.empty() || prop.value == u8"true") {
									scope.scope_inherits = true;
								}
							}
						}
						layer._scope_stack.emplace_back(std::move(scope));
					} else if (cur_capture.index == layer_lang.get_local_definition_capture_index()) {
						// the capture is a definition

						reference_highlight_index = highlight_configuration::no_associated_theme;
						definition_highlight_index = nullptr;

						local_scope &cur_scope = layer._scope_stack.back();
						// get the range of the value, if any
						uint32_t val_range_begin = 0, val_range_end = 0;
						for (uint16_t i = 0; i < match.match.capture_count; ++i) {
							auto &capture = match.match.captures[i];
							if (capture.index == layer_lang.get_local_definition_value_capture_index()) {
								val_range_begin = ts_node_start_byte(capture.node);
								val_range_end = ts_node_end_byte(capture.node);
								// FIXME maybe we should just break here?
							}
						}
						cur_scope.locals.emplace_back(local_definition{
							.name = get_source_for_range(range_begin, range_end, *_interp),
							.value_range_begin = val_range_begin,
							.value_range_end = val_range_end
						});
						definition_highlight_index = &cur_scope.locals.back().highlight;
					} else if (cur_capture.index == layer_lang.get_local_reference_capture_index()) {
						// the capture is a reference to a local definition
						if (definition_highlight_index == nullptr) { // not already a definition
							std::u8string def_name = get_source_for_range(range_begin, range_end, *_interp);
							for (
								auto scope_it = layer._scope_stack.rbegin();
								scope_it != layer._scope_stack.rend();
								++scope_it
							) {
								bool break_outer = false;
								for (
									auto def_it = scope_it->locals.rbegin();
									def_it != scope_it->locals.rend();
									++def_it
								) {
									if (def_it->name == def_name && range_begin >= def_it->value_range_end) {
										// found the definition
										reference_highlight_index = def_it->highlight;
										// break both loops
										break_outer = true;
										break;
									}
								}
								if (break_outer || !scope_it->scope_inherits) {
									break;
								}
							}
						}
					}

					// peek and continue processing if the next capture is the same node
					if (auto &next_match = layer.peek_capture(*_interp)) {
						auto &next_capture = next_match->match.captures[next_match->capture_index];
						if (std::memcmp(&next_capture.node, &cur_capture.node, sizeof(TSNode)) == 0) {
							match = layer.next_capture(*_interp).value();
							cur_capture = match.match.captures[match.capture_index];
							continue;
						}
					}

					// continue the outmost loop
					continue_main = true;
					break;
				}
				if (continue_main) {
					continue;
				}

				// otherwise the capture must represent a highlight
				if (
					range_begin == _last_highlight_begin &&
					range_end == _last_highlight_end &&
					layer.get_depth() < _last_highlight_depth
				) {
					continue;
				}

				if (
					definition_highlight_index ||
					reference_highlight_index != highlight_configuration::no_associated_theme
				) {
					// the current node is a local definition
					// skip over highlight patterns that are disabled for local variables
					while (layer_lang.get_non_local_variable_patterns()[match.capture_index]) {
						if (auto &next_match = layer.peek_capture(*_interp)) {
							auto &next_capture = next_match->match.captures[next_match->capture_index];
							if (std::memcmp(&next_capture.node, &cur_capture.node, sizeof(TSNode)) == 0) {
								// skip the match
								match = layer.next_capture(*_interp).value();
								cur_capture = match.match.captures[match.capture_index];
								continue;
							}
						}

						continue_main = true;
						break;
					}
					if (continue_main) {
						continue;
					}
				}

				// skip all highlights for the same node
				do {
					if (auto &next_match = layer.peek_capture(*_interp)) {
						auto &next_capture = next_match->match.captures[next_match->capture_index];
						if (std::memcmp(&next_capture.node, &cur_capture.node, sizeof(TSNode)) == 0) {
							layer.next_capture(*_interp);
							continue;
						}
					}
				} while (false); // break otherwise

				std::size_t current_highlight = layer_lang.get_capture_highlight_indices()[cur_capture.index];

				// store highlight index for local definition
				if (definition_highlight_index) {
					*definition_highlight_index = current_highlight;
				}

				std::size_t highlight = reference_highlight_index;
				if (highlight == highlight_configuration::no_associated_theme) {
					highlight = current_highlight;
				}
				if (highlight != highlight_configuration::no_associated_theme) {
					// start the scope
					_last_highlight_begin = range_begin;
					_last_highlight_end = range_end;
					_last_highlight_depth = layer.get_depth();
					_byte_position = range_begin;

					layer._highlight_end_stack.emplace_back(range_end);
					return event(_byte_position, highlight);
				}
			}
		}
	protected:
		/// Returns the key used for sorting layers. The first element is the position of the next event, the second
		/// indicates if the event is the end of a highlight region, and the last element is the negative depth of
		/// the layer.
		[[nodiscard]] inline static std::tuple<uint32_t, bool, int> _get_sort_key(
			highlight_layer_iterator &it, const codepad::editors::code::interpretation &interp
		) {
			std::optional<uint32_t> next_start, next_end;
			if (auto &capture = it.peek_capture(interp)) {
				next_start.emplace(ts_node_start_byte(capture->match.captures[capture->capture_index].node));
			}
			if (!it._highlight_end_stack.empty()) {
				next_end.emplace(it._highlight_end_stack.back());
			}

			int depth = -static_cast<int>(it.get_depth());
			if (next_start && next_end) {
				if (next_start.value() < next_end.value()) {
					return { next_start.value(), true, depth };
				} else {
					return { next_end.value(), false, depth };
				}
			}
			if (next_start) {
				return { next_start.value(), true, depth };
			}
			return { next_end.value(), true, depth };
		}

		std::vector<highlight_layer_iterator> _layers; ///< Iterators over highlight layers.
		/// A function that returns the \ref language_configuration that corresponds to a given language name.
		std::function<const language_configuration*(std::u8string_view)> _lang_callback;
		/// The interpretation associated with this iterator.
		const codepad::editors::code::interpretation *_interp = nullptr;
		std::size_t
			/// Starting position of the previous highlight region.
			_last_highlight_begin = std::numeric_limits<std::size_t>::max(),
			/// Past-the-end position of the previous highlight region.
			_last_highlight_end = std::numeric_limits<std::size_t>::max(),
			/// Depth of the previous highlight region.
			_last_highlight_depth = std::numeric_limits<std::size_t>::max(),
			_byte_position = 0; ///< Current position.
	};
}
