// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes used to record and manage font color, style, etc. in a \ref codepad::editors::code::interpretation.

#include <deque>

#include <codepad/core/red_black_tree.h>
#include <codepad/ui/renderer.h>

#include "../overlapping_range_registry.h"
#include "../theme_manager.h"

namespace codepad::editors::code {
	class interpretation;

	/// Records the text's theme across the entire buffer.
	struct text_theme_data {
		/// Stores the \ref text_theme_specification and a \p std::int32_t for additional debugging information for a
		/// range of text.
		struct range_value {
			/// Default constructor.
			range_value() = default;
			/// Initializes this struct without a cookie. Also an implicit conversion from
			/// \ref text_theme_specification.
			range_value(text_theme_specification v) : value(v) {
			}
			/// Initializes all fields of this struct.
			range_value(text_theme_specification v, std::int32_t c) : value(v), cookie(c) {
			}

			text_theme_specification value; ///< The theme of this particular range.
			std::int32_t cookie = 0; ///< Cookie used to provide additional debugging information.
		};
		/// The \ref overlapping_range_registry type used to hold all highlighted ranges.
		using storage = overlapping_range_registry<range_value>;

		storage ranges; ///< Highlighted ranges.

		/// Adds a new highlighted range.
		void add_range(std::size_t s, std::size_t pe, range_value val) {
			ranges.insert_range(s, pe - s, val);
		}
		/// Called when the interpretation is modified to update the theme data associated with it.
		void on_modification(std::size_t start, std::size_t erased_length, std::size_t inserted_length) {
			ranges.on_modification(start, erased_length, inserted_length);
		}

		/// Clears all highlighted ranges.
		void clear() {
			ranges.clear();
		}
	};

	/// Keeps track of numerous sets of \ref text_theme_data providers with varying priorities.
	class text_theme_provider_registry {
	public:
		/// The priority of a provider. Instances of this enum can take any value within the range of
		/// <tt>unsigned char</tt>; the enum values defined here are only for reference.
		enum class priority : unsigned char {
			/// Indicates that this highlight provider is obtained by analyzing the text only.
			approximate = 100,
			/// Indicates that this highlight provider is obtained by analyzing the semantics of tokens in this file,
			/// which may involve analyzing other dependencies.
			accurate = 200
		};
	protected:
		/// A provider in this registry.
		struct _entry {
			text_theme_data theme; ///< The theme obtained from this provider.
			priority provider_priority = priority::approximate; ///< The priority of this provider.
		};
	public:
		struct token;
		/// Used to modify the \ref text_theme_data of a provider.
		struct provider_modifier {
			friend token;
		public:
			/// No copy construction.
			provider_modifier(const provider_modifier&) = delete;
			/// Invokes \ref interpretation::appearance_changed.
			~provider_modifier();

			/// Used to access the \ref text_theme_data.
			[[nodiscard]] text_theme_data *operator->() const {
				return &_data;
			}
			/// Used to access the \ref text_theme_data.
			[[nodiscard]] text_theme_data &operator*() const {
				return _data;
			}
		protected:
			interpretation &_interp; ///< The associated \ref interpretation.
			text_theme_data &_data; ///< The associated \ref text_theme_data.

			/// Initializes \ref _data.
			provider_modifier(interpretation &interp, text_theme_data &data) : _interp(interp), _data(data) {
			}
		};
		/// A token used by providers to access the underlying \ref text_theme_data.
		struct token {
			friend text_theme_provider_registry;
		public:
			/// Default constructor.
			token() = default;

			/// Returns a \ref provider_modifier for this provider.
			[[nodiscard]] provider_modifier get_modifier() {
				return provider_modifier(*_interpretation, _it->theme);
			}
			/// Returns the readonly \ref text_theme_specification.
			[[nodiscard]] const text_theme_data &get_readonly() const {
				return _it->theme;
			}
		protected:
			std::list<_entry>::iterator _it; ///< Iterator to the \ref _entry.
			interpretation *_interpretation = nullptr; ///< The \ref interpretation that the provider belongs to.

			/// Initializes all fields of this token.
			token(interpretation &interp, std::list<_entry>::iterator it) : _it(it), _interpretation(&interp) {
			}
		};

		/// Used to iterate through the document, and to find the next theme change during the process.
		struct iterator {
		public:
			/// Default constructor.
			iterator() = default;
			/// Initializes this iterator using the given \ref text_theme_provider_registry.
			iterator(const text_theme_provider_registry &provs, const text_theme_specification &def_theme) :
				default_theme(def_theme), _providers(&provs), _layers(provs._providers.size()) {
			}

