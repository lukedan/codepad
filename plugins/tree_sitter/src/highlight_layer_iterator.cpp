// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/tree_sitter/highlight_layer_iterator.h"

/// \file
/// Implementation of layer iterators.

namespace codepad::tree_sitter {
	injection injection::from_match(
		const TSQueryMatch &match, const language_configuration &config,
		const query &q, const codepad::editors::code::interpretation &src
	) {
		uint32_t
			injection_content = config.get_injection_content_capture_index(),
			injection_language = config.get_injection_language_capture_index();
		injection result;
		// extract language and node of injection
		for (uint16_t i = 0; i < match.capture_count; ++i) {
			const TSQueryCapture &capture = match.captures[i];
			if (capture.index == injection_language) {
				result.language = get_source_for_node(capture.node, src);
			} else if (capture.index == injection_content) {
				result.node = capture.node;
			}
		}
		// extract settings hard-coded in the highlight files
		for (const query::property &prop : q.get_property_settings()[match.pattern_index]) {
			if (prop.key == u8"injection.language") {
				if (result.language.empty()) {
					result.language = prop.value;
				}
			} else if (prop.key == u8"injection.include-children") {
				result.include_children = true;
			}
		}
		return result;
	}


	std::vector<TSRange> highlight_layer_iterator::intersect_ranges(
		const std::vector<TSRange> parent_ranges, const std::vector<TSNode> &nodes, bool include_children
	) {
		constexpr uint32_t _uint32_max = std::numeric_limits<uint32_t>::max();
		const TSRange _full_range{
			.start_point = {.row = 0, .column = 0 },
			.end_point = {.row = _uint32_max, .column = _uint32_max },
			.start_byte = 0,
			.end_byte = _uint32_max
		};

		const TSRange *parent_cur, *parent_end;
		if (parent_ranges.empty()) {
			parent_cur = &_full_range;
			parent_end = parent_cur + 1;
		} else {
			parent_cur = parent_ranges.data();
			parent_end = parent_cur + parent_ranges.size();
		}

		std::vector<TSRange> result;
		for (const TSNode &node : nodes) {
			TSRange prev_exclude{
				.start_point = {.row = 0, .column = 0 },
				.end_point = ts_node_start_point(node),
				.start_byte = 0,
				.end_byte = ts_node_start_byte(node)
			};
			TSRange range_after{
				.start_point = ts_node_end_point(node),
				.end_point = {.row = _uint32_max, .column = _uint32_max },
				.start_byte = ts_node_end_byte(node),
				.end_byte = _uint32_max
			};

			std::optional<TSTreeCursor> cursor;
			TSRange excluded_range = range_after;
			if (!include_children) {
				cursor.emplace(ts_tree_cursor_new(node));
				if (ts_tree_cursor_goto_first_child(&cursor.value())) {
					excluded_range = get_range_for_node(ts_tree_cursor_current_node(&cursor.value()));
				} else {
					ts_tree_cursor_delete(&cursor.value());
					cursor.reset();
				}
			}
			while (true) { // loop through all child ranges of this node
				TSRange range{
					.start_point = prev_exclude.end_point,
					.end_point = excluded_range.start_point,
					.start_byte = prev_exclude.end_byte,
					.end_byte = excluded_range.start_byte
				};
				prev_exclude = excluded_range;

				// loop through all parent ranges that intersect with this range
				while (range.end_byte >= parent_cur->start_byte) {
					if (range.start_byte < parent_cur->end_byte) { // range intersects with parent_cur
						// make sure the start of range is contained by parent
						if (range.start_byte < parent_cur->start_byte) {
							range.start_byte = parent_cur->start_byte;
							range.start_point = parent_cur->start_point;
						}

						if (parent_cur->end_byte < range.end_byte) {
							if (range.start_byte < parent_cur->end_byte) {
								result.emplace_back(TSRange{
									.start_point = range.start_point,
									.end_point = parent_cur->end_point,
									.start_byte = range.start_byte,
									.end_byte = parent_cur->end_byte
									});
							}
							range.start_point = parent_cur->end_point;
							range.start_byte = parent_cur->end_byte;
						} else { // range is completely contained by parent_cur
							if (range.start_byte < range.end_byte) {
								result.emplace_back(range);
							}
							break;
						}
					}

					// move to next parent range
					if (++parent_cur == parent_end) {
						return result;
					}
				}

				if (cursor) { // move to the next child or excluded_range
					if (!ts_tree_cursor_goto_next_sibling(&cursor.value())) {
						ts_tree_cursor_delete(&cursor.value());
						cursor.reset();
						excluded_range = range_after;
					} else {
						excluded_range = get_range_for_node(ts_tree_cursor_current_node(&cursor.value()));
					}
				} else { // if cursor is empty, we have processed excluded_range and should just return
					break;
				}
			}
		}
		return result;
	}

