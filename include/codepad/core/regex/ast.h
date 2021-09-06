// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// The parsed syntax tree for regular expressions.

#include <deque>
#include <variant>

#include "codepad/core/unicode/common.h"
#include "codepad/core/assert.h"
#include "codepad/core/encodings.h"

namespace codepad::regex {
	class ast;
	class compiler;

	namespace ast_nodes {
		/// Additional information computed for a node.
		struct analysis {
			std::size_t minimum_length = 0; ///< Minimum potential length of matched strings.
			/// Maximum potential length of matched strings.
			std::size_t maximum_length = std::numeric_limits<std::size_t>::max();
		};

		/// Reference to a node.
		struct node_ref {
			friend ast;
		public:
			/// Index indicating that this reference is empty.
			constexpr static std::size_t invalid_index = std::numeric_limits<std::size_t>::max();

			/// Initializes this reference to be invalid.
			node_ref() {
			}

			/// Checks if this reference is valid.
			[[nodiscard]] explicit operator bool() const {
				return _index != invalid_index;
			}
		protected:
			/// Initializes \ref index.
			explicit node_ref(std::size_t i) : _index(i) {
			}

			std::size_t _index = invalid_index; ///< Index of the node.
		};


		/// Node used to indicate that this part of the expression failed to parse.
		struct error {
		};
		/// Node used to signal a feature is enabled.
		struct feature {
			codepoint_string identifier; ///< String used to identify the feature.
		};
		/// Overrides the start of the match.
		struct match_start_override {
		};
		/// A node that contains a string literal.
		struct literal {
			codepoint_string contents; ///< The literal.
			bool case_insensitive = false; ///< Whether this literal is matched in a case-insensitive manner.
		};
		/// A numbered backreference.
		struct numbered_backreference {
			/// Default constructor.
			numbered_backreference() = default;
			/// Initializes this backreference with the given numerical index.
			numbered_backreference(std::size_t id, bool ignore_case) : index(id), case_insensitive(ignore_case) {
			}

			std::size_t index = 0; ///< The index of this backreference.
			bool case_insensitive = false; ///< Whether this backreference is case-insensitive.
		};
		/// A named backreference.
		struct named_backreference {
			/// Default constructor.
			named_backreference() = default;
			/// Initializes all fields of this struct.
			named_backreference(codepoint_string n, bool ignore_case) :
				name(std::move(n)), case_insensitive(ignore_case) {
			}

			codepoint_string name; ///< Name of this backreference.
			bool case_insensitive = false; ///< Whether this backreference is case-insensitive.
		};
		/// A numbered subroutine.
		struct numbered_subroutine {
			/// Default constructor.
			numbered_subroutine() = default;
			/// Initializes \ref index.
			explicit numbered_subroutine(std::size_t id) : index(id) {
			}

			/// The index of the capture group. If this is 0, the subroutine references the entire pattern.
			std::size_t index = 0;
		};
		/// A named subroutine.
		struct named_subroutine {
			/// Default constructor.
			named_subroutine() = default;
			/// Initializes \ref name.
			explicit named_subroutine(codepoint_string n) : name(std::move(n)) {
			}

			codepoint_string name; ///< Name of the group.
		};
		/// Node that represents a class of characters.
		struct character_class {
			codepoint_range_list ranges; ///< Ranges in the character class.
			/// Indicates whether this matches all characters **not** in this class, as opposed to all characters
			/// in this class.
			bool is_negate = false;
			bool case_insensitive = false; ///< Whether this character class is case-insensitive.
		};
		/// A simple assertion.
		struct simple_assertion {
			/// The type of an assertion.
			enum class type : std::uint8_t {
				always_false, ///< An assertion that always fails.

				line_start, ///< Matches the start of the entire subject or the start of a new line.
				line_end, ///< Matches the end of the entire subject or the end of a new line.
				subject_start, ///< Matches the start of the entire subject.
				/// Matches the end of the entire subject, or a new line before the end.
				subject_end_or_trailing_newline,
				subject_end, ///< Matches the end of the entire subject.
				range_start, ///< Matches the start of the selected region of the subject.
			};

