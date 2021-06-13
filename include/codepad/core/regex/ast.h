// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// The parsed syntax tree for regular expressions.

#include <variant>

#include "codepad/core/unicode/common.h"

namespace codepad::regex::ast {
	struct node;

	namespace nodes {
		/// A node indicating an error.
		struct error {
		};

		/// Node used to signal a feature is enabled.
		struct feature {
			codepoint_string identifier; ///< String used to identify the feature.
		};
		/// A node that contains a string literal.
		struct literal {
			/// Returns a \ref literal node that contains only the given codepoint.
			[[nodiscard]] inline static literal from_codepoint(codepoint cp) {
				literal result;
				result.contents.push_back(cp);
				return result;
			}

			codepoint_string contents; ///< The literal.
		};
		/// A backreference.
		struct backreference {
			std::variant<std::size_t, codepoint_string> index; ///< The index of this backreference.
			/// Indicates that this may be an octal character code instead of a backreference.
			bool is_ambiguous = false;
		};
		/// Node that represents a class of characters.
		struct character_class {
			codepoint_range_list ranges; ///< Ranges in the character class.
			/// Indicates whether this matches all characters **not** in this class, as opposed to all characters
			/// in this class.
			bool is_negate = false;

			/// Handles negation. This function assumes that \ref sort_and_compact() has been called.
			[[nodiscard]] codepoint_range_list get_effective_ranges() const {
				if (is_negate) {
					codepoint last = 0;
					codepoint_range_list result;
					auto iter = ranges.ranges.begin();
					if (!ranges.ranges.empty() && ranges.ranges.front().first == 0) {
						last = ranges.ranges.front().last + 1;
						++iter;
					}
					for (; iter != ranges.ranges.end(); ++iter) {
						result.ranges.emplace_back(last, iter->first - 1);
						last = iter->last + 1;
					}
					if (last <= unicode::codepoint_max) {
						result.ranges.emplace_back(last, unicode::codepoint_max);
					}
					return result;
				}
				return ranges;
			}
		};

		/// A subexpression. This is not necessarily surrounded by brackets; this node simply represents any
		/// sequence of tokens.
		struct subexpression {
			/// The type of a subexpression.
			enum class type {
				normal, ///< Normal subexpressions.
				non_capturing, ///< This subexpression does not capture its contents.
				duplicate, ///< All captures within this alternative use the same capture indices.
				atomic, ///< The matcher should not retry when matching fails after this subexpression.
			};

			std::vector<node> nodes; ///< Nodes in this sub-expression.
			std::variant<std::size_t, codepoint_string> capture_index; ///< Capture index.
			/// The type of this subexpression or assertion. Since subexpressions are used in many contexts, by
			/// default the subexpression does not capture.
			type type_or_assertion = type::non_capturing;
		};

		/// Alternatives.
		struct alternative {
			std::vector<subexpression> alternatives; ///< Alternative expressions.
		};
		/// A node that represents a repetition.
		struct repetition {
			/// Indicates that there's no limit for the upper or lower bound of the number of repetitions.
			constexpr static std::size_t no_limit = std::numeric_limits<std::size_t>::max();

			subexpression expression; ///< The expression to be repeated.
			std::size_t min = 0; ///< The minimum number of repetitions.
			std::size_t max = no_limit; ///< The maximum number of repetitions.
		};
	}

	/// A generic node.
	struct node {
		/// Variant type for node storage.
		using storage = std::variant<
			nodes::error,
			nodes::feature,
			nodes::literal,
			nodes::backreference,
			nodes::character_class,
			nodes::subexpression,
			nodes::alternative,
			nodes::repetition
		>;

		storage value; ///< The value of this node.

		/// Shorthand for \p std::holds_alternative().
		template <typename Node> [[nodiscard]] bool is() const {
			return std::holds_alternative<Node>(value);
		}
	};


