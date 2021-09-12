// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/core/regex/compiler.h"

/// \file
/// Regex tests using the PCRE2 test data.

namespace codepad::regex {
	namespace half_compiled {
		compiled_unoptimized::state_ref state_machine::create_state() {
			std::size_t res_idx = states.size();
			states.emplace_back();
			return compiled_unoptimized::state_ref(res_idx);
		}

		std::pair<transition_ref, compiled_unoptimized::transition&> state_machine::create_transition_from_to(
			compiled_unoptimized::state_ref from, compiled_unoptimized::state_ref to
		) {
			auto &state = states[from._index];
			std::size_t res_idx = state.transitions.size();
			auto &res = state.transitions.emplace_back();
			res.new_state = to;
			return { transition_ref(from, res_idx), res };
		}

		/// A series of functions that dump transition conditions. This function is the fallback for all unhandled
		/// transition types.
		template <typename Cond> void _dump_condition(std::ostream &out, const Cond&) {
			out << "<UNHANDLED>";
		}
		void state_machine::dump(std::ostream &stream, bool valid_only) const {
			stream << "digraph {\n";
			stream << "n" << start_state._index << "[color=red];\n";
			stream << "n" << end_state._index << "[color=blue];\n";
			auto is_printable_char = [](codepoint cp) {
				return cp >= 0x20 && cp <= 0x7E;
			};
			auto print_char = [&](codepoint cp) {
				if (is_printable_char(cp)) {
					stream << static_cast<char>(cp);
				}
				stream << "[" << std::hex << cp << std::dec << "]";
			};
			for (std::size_t i = 0; i < states.size(); ++i) {
				const auto &s = states[i];
				for (std::size_t j = 0; j < s.transitions.size(); ++j) {
					auto &t = s.transitions[j];
					stream << "n" << i << " -> n" << t.new_state._index << " [label=\"" << j << ": ";
					if (std::holds_alternative<compiled_unoptimized::transitions::literal>(t.condition)) {
						const auto &cond = std::get<compiled_unoptimized::transitions::literal>(t.condition);
						for (codepoint cp : cond.contents) {
							if (is_printable_char(cp)) {
								stream << static_cast<char>(cp);
							} else {
								stream << "?";
							}
						}
					} else if (std::holds_alternative<compiled_unoptimized::transitions::character_class>(t.condition)) {
						const auto &list = std::get<compiled_unoptimized::transitions::character_class>(t.condition);
						bool first = true;
						for (auto range : list.ranges.ranges) {
							if (first) {
								first = false;
							} else {
								stream << ",";
							}
							if (valid_only && !is_printable_char(range.first)) {
								stream << "...";
								break;
							}
							print_char(range.first);
							if (range.last != range.first) {
								stream << "-";
								print_char(range.last);
							}
						}
						if (list.is_negate) {
							stream << " <!>";
						}
					} else if (std::holds_alternative<compiled_unoptimized::transitions::simple_assertion>(t.condition)) {
						stream << "<simple assertion>";
					} else if (std::holds_alternative<compiled_unoptimized::transitions::character_class_assertion>(t.condition)) {
						stream << "<char class assertion>";
					} else if (std::holds_alternative<compiled_unoptimized::transitions::capture_begin>(t.condition)) {
						const auto &cap = std::get<compiled_unoptimized::transitions::capture_begin>(t.condition);
						stream << "<start capture " << cap.index << ">";
					} else if (std::holds_alternative<compiled_unoptimized::transitions::capture_end>(t.condition)) {
						stream << "<end capture>";
					} else if (std::holds_alternative<compiled_unoptimized::transitions::numbered_backreference>(t.condition)) {
						const auto &backref = std::get<compiled_unoptimized::transitions::numbered_backreference>(t.condition);
						stream << "<backref #" << backref.index << ">";
					} else if (std::holds_alternative<compiled_unoptimized::transitions::named_backreference>(t.condition)) {
						const auto &backref = std::get<compiled_unoptimized::transitions::named_backreference>(t.condition);
						stream << "<named backref #" << backref.index << ">";
					} else if (std::holds_alternative<compiled_unoptimized::transitions::push_atomic>(t.condition)) {
						stream << "<push atomic>";
					} else if (std::holds_alternative<compiled_unoptimized::transitions::pop_atomic>(t.condition)) {
						stream << "<pop atomic>";
					} else if (std::holds_alternative<compiled_unoptimized::transitions::push_stream_checkpoint>(t.condition)) {
						stream << "<checkpoint>";
					} else if (std::holds_alternative<compiled_unoptimized::transitions::restore_stream_checkpoint>(t.condition)) {
						stream << "<restore checkpoint>";
					} else if (std::holds_alternative<compiled_unoptimized::transitions::conditions::numbered_capture>(t.condition)) {
						const auto &cond = std::get<compiled_unoptimized::transitions::conditions::numbered_capture>(t.condition);
						stream << "<cond: capture #" << cond.index << ">";
					} else if (std::holds_alternative<compiled_unoptimized::transitions::conditions::named_capture>(t.condition)) {
						const auto &cond = std::get<compiled_unoptimized::transitions::conditions::named_capture>(t.condition);
						stream << "<cond: named capture #" << cond.name_index << ">";
					} else if (std::holds_alternative<compiled_unoptimized::transitions::conditions::numbered_recursion>(t.condition)) {
						const auto &cond = std::get<compiled_unoptimized::transitions::conditions::numbered_recursion>(t.condition);
						stream << "<cond: recursion #" << cond.index << ">";
					} else if (std::holds_alternative<compiled_unoptimized::transitions::conditions::named_recursion>(t.condition)) {
						const auto &cond = std::get<compiled_unoptimized::transitions::conditions::named_recursion>(t.condition);
						stream << "<cond: named recursion #" << cond.name_index << ">";
					} else if (std::holds_alternative<compiled_unoptimized::transitions::push_position>(t.condition)) {
						stream << "<push pos>";
					} else if (std::holds_alternative<compiled_unoptimized::transitions::check_infinite_loop>(t.condition)) {
						stream << "<check loop>";
					} else if (std::holds_alternative<compiled_unoptimized::transitions::jump>(t.condition)) {
						const auto &jmp = std::get<compiled_unoptimized::transitions::jump>(t.condition);
						stream << "<jump>\"];\n";
						// additional edge for jumping back
						stream <<
							"n" << jmp.target._index << " -> n" << jmp.return_state._index <<
							" [color=green,label=\"n" << i << " -> n" << t.new_state._index;
					} else {
						stream << "<UNHANDLED>";
					}
					stream << "\"];\n";
				}
			}
			stream << "}\n";
		}
	}