			type assertion_type = type::always_false; ///< The type of this assertion.
		};
		/// An assertion that tests whether the two consecutive characters around the current position belong in the
		/// specified character class.
		struct character_class_assertion {
			character_class char_class; ///< The character class.
			/// If \p true, this assertion is only true if we're at a boundary of the character class; otherwise this
			/// assertion is only true if we're not at a boundary.
			bool boundary = false;
		};

		/// A subexpression. This is not necessarily surrounded by brackets; this node simply represents any
		/// sequence of tokens.
		struct subexpression {
			/// The type of a subexpression.
			enum class type {
				normal, ///< Normal subexpressions.
				non_capturing, ///< This subexpression does not capture its contents.
				/// A non-capturing group. All captures within this alternative use the same capture indices.
				duplicate,
				/// A non-capturing group. The matcher should not retry when matching fails after this subexpression.
				atomic,
			};

			std::vector<node_ref> nodes; ///< Nodes in this sub-expression.
			codepoint_string capture_name; ///< Capture name.
			std::size_t capture_index = 0; ///< Capture index.
			/// The type of this subexpression. Since subexpressions are used in many scenarios, by default the
			/// subexpression does not capture.
			type subexpr_type = type::non_capturing;
		};

		/// Alternatives.
		struct alternative {
			std::vector<node_ref> alternatives; ///< Alternative expressions.
		};
		/// A node that represents a repetition.
		struct repetition {
			/// Indicates that there's no limit for the upper or lower bound of the number of repetitions.
			constexpr static std::size_t no_limit = std::numeric_limits<std::size_t>::max();

			/// The type of a repetition.
			enum class type : std::uint8_t {
				normal, ///< A normal greedy repetition.
				lazy, ///< A lazy repetition that matches as few as possible instead of as many as possible.
				posessed, ///< A posessed (atomic) repetition.
			};

			node_ref expression; ///< The expression to be repeated.
			std::size_t min = 0; ///< The minimum number of repetitions.
			std::size_t max = no_limit; ///< The maximum number of repetitions.
			type repetition_type = type::normal; ///< The type of this repetition.
		};
		/// An assertion that involves a subexpression (i.e., a lookahead/lookbehind).
		struct complex_assertion {
			node_ref expression; ///< The expression.
			bool backward = false; ///< If \p true, this is a lookbehind; otherwise this is a lookahead.
			bool negative = false; ///< If \p true, this assertion is satisifed iff the subexpression does not match.
			/// If \p true, allow backtracking into this assertion. This is only meaningful if \ref negative is
			/// \p false.
			bool non_atomic = false;
		};
		/// An conditional subexpression.
		struct conditional_expression {
			/// Used to define groups that can be later referenced. This condition is always false and there must
			/// only be one alternative.
			struct define {
			};
			/// A condition that tests if a particular capture group has matched.
			struct numbered_capture_available {
				std::size_t index = 0; ///< The index of this capture.
			};
			/// A condition that tests if a named capture is available.
			struct named_capture_available {
				codepoint_string name; ///< Name of the capture.
			};
			/// A complex assertion used as a condition.
			struct complex_assertion {
				node_ref node; ///< Node of the assertion.
			};

			/// Type for the condition.
			using condition_t = std::variant<
				define,
				numbered_capture_available,
				named_capture_available,
				complex_assertion
			>;

			condition_t condition; ///< The condition.
			node_ref if_true; ///< Subexpression that's matched if the condition matches.
			std::optional<node_ref> if_false; ///< Subexpression that's matched if the condition does not match.
		};
	}


