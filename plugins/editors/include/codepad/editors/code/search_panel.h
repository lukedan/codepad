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
			_decoration_token = _contents->get_decoration_providers().add_provider(
				std::make_unique<decoration_provider>()
			);
			_on_input_changed();
		}


		/// Retrieves the setting entry that determines the decorations for search highlight decoration.
		static settings::retriever_parser<
			std::shared_ptr<decoration_renderer>
		> &get_decoration_renderer_setting(settings&);

		/// Returns the name for \ref _result_list.
		[[nodiscard]] inline static std::u8string_view get_result_list_name() {
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
		// TODO unregister decorations when this is closed
		/// Token for the decoration provider.
		contents_region::view_decoration_provider_list::token _decoration_token;

		ui::virtual_list_viewport *_result_list = nullptr; ///< The list of results.
		contents_region *_contents = nullptr; ///< The \ref contents_region associated with this panel.

		/// Updates the search.
		void _on_input_changed() override;

		/// Handles \p result_list.
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
