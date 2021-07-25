// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/core/regex/compiler.h"

/// \file
/// Regex tests using the PCRE2 test data.

namespace codepad::regex {
	namespace compiled {
		std::size_t state_machine::create_state() {
			std::size_t res = states.size();
			states.emplace_back();
			return res;
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
					} else if (std::holds_alternative<character_class>(t.condition)) {
						const auto &list = std::get<character_class>(t.condition);
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
					} else if (std::holds_alternative<assertion>(t.condition)) {
						stream << "<assertion>";
					} else if (std::holds_alternative<capture_begin>(t.condition)) {
						const auto &cap = std::get<capture_begin>(t.condition);
						stream << "<start capture " << cap.index;
						if (cap.name_index != capture_begin::unnamed_capture) {
							stream << " named:";
							for (std::size_t index : named_captures.get_indices_for_name(cap.name_index)) {
								stream << " " << index;
							}
						}
						stream << ">";
					} else if (std::holds_alternative<capture_end>(t.condition)) {
						stream << "<end capture>";
					} else if (std::holds_alternative<backreference>(t.condition)) {
						const auto &backref = std::get<backreference>(t.condition);
						stream << "<";
						if (backref.is_named) {
							stream << "named ";
						}
						stream << "backreference " << backref.index << ">";
					}
					stream << "\"];\n";
				}
			}
			stream << "}\n";
		}
	}


	void compiler::_compile(std::size_t start, std::size_t end, const ast::nodes::literal &node) {
		auto &transition = _result.states[start].transitions.emplace_back();
		transition.new_state_index = end;
		transition.case_insensitive = node.case_insensitive;
		if (node.case_insensitive) {
			auto &cond_str = transition.condition.emplace<codepoint_string>();
			for (codepoint cp : node.contents) {
				cond_str.push_back(unicode::case_folding::get_cached().fold_simple(cp));
			}
		} else {
			transition.condition = node.contents;
		}
	}

	void compiler::_compile(std::size_t start, std::size_t end, const ast::nodes::backreference &expr) {
		auto &trans = _result.states[start].transitions.emplace_back();
		trans.case_insensitive = expr.case_insensitive;
		trans.new_state_index = end;
		auto &ref = trans.condition.emplace<compiled::backreference>();
		if (std::holds_alternative<std::size_t>(expr.index)) {
			ref.index = std::get<std::size_t>(expr.index);
		} else {
			ref.is_named = true;
			const auto &name = std::get<codepoint_string>(expr.index);
			auto it = std::lower_bound(_capture_names.begin(), _capture_names.end(), name);
			ref.index = (it - _capture_names.begin()) - 1;
		}
	}

	void compiler::_compile(std::size_t start, std::size_t end, const ast::nodes::subexpression &expr) {
		auto compile_expr = [this](std::size_t cur, std::size_t next, const ast::node &n) {
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
			auto &begin_trans = _result.states[start].transitions.emplace_back();
			begin_trans.new_state_index = new_start;
			auto &end_trans = _result.states[new_end].transitions.emplace_back();
			end_trans.new_state_index = end;
			start = new_start;
			end = new_end;

			if (expr.subexpr_type == ast::nodes::subexpression::type::normal) {
				auto &capture_beg = begin_trans.condition.emplace<compiled::capture_begin>();
				capture_beg.index = expr.capture_index;
				if (!expr.capture_name.empty()) {
					auto iter = std::lower_bound(_capture_names.begin(), _capture_names.end(), expr.capture_name);
					capture_beg.name_index = (iter - _capture_names.begin()) - 1;
				}
				end_trans.condition.emplace<compiled::capture_end>();
			} else if (expr.subexpr_type == ast::nodes::subexpression::type::atomic) {
				begin_trans.condition.emplace<compiled::push_atomic>();
				end_trans.condition.emplace<compiled::pop_atomic>();
			}
		}

		if (expr.nodes.empty()) {
			_result.states[start].transitions.emplace_back().new_state_index = end;
		} else {
			std::size_t current = start;
			auto it = expr.nodes.begin();
			for (; it + 1 != expr.nodes.end(); ++it) {
				std::size_t next = _result.create_state();
				compile_expr(current, next, *it);
				current = next;
			}
			compile_expr(current, end, *it);
		}
	}

	void compiler::_compile(std::size_t start, std::size_t end, const ast::nodes::repetition &rep) {
		if (rep.min > 1000000 || (rep.max != ast::nodes::repetition::no_limit && rep.max > 1000000)) {
			// TODO better algorithm?
			return;
		}
		if (rep.max == 0) { // special case: don't match if the expression is matched zero times
			std::size_t bad_state = _result.create_state();
			_compile(start, bad_state, rep.expression);
			_result.states[start].transitions.emplace_back().new_state_index = end;
			return;
		}
		std::size_t cur = start;
		for (std::size_t i = 1; i < rep.min; ++i) {
			std::size_t next = _result.create_state();
			_compile(cur, next, rep.expression);
			cur = next;
		}
		if (rep.min == rep.max) {
			_compile(cur, end, rep.expression);
		} else if (rep.max == ast::nodes::repetition::no_limit) {
			if (rep.min > 0) {
				std::size_t next = _result.create_state();
				_compile(cur, next, rep.expression);
				cur = next;
			} else {
				// create a new state with epsilon transition,
				// so that we don't have an edge back to the starting node
				std::size_t next = _result.create_state();
				_result.states[cur].transitions.emplace_back().new_state_index = next;
				cur = next;
			}
			std::size_t next = _result.create_state();
			if (rep.lazy) {
				_result.states[cur].transitions.emplace_back().new_state_index = end;
				_compile(cur, next, rep.expression);
			} else {
				_compile(cur, next, rep.expression);
				_result.states[cur].transitions.emplace_back().new_state_index = end;
			}
			_result.states[next].transitions.emplace_back().new_state_index = cur;
		} else {
			if (rep.min > 0) {
				std::size_t next = _result.create_state();
				_compile(cur, next, rep.expression);
				cur = next;
			}
			for (std::size_t i = rep.min + 1; i < rep.max; ++i) {
				std::size_t next = _result.create_state();
				if (rep.lazy) {
					_result.states[cur].transitions.emplace_back().new_state_index = end;
					_compile(cur, next, rep.expression);
				} else {
					_compile(cur, next, rep.expression);
					_result.states[cur].transitions.emplace_back().new_state_index = end;
				}
				cur = next;
			}
			if (rep.lazy) {
				_result.states[cur].transitions.emplace_back().new_state_index = end;
				_compile(cur, end, rep.expression);
			} else {
				_compile(cur, end, rep.expression);
				_result.states[cur].transitions.emplace_back().new_state_index = end;
			}
		}
	}

	void compiler::_compile(std::size_t start, std::size_t end, const ast::nodes::assertion &rep) {
		auto &transition = _result.states[start].transitions.emplace_back();
		transition.new_state_index = end;
		auto &assertion = transition.condition.emplace<compiled::assertion>();
		assertion.assertion_type = rep.assertion_type;
		if (rep.assertion_type >= ast::nodes::assertion::type::complex_first) {
			compiler cmp;
			assertion.expression = cmp.compile(rep.expression);
		} else if (rep.assertion_type >= ast::nodes::assertion::type::character_class_first) {
			auto &cls = assertion.expression.states.emplace_back().transitions.emplace_back().condition.emplace<
				compiled::character_class
			>();
			const auto &cls_node = std::get<ast::nodes::character_class>(rep.expression.nodes[0].value);
			cls.ranges = cls_node.ranges;
			cls.is_negate = cls_node.is_negate;
		}
	}
}