	/// An abstract syntax tree for a regular expression.
	class ast {
		template <typename> friend class parser;
		friend compiler;
	public:
		/// A generic node.
		struct node {
			/// Variant type for node storage.
			using storage = std::variant<
				ast_nodes::error,
				ast_nodes::feature,
				ast_nodes::match_start_override,
				ast_nodes::literal,
				ast_nodes::numbered_backreference,
				ast_nodes::named_backreference,
				ast_nodes::numbered_subroutine,
				ast_nodes::named_subroutine,
				ast_nodes::character_class,
				ast_nodes::simple_assertion,
				ast_nodes::character_class_assertion,
				ast_nodes::subexpression,
				ast_nodes::alternative,
				ast_nodes::repetition,
				ast_nodes::complex_assertion,
				ast_nodes::conditional_expression
			>;

			storage value; ///< The value of this node.

			/// Shorthand for \p std::holds_alternative().
			template <typename Node> [[nodiscard]] bool is() const {
				return std::holds_alternative<Node>(value);
			}
		};
		/// Analysis for an entire AST.
		struct analysis {
			friend ast;
		public:
			/// Returns the analysis result for the specified node.
			const ast_nodes::analysis get_for(ast_nodes::node_ref n) const {
				return _node_analysis[n._index];
			}
		protected:
			std::vector<ast_nodes::analysis> _node_analysis; ///< Analysis result for all nodes.
		};
		/// Dumps an AST.
		template <typename Stream> struct dumper {
		public:
			/// Initializes \ref _stream and \ref _tree.
			explicit dumper(Stream &s, const ast &tree) : _stream(s), _tree(tree) {
			}

			/// Dumps a \ref ast_nodes::error.
			void dump(const ast_nodes::error&) {
				_indent();
				_stream << "©¤©¤ [ERROR]\n";
			}
			/// Dumps a \ref ast_nodes::feature.
			void dump(const ast_nodes::feature &n) {
				_indent();
				_stream << "©¤©¤ [feature: ";
				for (codepoint cp : n.identifier) {
					_stream << reinterpret_cast<const char*>(encodings::utf8::encode_codepoint(cp).c_str());
				}
				_stream << "]\n";
			}
			/// Dumps a \ref ast_nodes::match_start_override.
			void dump(const ast_nodes::match_start_override&) {
				_indent();
				_stream << "©¤©¤ [reset match start]\n";
			}
			/// Dumps a \ref ast_nodes::literal.
			void dump(const ast_nodes::literal &n) {
				_indent();
				_stream << "©¤©¤ [literal: \"";
				for (codepoint cp : n.contents) {
					_stream << reinterpret_cast<const char*>(encodings::utf8::encode_codepoint(cp).c_str());
				}
				_stream << "\"";
				if (n.case_insensitive) {
					_stream << "/i";
				}
				_stream << "]\n";
			}
			/// Dumps a \ref ast_nodes::numbered_backreference.
			void dump(const ast_nodes::numbered_backreference &n) {
				_indent();
				_stream << "©¤©¤ [backreference: #" << n.index;
				if (n.case_insensitive) {
					_stream << "/i";
				}
				_stream << "]\n";
			}
			/// Dumps a \ref ast_nodes::named_backreference.
			void dump(const ast_nodes::named_backreference &n) {
				_indent();
				_stream << "©¤©¤ [backreference: \"";
				for (codepoint cp : n.name) {
					_stream << reinterpret_cast<const char*>(encodings::utf8::encode_codepoint(cp).c_str());
				}
				_stream << "\"";
				if (n.case_insensitive) {
					_stream << "/i";
				}
				_stream << "]\n";
			}
			/// Dumps a \ref ast_nodes::numbered_subroutine.
			void dump(const ast_nodes::numbered_subroutine &n) {
				_indent();
				_stream << "©¤©¤ [subroutine: #" << n.index << "]\n";
			}
			/// Dumps a \ref ast_nodes::named_subroutine.
			void dump(const ast_nodes::named_subroutine &n) {
				_indent();
				_stream << "©¤©¤ [subroutine: \"";
				for (codepoint cp : n.name) {
					_stream << reinterpret_cast<const char*>(encodings::utf8::encode_codepoint(cp).c_str());
				}
				_stream << "\"" << "]\n";
			}
			/// Dumps a \ref ast_nodes::character_class.
			void dump(const ast_nodes::character_class &n) {
				_indent();
				_stream << "©¤©¤ [character class " << (n.is_negate ? "[!]" : "") << ": ";
				_dump_character_class(n.ranges);
				_stream << "]\n";
			}
			/// Dumps a \ref ast_nodes::subexpression.
			void dump(const ast_nodes::subexpression &n) {
				_indent();
				if (n.nodes.empty()) {
					_stream << "©¤";
				} else {
					_stream << "©Ð";
				}
				_stream << "©¤ [subexpression";
				if (n.subexpr_type == ast_nodes::subexpression::type::normal) {
					_stream << " #" << n.capture_index;
					if (!n.capture_name.empty()) {
						_stream << " \"";
						for (codepoint cp : n.capture_name) {
							_stream << reinterpret_cast<const char*>(
								encodings::utf8::encode_codepoint(cp).c_str()
							);
						}
						_stream << "\"";
					}
				}
				switch (n.subexpr_type) {
				case ast_nodes::subexpression::type::normal:
					break;
				case ast_nodes::subexpression::type::non_capturing:
					_stream << " (non-capturing)";
					break;
				case ast_nodes::subexpression::type::duplicate:
					_stream << " (duplicate)";
					break;
				case ast_nodes::subexpression::type::atomic:
					_stream << " (atomic)";
					break;
				}
				_stream << "]\n";
				_branch.emplace_back(true);
				for (std::size_t i = 0; i < n.nodes.size(); ++i) {
					if (i + 1 == n.nodes.size()) {
						_branch.back() = false;
					}
					dump(n.nodes[i]);
				}
				_branch.pop_back();
			}
			/// Dumps a \ref ast_nodes::alternative.
			void dump(const ast_nodes::alternative &n) {
				_indent();
				_stream << "©Ð©¤ [alternative]\n";
				_branch.emplace_back(true);
				for (std::size_t i = 0; i < n.alternatives.size(); ++i) {
					if (i + 1 == n.alternatives.size()) {
						_branch.back() = false;
					}
					dump(n.alternatives[i]);
				}
				_branch.pop_back();
			}
			/// Dumps a \ref ast_nodes::repetition.
			void dump(const ast_nodes::repetition &n) {
				_indent();
				_stream << "©Ð©¤ [repetition";
				switch (n.repetition_type) {
				case ast_nodes::repetition::type::normal:
					break;
				case ast_nodes::repetition::type::lazy:
					_stream << " (lazy)";
				case ast_nodes::repetition::type::posessed:
					_stream << " (posessed)";
					break;
				}
				_stream << " min: " << n.min << "  max: " << n.max << "]\n";
				_branch.emplace_back(false);
				dump(n.expression);
				_branch.pop_back();
			}
			/// Dumps a \ref ast_nodes::simple_assertion.
			void dump(const ast_nodes::simple_assertion &n) {
				_indent();
				_stream << "©¤©¤ [assertion (simple) type: " << static_cast<int>(n.assertion_type) << "]\n";
			}
			/// Dumps a \ref ast_nodes::character_class_assertion.
			void dump(const ast_nodes::character_class_assertion &n) {
				_indent();
				_stream << "©¤©¤ [assertion (char class " << (n.boundary ? "boundary" : "non-boundary") << ") ranges: ";
				_dump_character_class(n.char_class.ranges);
				_stream << "]\n";
			}
			/// Dumps a \ref ast_nodes::complex_assertion.
			void dump(const ast_nodes::complex_assertion &n) {
				_indent();
				_stream << "©Ð©¤ [assertion (complex)";
				if (n.negative) {
					_stream << " (neg)";
				}
				if (n.backward) {
					_stream << " (back)";
				}
				if (n.non_atomic) {
					_stream << " (non-atomic)";
				}
				_stream << "]\n";
				_branch.emplace_back(false);
				dump(n.expression);
				_branch.pop_back();
			}
			/// Dumps a \ref ast_nodes::conditional_expression.
			void dump(const ast_nodes::conditional_expression &n) {
				_indent();
				_stream << "©Ð©¤ [conditional: ";
				std::visit(
					[this](auto &&cond) {
						_dump_condition(cond);
					},
					n.condition
				);
				_stream << "]\n";

				_branch.emplace_back(true);
				if (std::holds_alternative<ast_nodes::conditional_expression::complex_assertion>(n.condition)) {
					const auto &ass = std::get<ast_nodes::conditional_expression::complex_assertion>(n.condition);
					dump(ass.node);
				}

				if (!n.if_false.has_value()) {
					_branch.back() = false;
				}
				dump(n.if_true);

				if (n.if_false) {
					_branch.back() = false;
					dump(n.if_false.value());
				}
				_branch.pop_back();
			}

			/// Dumps a \ref node.
			void dump(const ast_nodes::node_ref &n) {
				std::visit(
					[this](auto &&concrete_node) {
						dump(concrete_node);
					},
					_tree.get_node(n).value
				);
			}
		protected:
			std::vector<bool> _branch; ///< Indicates whether there's an active branch at the given level.
			Stream &_stream; ///< The output stream.
			const ast &_tree; ///< The syntax tree.

			/// Indents to the correct level.
			void _indent() {
				if (_branch.size() > 0) {
					_stream << "  ";
				}
				for (std::size_t i = 0; i + 1 < _branch.size(); ++i) {
					_stream << (_branch[i] ? "©¦ " : "  ");
				}
				if (_branch.empty()) {
					_stream << ">©¤";
				} else if (_branch.back()) {
					_stream << "©À©¤";
				} else {
					_stream << "©¸©¤";
				}
			}
			/// Dumps the given list of codepoint ranges.
			void _dump_character_class(const codepoint_range_list &ranges) {
				constexpr std::size_t _max_range_count = 5;

				bool first = true;
				for (std::size_t i = 0; i < ranges.ranges.size() && i < _max_range_count; ++i) {
					if (!first) {
						_stream << ", ";
					}
					first = false;
					auto [beg, end] = ranges.ranges[i];
					if (beg == end) {
						_stream << beg;
					} else {
						_stream << beg << " - " << end;
					}
				}
				if (ranges.ranges.size() > _max_range_count) {
					_stream << ", ...";
				}
			}

			/// Dumps a definition condition.
			void _dump_condition(const ast_nodes::conditional_expression::define&) {
				_stream << "<define>";
			}
			/// Dumps a condition that checks for a numbered assertion.
			void _dump_condition(const ast_nodes::conditional_expression::numbered_capture_available &cap) {
				_stream << "capture #" << cap.index;
			}
			/// Dumps a condition that checks for a named assertion.
			void _dump_condition(const ast_nodes::conditional_expression::named_capture_available &cap) {
				_stream << "capture \"";
				for (codepoint cp : cap.name) {
					_stream << reinterpret_cast<const char*>(encodings::utf8::encode_codepoint(cp).c_str());
				}
				_stream << "\"";
			}
			/// Dumps an assertion that's used as a condition.
			void _dump_condition(const ast_nodes::conditional_expression::complex_assertion&) {
				_stream << "<assertion>";
			}
		};

