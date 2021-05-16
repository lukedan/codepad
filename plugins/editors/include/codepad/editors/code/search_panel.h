// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// A panel for displaying search results.

#include <codepad/core/text.h>
#include <codepad/ui/elements/input_prompt.h>
#include <codepad/ui/elements/list_viewport.h>

#include "contents_region.h"

namespace codepad::editors::code {
	/// A search panel.
	class search_panel : public ui::input_prompt {
	public:
		/// Jumps to the selected match.
		void on_confirm() {
			// TODO
		}

		/// Sets \ref _contents_region. Also starts the first search.
		void set_contents_region(contents_region &rgn) {
			_contents = &rgn;
			_on_input_changed();
		}

		/// Returns the name for \ref _result_list.
		inline static std::u8string_view get_result_list_name() {
			return u8"result_list";
		}
		/// Returns the default class of elements of this type.
		[[nodiscard]] inline static std::u8string_view get_default_class() {
			return u8"search_panel";
		}
	protected:
		/// An item source containing all match results.
		class _match_result_source : public ui::virtual_list_viewport::item_source {
		public:
			/// Initializes \ref _parent.
			explicit _match_result_source(search_panel &parent) : _parent(&parent) {
			}

			/// Returns the number of elements in \ref _results.
			std::size_t get_item_count() const override {
				return _parent->_results.size();
			}
			/// Sets the \p position reference to the corresponding position.
			void set_item(std::size_t i, ui::reference_container &container) const override {
				if (auto ref = container.get_reference<ui::label>(u8"position")) {
					auto str = std::to_string(_parent->_results[i]);
					std::u8string_view sv(reinterpret_cast<const char8_t*>(str.c_str()), str.size());
					ref->set_text(std::u8string(sv));
				}
			}

			/// Called by the search panel when the list of results have changed.
			void on_items_changed() {
				_on_items_changed();
			}
		protected:
			search_panel *_parent = nullptr; ///< The associated \ref search_panel.
		};

		std::vector<std::size_t> _results; ///< Current results.
		ui::virtual_list_viewport *_result_list = nullptr; ///< The list of results.
		contents_region *_contents = nullptr; ///< The \ref contents_region associated with this panel.

		/// Updates the search.
		void _on_input_changed() override {
			using _codepoint_str = std::basic_string<codepoint>;

			_results.clear();

			if (!_input->get_text().empty()) {
				// decode search string
				_codepoint_str pattern;
				for (auto it = _input->get_text().begin(); it != _input->get_text().end(); ) {
					codepoint cp;
					if (!encodings::utf8::next_codepoint(it, _input->get_text().end(), cp)) {
						logger::get().log_error(CP_HERE) << "invalid codepoint in search string: " << cp;
						cp = encodings::replacement_character;
					}
					pattern.push_back(cp);
				}

				// match
				auto &doc = _contents->get_document();
				kmp_matcher<_codepoint_str> matcher(std::move(pattern));
				kmp_matcher<_codepoint_str>::state st;
				std::size_t position = 0;
				auto it = doc.character_at(position);
				while (!it.codepoint().ended()) {
					codepoint cp;
					if (it.is_linebreak()) {
						cp = U'\n';
					} else {
						cp =
							it.codepoint().is_codepoint_valid() ?
							it.codepoint().get_codepoint() :
							encodings::replacement_character;
					}
					auto [new_st, match] = matcher.put(cp, st);

					st = new_st;
					++position;
					it.next();
					if (match) {
						_results.emplace_back(position - pattern.size());
					}
				}
			}

			if (_result_list) {
				static_cast<_match_result_source*>(_result_list->get_source())->on_items_changed();
			}
		}

		/// Initializes 
		bool _handle_reference(std::u8string_view name, element *e) override {
			if (name == get_result_list_name()) {
				if (_reference_cast_to(_result_list, e)) {
					auto src = std::make_unique<_match_result_source>(*this);
					_result_list->replace_source(std::move(src));
				}
				return true;
			}
			return input_prompt::_handle_reference(name, e);
		}
	};
}