	half_compiled::state_machine compiler::compile(const ast &expr, const ast::analysis &analysis) {
		_ast = &expr;
		_analysis = &analysis;
		_result = half_compiled::state_machine();

		auto start_state = _result.create_state();
		auto end_state = _result.create_state();
		_result.start_state = start_state;
		_result.end_state = end_state;

		for (std::size_t i = 0; i < expr._nodes.size(); ++i) {
			_collect_capture_names(expr._nodes[i]);
		}

		if (!_named_captures.empty()) {
			std::sort(_named_captures.begin(), _named_captures.end());
			// collect named capture info
			codepoint_string last_name = _named_captures[0].name;
			_result.named_captures.start_indices.emplace_back(0);
			for (auto &cap : _named_captures) {
				if (cap.name != last_name) {
					_result.named_captures.start_indices.emplace_back(_result.named_captures.indices.size());
					_capture_names.emplace_back(std::exchange(last_name, std::move(cap.name)));
				}
				_result.named_captures.indices.emplace_back(cap.index);
			}
			_result.named_captures.start_indices.emplace_back(_result.named_captures.indices.size());
			_capture_names.emplace_back(std::move(last_name));

			// build reverse name mapping
			for (std::size_t i = 0; i + 1 < _result.named_captures.start_indices.size(); ++i) {
				for (auto id : _result.named_captures.get_indices_for_name(i)) {
					if (id >= _result.named_captures.reverse_mapping.size()) {
						_result.named_captures.reverse_mapping.resize(
							id + 1, compiled_unoptimized::named_capture_registry::no_reverse_mapping
						);
					}
					std::size_t &value = _result.named_captures.reverse_mapping[id];
					if (value != compiled_unoptimized::named_capture_registry::no_reverse_mapping && value != i) {
						// TODO error
					}
					value = i;
				}
			}
		}
		_compile(start_state, end_state, expr.root());

		// subroutine for the whole pattern, i.e., recursion
		if (_captures.empty()) {
			_captures.emplace_back();
		}
		_captures[0].start = start_state;
		_captures[0].end = end_state;
		// initialize all subroutine data
		for (auto sub : _subroutines) {
			if (sub.index >= _captures.size() || _captures[sub.index].is_empty()) {
				_result = half_compiled::state_machine();
				_result.start_state = _result.create_state();
				_result.end_state = _result.create_state();
				// TODO error
				continue;
			}
			auto &group_info = _captures[sub.index];
			auto &trans = _result.get_transition(sub.transition);
			auto &jmp = std::get<compiled_unoptimized::transitions::jump>(trans.condition);
			trans.new_state = group_info.start;
			jmp.target = group_info.end;
		}

		_named_captures.clear();
		_capture_names.clear();
		_captures.clear();
		_subroutines.clear();
		_fail_state = compiled_unoptimized::state_ref();
		_ast = nullptr;
		_analysis = nullptr;
		return std::move(_result);
	}