		/// Returns the referenced node.
		[[nodiscard]] node &get_node(ast_nodes::node_ref n) {
			return _nodes[n._index];
		}
		/// \overload
		[[nodiscard]] const node &get_node(ast_nodes::node_ref n) const {
			return _nodes[n._index];
		}

		/// Creates a new node and returns it.
		[[nodiscard]] ast_nodes::node_ref create_node() {
			ast_nodes::node_ref result(_nodes.size());
			_nodes.emplace_back();
			return result;
		}
		/// Creates a new node of the specified type.
		template <typename Node> [[nodiscard]] std::pair<ast_nodes::node_ref, Node&> create_node() {
			ast_nodes::node_ref result(_nodes.size());
			Node &res_ref = _nodes.emplace_back().value.emplace<Node>();
			return { result, res_ref };
		}

		/// Analyzes the entire AST.
		[[nodiscard]] analysis analyze() const;

		/// Returns a reference to the root node.
		[[nodiscard]] ast_nodes::node_ref root() const {
			return _root;
		}

		/// Dumps the entire AST to the given stream.
		template <typename Stream> [[nodiscard]] dumper<Stream> create_dumper(Stream &s) const {
			return dumper<Stream>(s, *this);
		}
	protected:
		/// Additional information about an analysis.
		struct _analysis_context {
			/// Nodes queued to have their \ref _collect_dependencies() function called.
			std::deque<ast_nodes::node_ref> dependency_queue;
			/// Nodes queued to have their \ref _end_analysis() function called.
			std::deque<ast_nodes::node_ref> end_stack;
			analysis output; ///< Analysis result.
		};