	std::vector<highlight_layer_iterator> highlight_layer_iterator::process_layers(
		std::vector<TSRange> ranges, const TSInput &input, const editors::code::interpretation &interp,
		const parser_ptr &parser, const language_configuration &lang_config,
		const std::function<const language_configuration*(std::u8string_view)> &lang_callback,
		std::size_t depth, const std::size_t *cancellation_token
	) {
		constexpr uint32_t _uint32_max = std::numeric_limits<uint32_t>::max();

		std::vector<highlight_layer_iterator> result;
		std::deque<_layer_info> queue;
		// for ts_parser_set_included_ranges, if length == 0, the entire document is parsed
		queue.emplace_back(_layer_info{
			.ranges = std::move(ranges), .lang_config = &lang_config, .depth = depth
			});
		while (!queue.empty()) {
			_layer_info cur_layer = std::move(queue.front());
			queue.pop_front();

			if (!ts_parser_set_included_ranges(
				parser.get(), cur_layer.ranges.data(), static_cast<uint32_t>(cur_layer.ranges.size())
			)) {
				logger::get().log_error(CP_HERE) << "failed to parse document: invalid ranges";
				break;
			}
			if (!ts_parser_set_language(parser.get(), cur_layer.lang_config->get_language())) {
				logger::get().log_error(CP_HERE) <<
					"failed to parse document: language version mismatch";
				break;
			}
			// TODO maybe reuse the old tree
			ts_parser_set_cancellation_flag(parser.get(), cancellation_token);
			tree_ptr tree(ts_parser_parse(parser.get(), nullptr, input));
			ts_parser_set_cancellation_flag(parser.get(), nullptr);
			if (!tree) {
				break; // operation was cancelled
			}

			// process combined injections
			// TODO the rust highlighter implements some kind of cursor reusing mechanism
			//      we'll see if that's necessary
			query_cursor_ptr cursor(ts_query_cursor_new());
			if (const query &combined_injections = cur_layer.lang_config->get_combined_injections_query()) {
				std::vector<combined_injection>
					injections_by_pattern_index(combined_injections.get_num_patterns());
				combined_injections.pattern_matches(
					cursor.get(), ts_tree_root_node(tree.get()),
					[&interp](const TSNode &node) {
						return get_source_for_node(node, interp);
					},
					[&](const TSQueryMatch &match) {
						injections_by_pattern_index[match.pattern_index].append(
							injection::from_match(match, lang_config, combined_injections, interp)
						);
						return true;
					}
					);
				for (const combined_injection &inj : injections_by_pattern_index) {
					if (inj.language.empty() || inj.nodes.empty()) {
						continue;
					}
					if (const language_configuration *new_cfg = lang_callback(inj.language)) {
						auto ranges = intersect_ranges(cur_layer.ranges, inj.nodes, inj.include_children);
						queue.emplace_back(_layer_info{
							.ranges = std::move(ranges), .lang_config = new_cfg, .depth = cur_layer.depth + 1
						});
					}
				}
			}

			result.emplace_back(highlight_layer_iterator(
				std::move(cur_layer.ranges), std::move(cursor), std::move(tree),
				cur_layer.lang_config, cur_layer.depth
			));
		}
		return result;
	}
}