	void compiler::_compile(
		compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end, const ast_nodes::literal &node
	) {
		auto [transition_ref, transition] = _result.create_transition_from_to(start, end);
		auto &cond = transition.condition.emplace<compiled_unoptimized::transitions::literal>();
		cond.case_insensitive = node.case_insensitive;
		if (node.case_insensitive) {
			for (codepoint cp : node.contents) {
				cond.contents.push_back(unicode::case_folding::get_cached().fold_simple(cp));
			}
		} else {
			cond.contents = node.contents;
		}
	}

	void compiler::_compile(
		compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
		const ast_nodes::numbered_backreference &expr
	) {
		auto [trans_ref, trans] = _result.create_transition_from_to(start, end);
		auto &cond = trans.condition.emplace<compiled_unoptimized::transitions::numbered_backreference>();
		cond.index = expr.index;
		cond.case_insensitive = expr.case_insensitive;
	}

	void compiler::_compile(
		compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
		const ast_nodes::named_backreference &expr
	) {
		if (auto id = _find_capture_name(expr.name)) {
			auto [trans_ref, trans] = _result.create_transition_from_to(start, end);
			auto &ref = trans.condition.emplace<compiled_unoptimized::transitions::named_backreference>();
			ref.index = id.value();
			ref.case_insensitive = expr.case_insensitive;
		} else {
			// TODO error
		}
	}

	void compiler::_compile(
		compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
		const ast_nodes::numbered_subroutine &expr
	) {
		auto [trans_ref, trans] = _result.create_transition_from_to(start, compiled_unoptimized::state_ref());
		auto &jmp = trans.condition.emplace<compiled_unoptimized::transitions::jump>();
		jmp.return_state = end;
		jmp.subroutine_index = expr.index;
		_subroutines.emplace_back(trans_ref, expr.index);
	}

	void compiler::_compile(
		compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
		const ast_nodes::named_subroutine &expr
	) {
		if (auto id = _find_capture_name(expr.name)) {
			auto indices = _result.named_captures.get_indices_for_name(id.value());
			if (indices.size() > 0) {
				auto [trans_ref, trans] =
					_result.create_transition_from_to(start, compiled_unoptimized::state_ref());
				auto &jmp = trans.condition.emplace<compiled_unoptimized::transitions::jump>();
				jmp.return_state = end;
				jmp.subroutine_index = indices[0];
				_subroutines.emplace_back(trans_ref, indices[0]);
			} else {
				// TODO error
			}
		} else {
			// TODO error
		}
	}