		ast_nodes::node_ref _root; ///< Reference to the root node.
		std::deque<node> _nodes; ///< The list of nodes.


		/// Forwards the call to the function that accepts the corresponding node type.
		void _collect_dependencies(ast_nodes::node_ref n, _analysis_context &ctx) const {
			std::visit(
				[&, this](const auto &val) {
					_collect_dependencies(val, ctx);
				},
				get_node(n).value
			);
		}
		/// Fallback for nodes that don't have dependencies.
		template <typename Node> void _collect_dependencies(const Node&, _analysis_context&) const {
		}
		// TODO: analyze referenced group for numbered & named backreferences?
		/// Marks all subnodes as dependencies.
		void _collect_dependencies(const ast_nodes::subexpression &expr, _analysis_context &ctx) const {
			for (auto n : expr.nodes) {
				ctx.dependency_queue.emplace_back(n);
			}
		}
		/// Marks all alternatives as dependencies.
		void _collect_dependencies(const ast_nodes::alternative &expr, _analysis_context &ctx) const {
			for (auto n : expr.alternatives) {
				ctx.dependency_queue.emplace_back(n);
			}
		}
		/// Marks the repeated sequence as a dependency.
		void _collect_dependencies(const ast_nodes::repetition &expr, _analysis_context &ctx) const {
			ctx.dependency_queue.emplace_back(expr.expression);
		}
		/// Marks the expression as a dependency.
		void _collect_dependencies(const ast_nodes::complex_assertion &expr, _analysis_context &ctx) const {
			ctx.dependency_queue.emplace_back(expr.expression);
		}
		/// Marks the clauses and (if necessary) the condition as a dependency.
		void _collect_dependencies(const ast_nodes::conditional_expression &expr, _analysis_context &ctx) const {
			ctx.dependency_queue.emplace_back(expr.if_true);
			if (expr.if_false) {
				ctx.dependency_queue.emplace_back(expr.if_false.value());
			}
			if (std::holds_alternative<ast_nodes::conditional_expression::complex_assertion>(expr.condition)) {
				ctx.dependency_queue.emplace_back(
					std::get<ast_nodes::conditional_expression::complex_assertion>(expr.condition).node
				);
			}
		}

