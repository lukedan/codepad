// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/core/regex/compiler.h"

/// \file
/// Regex tests using the PCRE2 test data.

namespace codepad::regex {
	namespace compiled {
		state_ref state_machine::create_state() {
			std::size_t res = states.size();
			states.emplace_back();
			return state_ref(*this, res);
		}

		transition_ref state_ref::create_transition() {
			std::size_t res = get().transitions.size();
			get().transitions.emplace_back();
			return transition_ref(*this, res);
		}

		void state_machine::dump(std::ostream &stream, bool valid_only) const {
			stream << "digraph {\n";
			stream << "n" << start_state << "[color=red];\n";
			stream << "n" << end_state << "[color=blue];\n";
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
					stream << "n" << i << " -> n" << t.new_state_index << " [label=\"" << j << ": ";
					if (std::holds_alternative<codepoint_string>(t.condition)) {
						const auto &str = std::get<codepoint_string>(t.condition);
						for (codepoint cp : str) {
							if (is_printable_char(cp)) {
								stream << static_cast<char>(cp);
							} else {
								stream << "?";
							}
						}
					} else if (std::holds_alternative<transitions::character_class>(t.condition)) {
						const auto &list = std::get<transitions::character_class>(t.condition);
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
					} else if (std::holds_alternative<transitions::assertion>(t.condition)) {
						stream << "<assertion>";
					} else if (std::holds_alternative<transitions::capture_begin>(t.condition)) {
						const auto &cap = std::get<transitions::capture_begin>(t.condition);
						stream << "<start capture " << cap.index;
						if (cap.name_index != transitions::capture_begin::unnamed_capture) {
							stream << " named:";
							for (std::size_t index : named_captures.get_indices_for_name(cap.name_index)) {
								stream << " " << index;
							}
						}
						stream << ">";
					} else if (std::holds_alternative<transitions::capture_end>(t.condition)) {
						stream << "<end capture>";
					} else if (std::holds_alternative<transitions::numbered_backreference>(t.condition)) {
						const auto &backref = std::get<transitions::numbered_backreference>(t.condition);
						stream << "<backref #" << backref.index << ">";
					} else if (std::holds_alternative<transitions::named_backreference>(t.condition)) {
						const auto &backref = std::get<transitions::named_backreference>(t.condition);
						stream << "<named backref #" << backref.index << ">";
					} else {
						stream << "<UNHANDLED>";
					}
					stream << "\"];\n";
				}
			}
			stream << "}\n";
		}
	}


	void compiler::_compile(compiled::state_ref start, compiled::state_ref end, const ast::nodes::literal &node) {
		auto transition = start.create_transition();
		transition->new_state_index = end.index;
		transition->case_insensitive = node.case_insensitive;
		if (node.case_insensitive) {
			auto &cond_str = transition->condition.emplace<codepoint_string>();
			for (codepoint cp : node.contents) {
				cond_str.push_back(unicode::case_folding::get_cached().fold_simple(cp));
			}
		} else {
			transition->condition = node.contents;
		}
	}

	void compiler::_compile(
		compiled::state_ref start, compiled::state_ref end, const ast::nodes::numbered_backreference &expr
	) {
		auto trans = start.create_transition();
		trans->case_insensitive = expr.case_insensitive;
		trans->new_state_index = end.index;
		trans->condition.emplace<compiled::transitions::numbered_backreference>().index = expr.index;
	}

	void compiler::_compile(
		compiled::state_ref start, compiled::state_ref end, const ast::nodes::named_backreference &expr
	) {
		auto trans = start.create_transition();
		trans->case_insensitive = expr.case_insensitive;
		trans->new_state_index = end.index;
		auto it = std::lower_bound(_capture_names.begin(), _capture_names.end(), expr.name);
		auto &ref = trans->condition.emplace<compiled::transitions::named_backreference>();
		ref.index = (it - _capture_names.begin()) - 1;
	}

	void compiler::_compile(
		compiled::state_ref start, compiled::state_ref end, const ast::nodes::subexpression &expr
	) {
		auto compile_expr = [this](compiled::state_ref cur, compiled::state_ref next, const ast::node &n) {
			std::visit(
				[this, cur, next](const auto &v) {
					_compile(cur, next, v);
				},
				n.value
			);
		};

		if (
			expr.subexpr_type == ast::nodes::subexpression::type::normal ||
			expr.subexpr_type == ast::nodes::subexpression::type::atomic
		) {
			// create states that allow for additional actions during the start and end of the group
			auto new_start = _result.create_state();
			auto new_end = _result.create_state();
			auto begin_trans = start.create_transition();
			begin_trans->new_state_index = new_start.index;
			auto end_trans = new_end.create_transition();
			end_trans->new_state_index = end.index;
			start = new_start;
			end = new_end;

			if (expr.subexpr_type == ast::nodes::subexpression::type::normal) {
				auto &capture_beg = begin_trans->condition.emplace<compiled::transitions::capture_begin>();
				capture_beg.index = expr.capture_index;
				if (!expr.capture_name.empty()) {
					auto iter = std::lower_bound(_capture_names.begin(), _capture_names.end(), expr.capture_name);
					capture_beg.name_index = (iter - _capture_names.begin()) - 1;
				}
				end_trans->condition.emplace<compiled::transitions::capture_end>();
			} else if (expr.subexpr_type == ast::nodes::subexpression::type::atomic) {
				begin_trans->condition.emplace<compiled::transitions::push_atomic>();
				end_trans->condition.emplace<compiled::transitions::pop_atomic>();
			}
		}

		if (expr.nodes.empty()) {
			start.create_transition()->new_state_index = end.index;
		} else {
			compiled::state_ref current = start;
			auto it = expr.nodes.begin();
			for (; it + 1 != expr.nodes.end(); ++it) {
				auto next = _result.create_state();
				compile_expr(current, next, *it);
				current = next;
			}
			compile_expr(current, end, *it);
		}
	}

	void compiler::_compile(
		compiled::state_ref start, compiled::state_ref end, const ast::nodes::repetition &rep
	) {
		if (rep.min > 1000000 || (rep.max != ast::nodes::repetition::no_limit && rep.max > 1000000)) {
			// TODO better algorithm?
			return;
		}

		if (rep.max == 0) { // ignore the expression
			start.create_transition()->new_state_index = end.index;
			return;
		}

		// handle posessed (atomic) repetition
		if (rep.repetition_type == ast::nodes::repetition::type::posessed) {
			auto new_start = _result.create_state();
			auto start_trans = start.create_transition();
			start_trans->new_state_index = new_start.index;
			start_trans->condition.emplace<compiled::transitions::push_atomic>();
			auto new_end = _result.create_state();
			auto end_trans = new_end.create_transition();
			end_trans->new_state_index = end.index;
			end_trans->condition.emplace<compiled::transitions::pop_atomic>();

			start = new_start;
			end = new_end;
		}

		compiled::state_ref cur = start;
		for (std::size_t i = 1; i < rep.min; ++i) {
			auto next = _result.create_state();
			_compile(cur, next, rep.expression);
			cur = next;
		}
		if (rep.min == rep.max) {
			_compile(cur, end, rep.expression);
		} else if (rep.max == ast::nodes::repetition::no_limit) {
			if (rep.min > 0) {
				auto next = _result.create_state();
				_compile(cur, next, rep.expression);
				cur = next;
			} else {
				// create a new state with epsilon transition,
				// so that we don't have an edge back to the starting node
				auto next = _result.create_state();
				cur.create_transition()->new_state_index = next.index;
				cur = next;
			}
			auto next = _result.create_state();
			if (rep.repetition_type == ast::nodes::repetition::type::lazy) {
				cur.create_transition()->new_state_index = end.index;
				_compile(cur, next, rep.expression);
			} else {
				_compile(cur, next, rep.expression);
				cur.create_transition()->new_state_index = end.index;
			}
			next.create_transition()->new_state_index = cur.index;
		} else {
			if (rep.min > 0) {
				auto next = _result.create_state();
				_compile(cur, next, rep.expression);
				cur = next;
			}
			for (std::size_t i = rep.min + 1; i < rep.max; ++i) {
				auto next = _result.create_state();
				if (rep.repetition_type == ast::nodes::repetition::type::lazy) {
					cur.create_transition()->new_state_index = end.index;
					_compile(cur, next, rep.expression);
				} else {
					_compile(cur, next, rep.expression);
					cur.create_transition()->new_state_index = end.index;
				}
				cur = next;
			}
			if (rep.repetition_type == ast::nodes::repetition::type::lazy) {
				cur.create_transition()->new_state_index = end.index;
				_compile(cur, end, rep.expression);
			} else {
				_compile(cur, end, rep.expression);
				cur.create_transition()->new_state_index = end.index;
			}
		}
	}

	void compiler::_compile(compiled::state_ref start, compiled::state_ref end, const ast::nodes::assertion &rep) {
		auto transition = start.create_transition();
		transition->new_state_index = end.index;
		auto &assertion = transition->condition.emplace<compiled::transitions::assertion>();
		assertion.assertion_type = rep.assertion_type;
		if (rep.assertion_type >= ast::nodes::assertion::type::complex_first) {
			compiler cmp;
			assertion.expression = cmp.compile(rep.expression);
		} else if (rep.assertion_type >= ast::nodes::assertion::type::character_class_first) {
			auto &cls = assertion.expression.states.emplace_back().transitions.emplace_back().condition.emplace<
				compiled::transitions::character_class
			>();
			const auto &cls_node = std::get<ast::nodes::character_class>(rep.expression.nodes[0].value);
			cls.ranges = cls_node.ranges;
			cls.is_negate = cls_node.is_negate;
		}
	}

	void compiler::_compile(
		compiled::state_ref start, compiled::state_ref end, const ast::nodes::conditional_expression &expr
	) {
		if (std::holds_alternative<ast::nodes::conditional_expression::define>(expr.condition)) {
			// do not actually generate graph for DEFINE's
			start.create_transition()->new_state_index = end.index;
			return;
		}

		auto branch_state = _result.create_state();
		auto yes_state = _result.create_state();
		auto no_state = _result.create_state();

		start.create_transition()->new_state_index = branch_state.index;

		branch_state->no_backtracking = true;
		auto yes_transition = branch_state.create_transition();
		std::visit(
			[&](auto &&cond) {
				_compile_condition(yes_transition.get(), cond);
			},
			expr.condition
		);
		yes_transition->new_state_index = yes_state.index;
		branch_state.create_transition()->new_state_index = no_state.index;

		_compile(yes_state, end, expr.if_true);
		if (expr.if_false) {
			_compile(no_state, end, expr.if_false.value());
		} else {
			no_state.create_transition()->new_state_index = end.index;
		}
	}

	void compiler::_compile_condition(compiled::transition &trans, const ast::nodes::assertion &cond) {
		auto &cond_val = trans.condition.emplace<compiled::transitions::assertion>();
		cond_val.assertion_type = cond.assertion_type;
		compiler cmp;
		cond_val.expression = cmp.compile(cond.expression);
	}
}