	void compiler::_compile(
		compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
		const ast_nodes::subexpression &expr
	) {
		if (
			expr.subexpr_type == ast_nodes::subexpression::type::normal ||
			expr.subexpr_type == ast_nodes::subexpression::type::atomic
		) {
			// create states that allow for additional actions during the start and end of the group
			auto new_start = _result.create_state();
			auto new_end = _result.create_state();
			auto [begin_trans_ref, begin_trans] = _result.create_transition_from_to(start, new_start);
			auto [end_trans_ref, end_trans] = _result.create_transition_from_to(new_end, end);
			start = new_start;
			end = new_end;

			if (expr.subexpr_type == ast_nodes::subexpression::type::normal) {
				auto &capture_beg =
					begin_trans.condition.emplace<compiled_unoptimized::transitions::capture_begin>();
				capture_beg.index = expr.capture_index;
				end_trans.condition.emplace<compiled_unoptimized::transitions::capture_end>();
				// save capture group information
				if (expr.capture_index >= _captures.size() || _captures[expr.capture_index].is_empty()) {
					// this is the first occurence - save it
					_captures.resize(std::max(_captures.size(), expr.capture_index + 1));
					// save the range without the push/pop capture transitions - we don't need them for subroutines
					_captures[expr.capture_index].start = start;
					_captures[expr.capture_index].end = end;
				}
			} else if (expr.subexpr_type == ast_nodes::subexpression::type::atomic) {
				begin_trans.condition.emplace<compiled_unoptimized::transitions::push_atomic>();
				end_trans.condition.emplace<compiled_unoptimized::transitions::pop_atomic>();
			}
		}

		if (expr.nodes.empty()) {
			_result.create_transition_from_to(start, end);
		} else {
			compiled_unoptimized::state_ref current = start;
			auto it = expr.nodes.begin();
			for (; it + 1 != expr.nodes.end(); ++it) {
				auto next = _result.create_state();
				_compile(current, next, *it);
				current = next;
			}
			_compile(current, end, *it);
		}
	}

	void compiler::_compile(
		compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end, const ast_nodes::repetition &rep
	) {
		if (rep.min > 1000000 || (rep.max != ast_nodes::repetition::no_limit && rep.max > 1000000)) {
			// TODO better algorithm?
			return;
		}

		if (rep.max == 0) {
			// ignore the expression
			_result.create_transition_from_to(start, end);
			// and compile the graph to an island, in case it's called as a subroutine
			auto island_start = _result.create_state();
			auto island_end = _result.create_state();
			_compile(island_start, island_end, rep.expression);
			return;
		}

		// handle posessed (atomic) repetition
		if (rep.repetition_type == ast_nodes::repetition::type::posessed) {
			auto new_start = _result.create_state();
			auto [start_trans_ref, start_trans] = _result.create_transition_from_to(start, new_start);
			start_trans.condition.emplace<compiled_unoptimized::transitions::push_atomic>();
			auto new_end = _result.create_state();
			auto [end_trans_ref, end_trans] = _result.create_transition_from_to(new_end, end);
			end_trans.condition.emplace<compiled_unoptimized::transitions::pop_atomic>();

			start = new_start;
			end = new_end;
		}

		compiled_unoptimized::state_ref cur = start;
		for (std::size_t i = 1; i < rep.min; ++i) {
			auto next = _result.create_state();
			_compile(cur, next, rep.expression);
			cur = next;
		}
		if (rep.min == rep.max) {
			_compile(cur, end, rep.expression);
		} else if (rep.max == ast_nodes::repetition::no_limit) {
			if (rep.min > 0) {
				auto next = _result.create_state();
				_compile(cur, next, rep.expression);
				cur = next;
			} else {
				// create a new state with epsilon transition,
				// so that we don't have an edge back to the starting node
				auto next = _result.create_state();
				_result.create_transition_from_to(cur, next);
				cur = next;
			}
			auto next = _result.create_state();
			if (rep.repetition_type == ast_nodes::repetition::type::lazy) {
				_result.create_transition_from_to(cur, end);
				_compile(cur, next, rep.expression);
				_result.create_transition_from_to(next, cur);
			} else {
				auto body_start = _result.create_state();
				// push the current stream position so that we can detect infinite loops
				auto [enter_ref, enter] = _result.create_transition_from_to(cur, body_start);
				enter.condition.emplace<compiled_unoptimized::transitions::push_position>();
				// if the current iteration does not match the body, then continue on to the rest of the expression
				// this should start from cur instead of start to avoid pushing unnecessary stream positions
				_result.create_transition_from_to(cur, end);

				_compile(body_start, next, rep.expression);

				// loop back - watch out for infinite loops
				auto [loopback_ref, loopback] = _result.create_transition_from_to(next, cur);
				loopback.condition.emplace<compiled_unoptimized::transitions::check_infinite_loop>();
				// if loop back does not pass, then just jump to the end
				_result.create_transition_from_to(next, end);
			}
		} else {
			if (rep.min > 0) {
				auto next = _result.create_state();
				_compile(cur, next, rep.expression);
				cur = next;
			}
			for (std::size_t i = rep.min + 1; i < rep.max; ++i) {
				auto next = _result.create_state();
				if (rep.repetition_type == ast_nodes::repetition::type::lazy) {
					_result.create_transition_from_to(cur, end);
					_compile(cur, next, rep.expression);
				} else {
					_compile(cur, next, rep.expression);
					_result.create_transition_from_to(cur, end);
				}
				cur = next;
			}
			if (rep.repetition_type == ast_nodes::repetition::type::lazy) {
				_result.create_transition_from_to(cur, end);
				_compile(cur, end, rep.expression);
			} else {
				_compile(cur, end, rep.expression);
				_result.create_transition_from_to(cur, end);
			}
		}
	}