		/// Forwards the call to the function that accepts the corresponding node type, and writes the analysis
		/// result back to the \ref _analysis_context.
		void _analyze(ast_nodes::node_ref n, _analysis_context &ctx) const {
			ctx.output._node_analysis[n._index] = std::visit(
				[&, this](const auto &val) {
					return _analyze(val, ctx);
				},
				get_node(n).value
			);
		}
		/// Analyzes a \ref ast_nodes::error node.
		[[nodiscard]] ast_nodes::analysis _analyze(const ast_nodes::error&, _analysis_context&) const {
			ast_nodes::analysis result;
			result.maximum_length = 0;
			result.maximum_length = 0;
			return result;
		}
		/// Analyzes a \ref ast_nodes::feature node.
		[[nodiscard]] ast_nodes::analysis _analyze(const ast_nodes::feature&, _analysis_context&) const {
			ast_nodes::analysis result;
			result.maximum_length = 0;
			result.maximum_length = 0;
			return result;
		}
		/// Analyzes a \ref ast_nodes::match_start_override node.
		[[nodiscard]] ast_nodes::analysis _analyze(
			const ast_nodes::match_start_override&, _analysis_context&
		) const {
			ast_nodes::analysis result;
			result.maximum_length = 0;
			result.maximum_length = 0;
			return result;
		}
		/// Analyzes a \ref ast_nodes::literal node.
		[[nodiscard]] ast_nodes::analysis _analyze(const ast_nodes::literal &n, _analysis_context&) const {
			ast_nodes::analysis result;
			result.minimum_length = result.maximum_length = n.contents.size();
			return result;
		}
		/// Analyzes a \ref ast_nodes::numbered_backreference node.
		[[nodiscard]] ast_nodes::analysis _analyze(
			const ast_nodes::numbered_backreference&, _analysis_context&
		) const {
			ast_nodes::analysis result;
			// TODO we can do more analysis here
			return result;
		}
		/// Analyzes a \ref ast_nodes::named_backreference node.
		[[nodiscard]] ast_nodes::analysis _analyze(const ast_nodes::named_backreference&, _analysis_context&) const {
			ast_nodes::analysis result;
			// TODO we can do more analysis here
			return result;
		}
		/// Analyzes a \ref ast_nodes::numbered_subroutine node.
		[[nodiscard]] ast_nodes::analysis _analyze(const ast_nodes::numbered_subroutine&, _analysis_context&) const {
			ast_nodes::analysis result;
			// TODO we can do more analysis here
			return result;
		}
		/// Analyzes a \ref ast_nodes::named_subroutine node.
		[[nodiscard]] ast_nodes::analysis _analyze(const ast_nodes::named_subroutine&, _analysis_context&) const {
			ast_nodes::analysis result;
			// TODO we can do more analysis here
			return result;
		}
		/// Analyzes a \ref ast_nodes::character_class node.
		[[nodiscard]] ast_nodes::analysis _analyze(const ast_nodes::character_class&, _analysis_context&) const {
			ast_nodes::analysis result;
			result.minimum_length = result.maximum_length = 1;
			return result;
		}
		/// Analyzes a \ref ast_nodes::simple_assertion node.
		[[nodiscard]] ast_nodes::analysis _analyze(const ast_nodes::simple_assertion&, _analysis_context&) const {
			ast_nodes::analysis result;
			result.minimum_length = result.maximum_length = 0;
			return result;
		}
		/// Analyzes a \ref ast_nodes::character_class_assertion node.
		[[nodiscard]] ast_nodes::analysis _analyze(
			const ast_nodes::character_class_assertion&, _analysis_context&
		) const {
			ast_nodes::analysis result;
			result.minimum_length = result.maximum_length = 0;
			return result;
		}
		/// Analyzes a \ref ast_nodes::subexpression node.
		[[nodiscard]] ast_nodes::analysis _analyze(const ast_nodes::subexpression&, _analysis_context&) const;
		/// Analyzes a \ref ast_nodes::alternative node.
		[[nodiscard]] ast_nodes::analysis _analyze(const ast_nodes::alternative&, _analysis_context&) const;
		/// Analyzes a \ref ast_nodes::repetition node.
		[[nodiscard]] ast_nodes::analysis _analyze(const ast_nodes::repetition&, _analysis_context&) const;
		/// Analyzes a \ref ast_nodes::complex_assertion node.
		[[nodiscard]] ast_nodes::analysis _analyze(const ast_nodes::complex_assertion&, _analysis_context&) const {
			ast_nodes::analysis result;
			result.minimum_length = result.maximum_length = 0;
			return result;
		}
		/// Analyzes a \ref ast_nodes::conditional_expression node.
		[[nodiscard]] ast_nodes::analysis _analyze(
			const ast_nodes::conditional_expression&, _analysis_context&
		) const;
	};
}
