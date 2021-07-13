// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// The parsed syntax tree for regular expressions.

#include <variant>

#include "codepad/core/unicode/common.h"
#include "codepad/core/assert.h"
#include "codepad/core/encodings.h"

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
			bool case_insensitive = false; ///< Whether this literal is matched in a case-insensitive manner.
		};
		/// A backreference.
		struct backreference {
			/// Default constructor.
			backreference() = default;
			/// Initializes this backreference with the given numerical index.
			explicit backreference(std::size_t id) : index(std::in_place_type<std::size_t>, id) {
			}
			/// Initializes this backreference with the given string index.
			explicit backreference(codepoint_string id) : index(std::in_place_type<codepoint_string>, std::move(id)) {
			}

			std::variant<std::size_t, codepoint_string> index; ///< The index of this backreference.
		};
		/// Node that represents a class of characters.
		struct character_class {
			codepoint_range_list ranges; ///< Ranges in the character class.
			/// Indicates whether this matches all characters **not** in this class, as opposed to all characters
			/// in this class.
			bool is_negate = false;
			bool case_insensitive = false; ///< Whether this character class is case-insensitive.

			/// Handles negation. This function assumes that \ref sort_and_compact() has been called.
			[[nodiscard]] codepoint_range_list get_effective_ranges() const;
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

			std::vector<node> nodes; ///< Nodes in this sub-expression.
			codepoint_string capture_name; ///< Capture name.
			std::size_t capture_index = 0; ///< Capture index.
			/// The type of this subexpression. Since subexpressions are used in many scenarios, by default the
			/// subexpression does not capture.
			type subexpr_type = type::non_capturing;
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
			/// Whether this repetition matches as few repetitions as possible, instead of as many as possible.
			bool lazy = false;
		};
		/// An assertion.
		struct assertion {
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

				/// Matches a character boundary if one character is in a character class while the other isn't. The
				/// character class is given by the first node of \ref expression.
				character_class_boundary,
				/// Matches a character boundary if both character are in the character class or if both character
				/// are not in the character class. The character class is given by the first node of
				/// \ref expression.
				character_class_nonboundary,

				positive_lookahead, ///< Lookahead assertion that expects to match \ref expression.
				negative_lookahead, ///< Lookahead assertion that expects **not** to match \ref expression.
				positive_lookbehind, ///< Lookbehind assertion that expects to match \ref expression.
				negative_lookbehind, ///< Lookbehind assertion that expects **not** to match \ref expression.

				/// The first assertion type that includes a character class in \ref expression.
				character_class_first = character_class_boundary,
				complex_first = positive_lookahead ///< The first assertion type for which \ref expression is tested.
			};

			subexpression expression; ///< The expression used by this assertion.
			type assertion_type = type::always_false; ///< The type of this assertion.
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
			nodes::repetition,
			nodes::assertion
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
			_dump_character_class(n.ranges);
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
			if (n.subexpr_type != ast::nodes::subexpression::type::non_capturing) {
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
			_stream << "©Ð©¤ [repetition ";
			if (n.lazy) {
				_stream << " lazy";
			}
			_stream << " min: " << n.min << "  max: " << n.max << "]\n";
			_branch.emplace_back(false);
			dump(n.expression);
			_branch.pop_back();
		}
		/// Dumps a \ref nodes::assertion.
		void dump(const nodes::assertion &n) {
			_indent();
			if (n.assertion_type >= ast::nodes::assertion::type::complex_first) {
				_stream << "©Ð©¤ [assertion type: " << static_cast<int>(n.assertion_type) << "]\n";
				_branch.emplace_back(false);
				dump(n.expression);
				_branch.pop_back();
			} else if (n.assertion_type >= ast::nodes::assertion::type::character_class_first) {
				assert_true_logical(
					n.expression.nodes.size() == 1 &&
					std::holds_alternative<ast::nodes::character_class>(n.expression.nodes[0].value),
					"invalid character class assertion"
				);
				_stream << "©¤©¤ [assertion type: " << static_cast<int>(n.assertion_type) << " ranges: ";
				_dump_character_class(std::get<ast::nodes::character_class>(n.expression.nodes[0].value).ranges);
				_stream << "]\n";
			} else {
				_stream << "©¤©¤ [assertion type: " << static_cast<int>(n.assertion_type) << "]\n";
			}
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
	};
	/// Shorthand for creating a \ref dumper.
	template <typename Stream> [[nodiscard]] dumper<Stream> make_dumper(Stream &s) {
		return dumper<Stream>(s);
	}
}
