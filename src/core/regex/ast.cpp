// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/core/regex/ast.h"

/// \file
/// Implementation of certain AST functionalities such as analysis.

namespace codepad::regex {
	ast::analysis ast::analyze() const {
		_analysis_context ctx;
		ctx.output._node_analysis.resize(_nodes.size());
		// figure out the order in which we need to analyze the nodes
		ctx.dependency_queue.emplace_back(_root);
		while (!ctx.dependency_queue.empty()) {
			ctx.end_stack.emplace_back(ctx.dependency_queue.front());
			_collect_dependencies(ctx.dependency_queue.front(), ctx);
			ctx.dependency_queue.pop_front();
		}
		// actually analyze all nodes
		for (auto it = ctx.end_stack.rbegin(); it != ctx.end_stack.rend(); ++it) {
			_analyze(*it, ctx);
		}
		return std::move(ctx.output);
	}

	ast_nodes::analysis ast::_analyze(const ast_nodes::subexpression &n, _analysis_context &ctx) const {
		ast_nodes::analysis result;
		result.minimum_length = result.maximum_length = 0;
		for (auto child : n.nodes) {
			auto child_analysis = ctx.output.get_for(child);
			result.minimum_length += child_analysis.minimum_length;
			result.maximum_length += child_analysis.maximum_length;
		}
		return result;
	}

	ast_nodes::analysis ast::_analyze(const ast_nodes::alternative &n, _analysis_context &ctx) const {
		ast_nodes::analysis result;
		if (n.alternatives.size() == 0) {
			result.minimum_length = result.maximum_length = 0;
			return result;
		}
		result.minimum_length = std::numeric_limits<std::size_t>::max();
		result.maximum_length = 0;
		for (auto child : n.alternatives) {
			auto child_analysis = ctx.output.get_for(child);
			result.minimum_length = std::min(result.minimum_length, child_analysis.minimum_length);
			result.maximum_length = std::max(result.maximum_length, child_analysis.maximum_length);
		}
		return result;
	}

	ast_nodes::analysis ast::_analyze(const ast_nodes::repetition &n, _analysis_context &ctx) const {
		ast_nodes::analysis result;
		ast_nodes::analysis subject_analysis = ctx.output.get_for(n.expression);
		// minimum length
		result.minimum_length = subject_analysis.minimum_length * n.min;
		// maximum length
		if (n.max == ast_nodes::repetition::no_limit) {
			result.maximum_length =
				subject_analysis.maximum_length > 0 ?
				std::numeric_limits<std::size_t>::max() :
				0;
		} else {
			result.maximum_length = subject_analysis.maximum_length * n.max;
		}
		return result;
	}

	ast_nodes::analysis ast::_analyze(
		const ast_nodes::conditional_expression &n, _analysis_context &ctx
	) const {
		ast_nodes::analysis result;
		result = ctx.output.get_for(n.if_true);
		if (n.if_false) {
			ast_nodes::analysis false_analysis = ctx.output.get_for(n.if_false.value());
			result.minimum_length = std::min(result.minimum_length, false_analysis.minimum_length);
			result.maximum_length = std::max(result.maximum_length, false_analysis.maximum_length);
		} else {
			result.minimum_length = 0;
		}
		return result;
	}
}