			/// Repositions this iterator.
			void reposition(std::size_t pos) {
				auto it = _layers.begin();
				bool theme_set = false;
				for (auto p = _providers->_providers.begin(); p != _providers->_providers.end(); ++p, ++it) {
					it->reposition(p->theme.ranges, pos);
					if (!theme_set && !it->active_stack.empty()) {
						current_theme = it->active_stack.back().theme;
						theme_set = true;
					}
				}
				if (!theme_set) {
					current_theme = default_theme;
				}
			}
			/// Returns the maximum number of steps that can be taken before a theme change happens.
			[[nodiscard]] std::size_t forecast(std::size_t pos) const {
				// earliest start position from all previous providers
				std::size_t next_start = std::numeric_limits<std::size_t>::max();
				auto prov = _providers->_providers.begin();
				for (auto it = _layers.begin(); it != _layers.end(); ++prov, ++it) {
					if (!it->active_stack.empty()) { // next event must happen within this layer
						std::size_t min_end = next_start;
						for (auto &hl : it->active_stack) {
							min_end = std::min(min_end, hl.end);
						}
						return min_end - pos;
					}
					if (it->iter.get_iterator() != prov->theme.ranges.end()) {
						next_start = std::min(next_start, it->iter.get_range_start());
					}
				}
				return next_start - pos;
			}
			/// Moves forward to the specified position.
			///
			/// \return Whether the current theme has been changed.
			void move_forward(std::size_t pos) {
				auto prov = _providers->_providers.begin();
				bool theme_set = false;
				for (auto it = _layers.begin(); it != _layers.end(); ++prov, ++it) {
					it->move_forward(prov->theme.ranges, pos);
					if (!theme_set && !it->active_stack.empty()) {
						current_theme = it->active_stack.back().theme;
						theme_set = true;
					}
				}
				if (!theme_set) {
					current_theme = default_theme;
				}
			}

			text_theme_specification
				default_theme, ///< The default theme that will be used when no provider is active.
				current_theme; ///< Current text theme.
		protected:
			/// Used to keep track of highlight information from a single provider.
			struct _highlight_layer {
				/// A highlight that's currently active. Being active means the highlight overlaps with the current
				/// region, but doesn't necessarily mean that it will be displayed because it may get overriden.
				struct active_highlight {
					/// Default constructor.
					active_highlight() = default;
					/// Initializes all fields of this struct.
					active_highlight(text_theme_specification spec, std::size_t e) : theme(spec), end(e) {
					}

					text_theme_specification theme; ///< The \ref text_theme_specification for this highlight.
					std::size_t end = 0; ///< The end position of this highlight.
				};

				std::deque<active_highlight> active_stack; ///< The stack of active highlights.
				/// Iterator to the next range that starts after the current position.
				text_theme_data::storage::iterator_position iter;

				/// Repositions \ref iter and updates \ref active_stack.
				void reposition(const text_theme_data::storage &theme, std::size_t pos) {
					active_stack.clear();
					iter = theme.find_first_range_ending_after(pos);
					while (iter.get_iterator() != theme.end()) {
						std::size_t range_start = iter.get_range_start();
						if (range_start > pos) {
							break;
						}
						active_stack.emplace_back(
							iter.get_iterator()->value.value, range_start + iter.get_iterator()->length
						);
						iter = theme.find_next_range_ending_after(pos, iter);
					}
				}
				/// Moves this layer forward.
				///
				/// \return Whether the top element of the active stack has changed.
				bool move_forward(const text_theme_data::storage &theme, std::size_t pos) {
					bool changed = false;
					for (auto it = active_stack.begin(); it != active_stack.end(); ) {
						if (it->end <= pos) {
							it = active_stack.erase(it);
							if (it == active_stack.end()) {
								changed = true;
								break;
							}
						} else {
							++it;
						}
					}
					if (iter.get_iterator() != theme.end()) {
						// the first iteration is special: this may end before the given position
						std::size_t range_beg = iter.get_range_start();
						if (range_beg <= pos) {
							if (std::size_t range_end = range_beg + iter.get_iterator()->length; range_end > pos) {
								active_stack.emplace_back(iter.get_iterator()->value.value, range_end);
								changed = true;
							}
							iter = theme.find_next_range_ending_after(pos, iter);

							// all other iterations
							while (iter.get_iterator() != theme.end()) {
								range_beg = iter.get_range_start();
								if (range_beg > pos) {
									break;
								}
								active_stack.emplace_back(
									iter.get_iterator()->value.value, range_beg + iter.get_iterator()->length
								);
								changed = true;
								iter = theme.find_next_range_ending_after(pos, iter);
							}
						}
					}
					return changed;
				}
			};

			/// Highlight layers corresponding to \ref _providers.
			std::vector<_highlight_layer> _layers;
			const text_theme_provider_registry *_providers = nullptr; ///< The list of providers.
		};


		/// Initializes this registry with the associated \ref interpretation.
		explicit text_theme_provider_registry(interpretation &interp) : _interpretation(interp) {
		}

		/// Adds a new provider with the given priority and returns a corresponding \ref token.
		[[nodiscard]] token add_provider(priority p) {
			auto insert_before = _providers.begin();
			for (; insert_before != _providers.end(); ++insert_before) {
				if (insert_before->provider_priority <= p) {
					break;
				}
			}
			auto iter = _providers.emplace(insert_before);
			iter->provider_priority = p;
			return token(_interpretation, iter);
		}
		/// Removes the given provider, invokes \ref interpretation::appearance_changed and resets the given token.
		void remove_provider(token&);

		/// Handles a single modification by updating all providers.
		void on_modification(std::size_t beg, std::size_t erased_len, std::size_t inserted_len) {
			for (auto &entry : _providers) {
				entry.theme.on_modification(beg, erased_len, inserted_len);
			}
		}
	protected:
		std::list<_entry> _providers; ///< The list of providers, sorted in descending order by their priorities.
		interpretation &_interpretation; ///< The associated \ref _interpretation.
	};
}
