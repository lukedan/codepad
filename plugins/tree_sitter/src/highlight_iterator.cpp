// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/tree_sitter/highlight_iterator.h"

/// \file
/// Implementation of iterators used to go through highlighted regions. This is basically a transcript of the Rust
/// version.

namespace codepad::tree_sitter {
	std::optional<highlight_iterator::event> highlight_iterator::next(
		const TSInput &input, const parser_ptr &parser
	) {
		while (true) {
			// check for cancellation
			if (_cancellation_token) {
				if (++_iterations >= cancellation_check_interval) {
					_iterations = 0;
					std::atomic_ref<std::size_t> cancel(*_cancellation_token);
					if (cancel != 0) {
						return std::nullopt;
					}
				}
			}

			// remove empty layers
			for (std::size_t i = 0; i < _layers.size(); ) {
				if (_layers[i].has_ended(_interp)) {
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
				auto min_key = _get_sort_key(_layers[0], _interp);
				for (std::size_t i = 1; i < _layers.size(); ++i) {
					auto key = _get_sort_key(_layers[i], _interp);
					if (key < min_key) {
						min_index = i;
						min_key = key;
					}
				}
			}

			auto &layer = _layers[min_index];
			auto &layer_lang = layer.get_language();

			uint32_t range_begin, range_end;
			if (auto &match = layer.peek_capture(_interp)) {
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

			capture match = layer.next_capture(_interp).value();
			TSQueryCapture cur_capture = match.match.captures[match.capture_index];

			// handle injections
			if (match.match.pattern_index < layer_lang.get_locals_pattern_index()) {
				auto inj = injection::from_match(
					match.match, layer_lang, layer_lang.get_query(), _interp
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
								std::move(ranges), input, _interp, parser,
								*new_lang, _lang_callback, layer.get_depth() + 1, _cancellation_token
							);
							for (auto &l : new_layers) {
								_layers.emplace_back(std::move(l));
							}
						}
					}
				}

				continue;
			}

			std::size_t reference_highlight_index = editors::theme_configuration::no_associated_theme;
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

					reference_highlight_index = editors::theme_configuration::no_associated_theme;
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
						.name = get_source_for_range(range_begin, range_end, _interp),
						.value_range_begin = val_range_begin,
						.value_range_end = val_range_end
						});
					definition_highlight_index = &cur_scope.locals.back().highlight;
				} else if (cur_capture.index == layer_lang.get_local_reference_capture_index()) {
					// the capture is a reference to a local definition
					if (definition_highlight_index == nullptr) { // not already a definition
						std::u8string def_name = get_source_for_range(range_begin, range_end, _interp);
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
				if (auto &next_match = layer.peek_capture(_interp)) {
					auto &next_capture = next_match->match.captures[next_match->capture_index];
					if (std::memcmp(&next_capture.node, &cur_capture.node, sizeof(TSNode)) == 0) {
						match = layer.next_capture(_interp).value();
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
				reference_highlight_index != editors::theme_configuration::no_associated_theme
			) {
				// the current node is a local definition
				// skip over highlight patterns that are disabled for local variables
				while (layer_lang.get_non_local_variable_patterns()[match.capture_index]) {
					if (auto &next_match = layer.peek_capture(_interp)) {
						auto &next_capture = next_match->match.captures[next_match->capture_index];
						if (std::memcmp(&next_capture.node, &cur_capture.node, sizeof(TSNode)) == 0) {
							// skip the match
							match = layer.next_capture(_interp).value();
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
				if (auto &next_match = layer.peek_capture(_interp)) {
					auto &next_capture = next_match->match.captures[next_match->capture_index];
					if (std::memcmp(&next_capture.node, &cur_capture.node, sizeof(TSNode)) == 0) {
						layer.next_capture(_interp);
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
			if (highlight == editors::theme_configuration::no_associated_theme) {
				highlight = current_highlight;
			}
			if (highlight != editors::theme_configuration::no_associated_theme) {
				// start the scope
				_last_highlight_begin = range_begin;
				_last_highlight_end = range_end;
				_last_highlight_depth = layer.get_depth();
				_byte_position = range_begin;

				layer._highlight_end_stack.emplace_back(range_end);
				return event(_byte_position, highlight, cur_capture.index);
			}
		}
	}

	std::tuple<uint32_t, bool, int> highlight_iterator::_get_sort_key(
		highlight_layer_iterator &it, const editors::code::interpretation &interp
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
}