	/// Dumps an AST.
	template <typename Stream> struct dumper {
	public:
		/// Initializes \ref _stream.
		explicit dumper(Stream &s) : _stream(s) {
		}

		/// Dumps a \ref nodes::error.
		void dump(const nodes::error&) {
			_indent();
			_stream << "©¤©¤ [ERROR]\n";
		}
		/// Dumps a \ref nodes::feature.
		void dump(const nodes::feature &n) {
			_indent();
			_stream << "©¤©¤ [feature: ";
			for (codepoint cp : n.identifier) {
				_stream << reinterpret_cast<const char*>(encodings::utf8::encode_codepoint(cp).c_str());
			}
			_stream << "]\n";
		}
		/// Dumps a \ref nodes::literal.
		void dump(const nodes::literal &n) {
			_indent();
			_stream << "©¤©¤ [literal: \"";
			for (codepoint cp : n.contents) {
				_stream << reinterpret_cast<const char*>(encodings::utf8::encode_codepoint(cp).c_str());
			}
			_stream << "\"]\n";
		}
		/// Dumps a \ref nodes::backreference.
		void dump(const nodes::backreference &n) {
			_indent();
			_stream << "©¤©¤ [backreference: ";
			if (std::holds_alternative<codepoint_string>(n.index)) {
				_stream << "\"";
				const auto &id = std::get<codepoint_string>(n.index);
				for (codepoint cp : id) {
					_stream << reinterpret_cast<const char*>(encodings::utf8::encode_codepoint(cp).c_str());
				}
				_stream << "\"";
			} else {
				_stream << "#" << std::get<std::size_t>(n.index);
			}
			_stream << "]\n";
		}
		/// Dumps a \ref nodes::character_class.
		void dump(const nodes::character_class &n) {
			_indent();
			_stream << "©¤©¤ [character class " << (n.is_negate ? "[!]" : "") << ": ";
			bool first = true;
			for (auto [beg, end] : n.ranges.ranges) {
				if (!first) {
					_stream << ", ";
				}
				first = false;
				if (beg == end) {
					_stream << beg;
				} else {
					_stream << beg << " - " << end;
				}
			}
			_stream << "]\n";
		}
		/// Dumps a \ref nodes::subexpression.
		void dump(const nodes::subexpression &n) {
			_indent();
			if (n.nodes.empty()) {
				_stream << "©¤";
			} else {
				_stream << "©Ð";
			}
			_stream << "©¤ [subexpression";
			if (n.type_or_assertion != ast::nodes::subexpression::type::non_capturing) {
				_stream << " #";
				if (std::holds_alternative<std::size_t>(n.capture_index)) {
					_stream << std::get<std::size_t>(n.capture_index);
				} else {
					auto &str = std::get<codepoint_string>(n.capture_index);
					for (codepoint cp : str) {
						_stream << reinterpret_cast<const char*>(
							encodings::utf8::encode_codepoint(cp).c_str()
							);
					}
				}
			}
			switch (n.type_or_assertion) {
			case ast::nodes::subexpression::type::normal:
				break;
			case ast::nodes::subexpression::type::non_capturing:
				_stream << " (non-capturing)";
				break;
			case ast::nodes::subexpression::type::duplicate:
				_stream << " (duplicate)";
				break;
			case ast::nodes::subexpression::type::atomic:
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
		/// Dumps a \ref nodes::alternative.
		void dump(const nodes::alternative &n) {
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
		/// Dumps a \ref nodes::repetition.
		void dump(const nodes::repetition &n) {
			_indent();
			_stream << "©Ð©¤ [repetition  min: " << n.min << "  max: " << n.max << "]\n";
			_branch.emplace_back(false);
			dump(n.expression);
			_branch.pop_back();
		}

		/// Dumps a \ref node.
		void dump(const node &n) {
			std::visit(
				[this](auto &&concrete_node) {
					dump(concrete_node);
				},
				n.value
					);
		}
	protected:
		std::vector<bool> _branch; ///< Indicates whether there's an active branch at the given level.
		Stream &_stream; ///< The output stream.

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
	};
	/// Shorthand for creating a \ref dumper.
	template <typename Stream> dumper<Stream> make_dumper(Stream &s) {
		return dumper<Stream>(s);
	}
}
