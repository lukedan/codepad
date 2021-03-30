// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/tree_sitter/highlight_collector.h"

/// \file
/// Implementation of iterators used to go through highlighted regions. This is basically a transcript of the Rust
/// version.

namespace codepad::tree_sitter {
	[[nodiscard]] highlight_collector::document_highlight_data highlight_collector::compute(
		const parser_ptr &parser
	) {
		document_highlight_data result;
		while (!_layers.empty()) {
			if (_check_cancel()) {
				break;
			}
			compute_for_layer(result, std::move(_layers.front()), parser);
			_layers.pop_front();
		}
		return result;
	}

	void highlight_collector::compute_for_layer(
		document_highlight_data &out, highlight_layer_iterator layer, const parser_ptr &parser
	) {
		uint32_t last_queried_pos = 0;
		editors::code::interpretation::character_position_converter conv(_interp);
		auto byte_to_char = [&](uint32_t pos) {
			if (pos < last_queried_pos) {
				conv.reset();
			}
			last_queried_pos = pos;
			return conv.byte_to_character(pos);
		};

		// add all capture names of the current language to `out.capture_names`
		std::size_t first_name = out.capture_names.size();
		for (auto name : layer.get_language().get_query().get_captures()) {
			out.capture_names.emplace_back(
				u8"[" + layer.get_language().get_language_name() + u8"]" + std::u8string(name)
			);
		}

		while (auto match = layer.next_capture(_interp)) {
			// check for cancellation
			if (_check_cancel()) {
				return;
			}

			auto &layer_lang = layer.get_language();
			TSQueryCapture cur_capture = match->match.captures[match->capture_index];
			uint32_t
				range_begin = ts_node_start_byte(cur_capture.node),
				range_end = ts_node_end_byte(cur_capture.node);

			// handle injections
			if (match->match.pattern_index < layer_lang.get_locals_pattern_index()) {
				auto inj = injection::from_match(
					match->match, layer_lang, layer_lang.get_query(), _interp
				);
				layer.remove_match(match->match);

				if (!inj.language.empty() && inj.node.has_value()) {
					if (const language_configuration *new_lang = _lang_callback(inj.language)) {
						std::vector<TSNode> nodes{ inj.node.value() };
						std::vector<TSRange> ranges = highlight_layer_iterator::intersect_ranges(
							layer.get_ranges(), nodes, inj.include_children
						);
						if (!ranges.empty()) {
							auto new_layers = highlight_layer_iterator::process_layers(
								std::move(ranges), _input, _interp, parser,
								*new_lang, _lang_callback, _cancellation_token
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
			while (match->match.pattern_index < layer_lang.get_highlights_pattern_index()) {
				if (cur_capture.index == layer_lang.get_local_scope_capture_index()) {
					// the capture is a local scope

					definition_highlight_index = nullptr;

					local_scope scope(range_begin, range_end);
					auto &props = layer_lang.get_query().get_property_settings()[
						match->match.pattern_index
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
					for (uint16_t i = 0; i < match->match.capture_count; ++i) {
						auto &capture = match->match.captures[i];
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
						cur_capture = match->match.captures[match->capture_index];
						continue;
					}
				}

				// otherwise we don't care about this capture; continue the outmost loop
				continue_main = true;
				break;
			}
			if (continue_main) {
				continue;
			}

			// otherwise the capture must represent a highlight
			if (
				definition_highlight_index ||
				reference_highlight_index != editors::theme_configuration::no_associated_theme
			) {
				// the current node is a local definition; skip over highlight patterns that are disabled for local
				// variables
				while (layer_lang.get_non_local_variable_patterns()[match->capture_index]) {
					if (auto &next_match = layer.peek_capture(_interp)) {
						auto &next_capture = next_match->match.captures[next_match->capture_index];
						if (std::memcmp(&next_capture.node, &cur_capture.node, sizeof(TSNode)) == 0) {
							// skip the match
							match = layer.next_capture(_interp).value();
							cur_capture = match->match.captures[match->capture_index];
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
			while (auto &next_match = layer.peek_capture(_interp)) {
				auto &next_capture = next_match->match.captures[next_match->capture_index];
				if (std::memcmp(&next_capture.node, &cur_capture.node, sizeof(TSNode)) != 0) {
					break; // not the same node
				}
				layer.next_capture(_interp);
			}

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
				// add the range to `out`
				std::size_t start_char = byte_to_char(range_begin);
				std::size_t end_char = byte_to_char(range_end);
				out.theme.add_range(start_char, end_char, editors::code::document_theme::range_value(
					layer_lang.get_highlight_configuration()->entries[highlight].theme,
					first_name + cur_capture.index
				));
			}
		}
	}
}