	void compiler::_compile(
		compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
		const ast_nodes::complex_assertion &ass
	) {
		{ // checkpoint & restore the stream
			auto new_start = _result.create_state();
			auto new_end = _result.create_state();

			auto [start_trans_ref, start_trans] = _result.create_transition_from_to(start, new_start);
			start_trans.condition.emplace<compiled_unoptimized::transitions::push_stream_checkpoint>();

			auto [end_trans_ref, end_trans] = _result.create_transition_from_to(new_end, end);
			end_trans.condition.emplace<compiled_unoptimized::transitions::restore_stream_checkpoint>();

			start = new_start;
			end = new_end;
		}

		if (ass.negative || !ass.non_atomic) { // negative assertions are always atomic
			auto new_start = _result.create_state();
			auto new_end = _result.create_state();

			auto [start_trans_ref, start_trans] = _result.create_transition_from_to(start, new_start);
			start_trans.condition.emplace<compiled_unoptimized::transitions::push_atomic>();

			auto [end_trans_ref, end_trans] = _result.create_transition_from_to(new_end, end);
			end_trans.condition.emplace<compiled_unoptimized::transitions::pop_atomic>();

			start = new_start;
			end = new_end;
		}
		
		auto end_state_for_negative = end;
		if (ass.negative) {
			auto before_fail_state = _result.create_state();
			// prevent backtracking by inserting a pop_atomic
			auto [trans_ref, trans] = _result.create_transition_from_to(before_fail_state, _get_fail_state());
			trans.condition.emplace<compiled_unoptimized::transitions::pop_atomic>();
			end = before_fail_state;
		}

		// actually compile the expression
		if (ass.backward) {
			const auto &expr = std::get<ast_nodes::subexpression>(_ast->get_node(ass.expression).value);
			if (expr.nodes.size() == 1 && _ast->get_node(expr.nodes[0]).is<ast_nodes::alternative>()) {
				auto &alt_node = std::get<ast_nodes::alternative>(_ast->get_node(expr.nodes[0]).value);
				for (auto alt : alt_node.alternatives) {
					auto analysis = _analysis->get_for(alt);
					if (analysis.minimum_length != analysis.maximum_length) {
						// TODO error
					} else {
						auto state = _result.create_state();
						auto [trans_ref, trans] = _result.create_transition_from_to(start, state);
						auto &cond = trans.condition.emplace<compiled_unoptimized::transitions::rewind>();
						cond.num_codepoints = analysis.minimum_length;
						_compile(state, end, alt);
					}
				}
			} else {
				auto analysis = _analysis->get_for(ass.expression);
				if (analysis.minimum_length != analysis.maximum_length) {
					// TODO error
				} else {
					auto state = _result.create_state();
					auto [trans_ref, trans] = _result.create_transition_from_to(start, state);
					auto &cond = trans.condition.emplace<compiled_unoptimized::transitions::rewind>();
					cond.num_codepoints = analysis.minimum_length;
					_compile(state, end, ass.expression);
				}
			}
			// TODO handle reversed stream
		} else {
			_compile(start, end, ass.expression);
		}

		if (ass.negative) {
			_result.create_transition_from_to(start, end_state_for_negative);
		}
	}

	void compiler::_compile(
		compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
		const ast_nodes::conditional_expression &expr
	) {
		if (std::holds_alternative<ast_nodes::conditional_expression::define>(expr.condition)) {
			_result.create_transition_from_to(start, end);
			// generate a separate graph for DEFINE's
			auto define_start = _result.create_state();
			auto define_end = _result.create_state();
			_compile(define_start, define_end, expr.if_true);
			return;
		}

		auto branch_state = _result.create_state();
		auto before_yes_state = _result.create_state();
		auto yes_state = _result.create_state();
		auto before_no_state = _result.create_state();
		auto no_state = _result.create_state();

		auto [start_trans_ref, start_trans] = _result.create_transition_from_to(start, branch_state);
		start_trans.condition.emplace<compiled_unoptimized::transitions::push_atomic>();

		std::visit(
			[&](auto &&cond) {
				_compile_condition(branch_state, before_yes_state, cond);
			},
			expr.condition
		);
		_result.create_transition_from_to(branch_state, before_no_state);

		auto [pop_trans1_ref, pop_trans1] = _result.create_transition_from_to(before_yes_state, yes_state);
		pop_trans1.condition.emplace<compiled_unoptimized::transitions::pop_atomic>();

		auto [pop_trans2_ref, pop_trans2] = _result.create_transition_from_to(before_no_state, no_state);
		pop_trans2.condition.emplace<compiled_unoptimized::transitions::pop_atomic>();

		_compile(yes_state, end, expr.if_true);
		if (expr.if_false) {
			_compile(no_state, end, expr.if_false.value());
		} else {
			_result.create_transition_from_to(no_state, end);
		}
	}

	void compiler::_compile_condition(
		compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
		const ast_nodes::conditional_expression::named_capture_available &cond
	) {
		auto [trans_ref, trans] = _result.create_transition_from_to(start, end);
		if (auto id = _find_capture_name(cond.name)) {
			trans.condition.emplace<
				compiled_unoptimized::transitions::conditions::named_capture
			>().name_index = id.value();
			return;
		}
		// check if this is a recursion
		if (cond.name.size() > 0 && cond.name[0] == U'R') {
			if (cond.name.size() > 1 && cond.name[1] == U'&') { // named
				if (auto id = _find_capture_name(codepoint_string_view(cond.name).substr(2))) {
					auto &rec = trans.condition.emplace<
						compiled_unoptimized::transitions::conditions::named_recursion
					>();
					rec.name_index = id.value();
				} else {
					// TODO error
				}
			} else { // numbered
				auto &rec = trans.condition.emplace<
					compiled_unoptimized::transitions::conditions::numbered_recursion
				>();
				if (cond.name.size() > 1) {
					// an index follows - parse it
					rec.index = 0;
					for (std::size_t i = 1; i < cond.name.size(); ++i) {
						if (cond.name[i] < U'0' || cond.name[i] > U'9') {
							// TODO error
							return;
						}
						rec.index = rec.index * 10 + (cond.name[i] - U'0');
					}
				}
			}
			return;
		}
		// TODO error
	}

	void compiler::_compile_condition(
		compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
		const ast_nodes::conditional_expression::complex_assertion &cond
	) {
		_compile(start, end, cond.node);
	}
}
