// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// The code editing component of a \ref codepad::editors::editor.

#include <memory>

#include <codepad/core/red_black_tree.h>
#include <codepad/core/settings.h>
#include <codepad/ui/elements/popup.h>
#include <codepad/ui/elements/stack_panel.h>

#include "../decoration.h"
#include "../editor.h"
#include "../interaction_modes.h"
#include "caret_set.h"
#include "view.h"
#include "fragment_generation.h"

namespace codepad::editors {
	class buffer_manager;
}
namespace codepad::editors::code {
	/// A tooltip with a \ref ui::stack_panel that contains tooltip contents from all providers.
	class contents_region_tooltip : public ui::popup {
	public:
		/// Returns \ref _contents.
		[[nodiscard]] ui::stack_panel &get_contents_panel() const {
			return *_contents;
		}

		/// Returns the role used to identify \ref _contents.
		[[nodiscard]] inline static std::u8string_view get_contents_panel_role() {
			return u8"contents_panel";
		}
		/// Returns the default class of elements of this type.
		[[nodiscard]] inline static std::u8string_view get_default_class() {
			return u8"contents_region_tooltip";
		}
	protected:
		ui::stack_panel *_contents = nullptr; ///< The panel for contents from all providers.

		/// Handles \ref _contents.
		bool _handle_reference(std::u8string_view role, element *e) override {
			if (role == get_contents_panel_role()) {
				_reference_cast_to(_contents, e);
				return true;
			}
			return ui::popup::_handle_reference(role, e);
		}
	};

	/// The main component of a \ref editor that's responsible of text editing. This element should only be used as a
	/// child of a \ref editor.
	///
	/// \todo Proper drag & drop, code folding, etc.
	/// \todo Extract caret movement code to somewhere else.
	class contents_region : public interactive_contents_region_base<caret_set> {
		friend buffer_manager;
	public:
		/// Returns the \ref buffer associated with \ref _doc.
		buffer &get_buffer() const override {
			return _doc->get_buffer();
		}
		/// Returns the \ref interpretation currently bound to this contents_region.
		interpretation &get_document() const {
			return *_doc;
		}

		/// Returns the total number of visual lines.
		std::size_t get_num_visual_lines() const {
			return _fmt.get_linebreaks().num_visual_lines() - _fmt.get_folding().folded_linebreaks();
		}

		/// Sets the font family given its name.
		void set_font_families_by_name(const std::vector<std::u8string> &names) {
			std::vector<std::shared_ptr<ui::font_family>> fonts;
			for (const std::u8string &name : names) {
				fonts.emplace_back(get_manager().get_renderer().find_font_family(name));
			}
			set_font_families(std::move(fonts));
		}
		/// Sets the set of font families.
		void set_font_families(std::vector<std::shared_ptr<ui::font_family>> fs) {
			_font_families = std::move(fs);
			_on_editing_visual_changed();
		}
		/// Returns the set of font families.
		const std::vector<std::shared_ptr<ui::font_family>> &get_font_families() const {
			return _font_families;
		}

		/// Sets the font size.
		void set_font_size(double size) {
			_font_size = size;
			_on_editing_visual_changed();
		}
		/// Returns the font size.
		double get_font_size() const {
			return _font_size;
		}

		/// Sets the height of a line.
		void set_line_height(double val) {
			_line_height = val;
			_on_editing_visual_changed();
		}
		/// Returns the height of a line.
		double get_line_height() const {
			return _line_height;
		}

		/// Returns the vertical position of the baseline.
		double get_baseline() const {
			return _line_height * 0.8; // TODO customizable baseline?
		}

		/// Sets the maximum width of a tab character relative to the width of spaces.
		void set_tab_space_width(double w) {
			_tab_space_width = w;
			_on_editing_visual_changed();
		}
		/// Returns the maximum width of a tab character relative to the width of spaces.
		[[nodiscard]] double get_tab_space_width() const {
			return _tab_space_width;
		}
		/// Returns the maximum absolute width of a tab character.
		[[nodiscard]] double get_tab_width() const {
			auto font = _font_families[0]->get_matching_font(
				ui::font_style::normal, ui::font_weight::normal, ui::font_stretch::normal
			);
			return font->get_character_width_em(U' ') * _tab_space_width * _font_size;
		}

		// TODO set folded fragment gizmo
		/// Returns \ref _fold_fragment_func.
		const folded_region_skipper::fragment_func &get_folded_fragment_function() const {
			return _fold_fragment_func;
		}

		/// Sets \ref _invalid_cp_func.
		void set_invalid_codepoint_fragment_func(invalid_codepoint_fragment_func fmt) {
			_invalid_cp_func = std::move(fmt);
			_on_editing_visual_changed();
		}
		/// Returns \ref _invalid_cp_func.
		const invalid_codepoint_fragment_func &get_invalid_codepoint_fragment_func() const {
			return _invalid_cp_func;
		}

		/// Sets the font size and automatically adjusts the line height.
		void set_font_size_and_line_height(double fontsize) {
			_font_size = fontsize;
			_line_height = _font_size * 1.5; // TODO magic number
			_on_editing_visual_changed();
		}

		/// Returns the length of scrolling by one tick.
		double get_vertical_scroll_delta() const override {
			return get_line_height() * _lines_per_scroll;
		}
		/// Returns the vertical viewport range.
		double get_vertical_scroll_range() const override {
			return
				get_line_height() * static_cast<double>(get_num_visual_lines() - 1) +
				get_layout().height() + get_padding().top;
		}
		/// Returns the horizontal viewport range.
		double get_horizontal_scroll_range() const override {
			// TODO
			return 1.0;
		}

		/// Returns the overriden cursor if there is one, otherwise returns the `I'-beam cursor.
		ui::cursor get_current_display_cursor() const override {
			if (ui::cursor c = _interaction_manager.get_override_cursor(); c != ui::cursor::not_specified) {
				return c;
			}
			return ui::cursor::text_beam;
		}

		/// Sets the number of lines to scroll per `tick'.
		void set_num_lines_per_scroll(double v) {
			_lines_per_scroll = v;
		}
		/// Sets the number of lines to scroll per `tick'.
		double get_num_lines_per_scroll() const {
			return _lines_per_scroll;
		}

		/// Returns the current set of carets.
		const caret_set &get_carets() const override {
			return _carets;
		}
		/// Sets the current carets. The provided set of carets must not be empty. This function calculates the
		/// horizontal positions of all carets, and scrolls the viewport when necessary.
		void set_carets(const std::vector<ui::caret_selection> &cs) {
			assert_true_usage(!cs.empty(), "must have at least one caret");
			caret_set set;
			for (const ui::caret_selection &sel : cs) {
				caret_data data;
				data.alignment = get_horizontal_caret_position(_extract_position(sel, data));
				set.add(sel, data);
			}
			set_carets_keepdata(std::move(set));
		}
		/// \overload
		///
		/// \note The alignment specified in \p cs is ignored.
		void set_carets(caret_set cs) {
			assert_true_usage(!cs.carets.empty(), "must have at least one caret");
			for (auto it = cs.begin(); it.get_iterator() != cs.carets.end(); it.move_next()) {
				ui::caret_selection sel = it.get_caret_selection();
				it.get_iterator().get_value_rawmod().data.alignment =
					get_horizontal_caret_position(_extract_position(sel, it.get_iterator()->data));
			}
			set_carets_keepdata(std::move(cs));
		}
		/// Sets tue current carets, keeping any addtional data specified in \p cs. This function scrolls the
		/// viewport when necessary.
		///
		/// \todo Find a better position to scroll to.
		void set_carets_keepdata(caret_set cs) {
			assert_true_logical(!cs.carets.empty(), "must have at least one caret");
			_carets = std::move(cs);
			auto caret_it = _carets.begin();
			_make_caret_visible(_extract_position(caret_it.get_caret_selection(), caret_it.get_iterator()->data));
			_on_carets_changed();
		}

		/// Adds the given caret to this \ref contents_region.
		void add_caret(caret_selection_position c) override {
			auto [it, merged] = _carets.add(c.caret, caret_data(0.0, c.caret_after_stall));
			// the added caret may be merged, so alignment is calculated again here
			it.get_iterator().get_value_rawmod().data.alignment = get_horizontal_caret_position(
				_extract_position(it.get_caret_selection(), it.get_iterator()->data)
			);
			_on_carets_changed();
		}
		/// Removes the given caret.
		void remove_caret(caret_set::iterator it) override {
			_carets.remove(it);
			_on_carets_changed();
		}
		/// Clears all carets from this \ref contents_region.
		void clear_carets() override {
			_carets.carets.clear();
			_on_carets_changed();
		}

		/// Moves all carets. The caller needs to give very precise instructions on how to move them.
		///
		/// \param transform A function-like object that takes a \ref ui::caret_selection and a \ref caret_data
		///                  associated with a caret in the old set of carets as input, and returns the transformed
		///                  caret information. The transformed information is kept as-is (e.g., alignment is not
		///                  re-calculated).
		template <typename Transform> void move_carets_raw(const Transform &transform) {
			caret_set new_carets;
			for (auto iter = _carets.begin(); iter.get_iterator() != _carets.carets.end(); iter.move_next()) {
				auto [caret_sel, data] = transform(iter.get_caret_selection(), iter.get_iterator()->data);
				new_carets.add(caret_sel, data);
			}
			set_carets_keepdata(std::move(new_carets));
		}
	protected:
		/// Updates and returns the given caret information using the specified \ref caret_position and alignment. If
		/// \p continue_selection is \p true, \ref ui::caret_selection::move_caret() is used; otherwise the
		/// \ref ui::caret_selection is reset.
		[[nodiscard]] std::pair<ui::caret_selection, caret_data> _update_caret_selection(
			ui::caret_selection caret_sel, caret_data data,
			bool continue_selection, std::pair<caret_position, double> new_caret_and_alignment
		) {
			data.alignment = new_caret_and_alignment.second;
			data.after_stall = new_caret_and_alignment.first.after_stall;
			if (continue_selection) {
				caret_sel.move_caret(new_caret_and_alignment.first.position);
			} else {
				caret_sel = ui::caret_selection(new_caret_and_alignment.first.position);
			}
			return { caret_sel, data };
		}
		/// Computes the horizontal position of the \ref caret_position and uses it as alignment, and returns the
		/// result of the other overload.
		[[nodiscard]] std::pair<ui::caret_selection, caret_data> _update_caret_selection(
			ui::caret_selection &caret_sel, caret_data &data, bool continue_selection, caret_position new_caret
		) {
			return _update_caret_selection(
				caret_sel, data, continue_selection, { new_caret, get_horizontal_caret_position(new_caret) }
			);
		}
	public:
		/// Transforms all caret positions based on the return value of the supplied callback functions.
		template <typename EditSelection, typename CancelSelection> void move_carets(
			const EditSelection &edit_sel, const CancelSelection &cancel_sel, bool continue_selection
		) {
			if (continue_selection) {
				move_carets_raw([this, &edit_sel](ui::caret_selection caret_sel, caret_data data) {
					return _update_caret_selection(caret_sel, data, true, edit_sel(caret_sel, data));
				});
			} else {
				move_carets_raw([this, &edit_sel, &cancel_sel](ui::caret_selection caret_sel, caret_data data) {
					if (caret_sel.has_selection()) {
						return _update_caret_selection(caret_sel, data, false, cancel_sel(caret_sel, data));
					}
					return _update_caret_selection(caret_sel, data, false, edit_sel(caret_sel, data));
				});
			}
		}
		/// Moves all carets to specified positions, treating selections and non-selections the same way.
		template <typename EditSelection> void move_carets(const EditSelection &edit_sel, bool continue_selection) {
			return move_carets(edit_sel, edit_sel, continue_selection);
		}


		/// Moves all carets one character to the left. If \p continueselection is \p false, then all carets that have
		/// selected regions will be placed at the front of the selection.
		void move_all_carets_left(bool continue_selection) {
			move_carets(
				[this](ui::caret_selection caret_sel, caret_data) {
					return caret_position(_get_previous_caret_position(caret_sel.get_caret_position()), true);
				},
				[](ui::caret_selection caret_sel, caret_data data) {
					if (caret_sel.caret_offset == 0) {
						return _extract_position(caret_sel, data);
					}
					return caret_position(caret_sel.selection_begin, true);
				},
				continue_selection
			);
		}
		/// Moves all carets one character to the right. If \p continueselection is \p false, then all carets that have
		/// selected regions will be placed at the back of the selection.
		void move_all_carets_right(bool continue_selection) {
			move_carets(
				[this](ui::caret_selection caret_sel, caret_data) {
					return caret_position(_get_next_caret_position(caret_sel.get_caret_position()), false);
				},
				[](ui::caret_selection caret_sel, caret_data data) {
					if (caret_sel.caret_offset == caret_sel.selection_length) {
						return _extract_position(caret_sel, data);
					}
					return caret_position(caret_sel.get_selection_end(), false);
				},
				continue_selection
			);
		}
		/// Moves all carets vertically by a given number of lines.
		void move_all_carets_vertically(int offset, bool continue_selection) {
			move_carets(
				[this, offset](ui::caret_selection caret_sel, caret_data data) {
					auto res = _move_caret_vertically(
						_get_visual_line_of_caret(_extract_position(caret_sel, data)), offset, data.alignment
					);
					return std::make_pair(res, data.alignment);
				},
				continue_selection
			);
		}
		/// Moves all carets one line up.
		void move_all_carets_up(bool continue_selection) {
			move_all_carets_vertically(-1, continue_selection);
		}
		/// Moves all carets one line down.
		void move_all_carets_down(bool continue_selection) {
			move_all_carets_vertically(1, continue_selection);
		}
		/// Moves all carets to the beginning of the lines they're at, with folding and word wrapping enabled.
		void move_all_carets_to_line_beginning(bool continue_selection) {
			move_carets(
				[this](ui::caret_selection caret_sel, caret_data data) {
					std::size_t begin_pos =
						_fmt.get_linebreaks().get_beginning_char_of_visual_line(
							_fmt.get_folding().folded_to_unfolded_line_number(
								_get_visual_line_of_caret(_extract_position(caret_sel, data))
							)
						).first;
					return caret_position(begin_pos, true);
				},
				continue_selection
			);
		}
		/// Moves all carets to the beginning of the lines they're at, with folding and word wrapping enabled,
		/// skipping all spaces in the front of the line.
		void move_all_carets_to_line_beginning_advanced(bool continue_selection) {
			move_carets(
				[this](ui::caret_selection caret_sel, caret_data data) {
					std::size_t
						visline = _get_visual_line_of_caret(_extract_position(caret_sel, data)),
						unfolded = _fmt.get_folding().folded_to_unfolded_line_number(visline);
					auto linfo = _fmt.get_linebreaks().get_line_info(unfolded);
					std::size_t begp = std::max(linfo.first.first_char, linfo.second.prev_chars), exbegp = begp;
					if (linfo.first.first_char >= linfo.second.prev_chars) {
						std::size_t nextsb =
							linfo.second.entry == _fmt.get_linebreaks().end() ?
							std::numeric_limits<std::size_t>::max() :
							linfo.second.prev_chars + linfo.second.entry->length;
						for (
							auto cit = _doc->character_at(begp);
							!cit.is_linebreak() && (
								cit.codepoint().get_codepoint() == ' ' || cit.codepoint().get_codepoint() == '\t'
								) && exbegp < nextsb;
							cit.next(), ++exbegp
							) {
						}
						auto finfo = _fmt.get_folding().find_region_containing_open(exbegp);
						if (finfo.entry != _fmt.get_folding().end()) {
							exbegp = finfo.prev_chars + finfo.entry->gap;
						}
						if (nextsb <= exbegp) {
							exbegp = begp;
						}
					}
					return caret_position(caret_sel.get_caret_position() == exbegp ? begp : exbegp, true);
				},
				continue_selection
			);
		}
		/// Moves all carets to the end of the lines they're at, with folding and word wrapping enabled.
		void move_all_carets_to_line_ending(bool continue_selection) {
			move_carets(
				[this](ui::caret_selection caret_sel, caret_data data) {
					return std::make_pair(
						caret_position(_fmt.get_linebreaks().get_past_ending_char_of_visual_line(
							_fmt.get_folding().folded_to_unfolded_line_number(
								_get_visual_line_of_caret(_extract_position(caret_sel, data)) + 1
							) - 1
						).first, false),
						std::numeric_limits<double>::max()
					);
				},
				continue_selection
			);
		}
		/// Cancels all selected regions.
		void cancel_all_selections() {
			caret_set new_carets;
			for (auto it = _carets.begin(); it.get_iterator() != _carets.carets.end(); it.move_next()) {
				new_carets.add(
					ui::caret_selection(it.get_caret_selection().get_caret_position()),
					it.get_iterator()->data
				);
			}
			set_carets_keepdata(std::move(new_carets));
		}


		// edit operations
		/// Inserts the input text at each caret.
		void on_text_input(std::u8string_view) override;
		/// Calls \ref interpretation::on_backspace() with the current set of carets.
		void on_backspace() {
			_interaction_manager.on_edit_operation();
			_doc->on_backspace(_carets, this);
		}
		/// Calls \ref interpretation::on_delete() with the current set of carets.
		void on_delete() {
			_interaction_manager.on_edit_operation();
			_doc->on_delete(_carets, this);
		}
		/// Called when the user presses the `enter' key to insert a line break at all carets.
		void on_return() {
			_interaction_manager.on_edit_operation();
			std::u32string_view le = line_ending_to_string(_doc->get_default_line_ending());
			byte_string encoded;
			for (char32_t c : le) {
				encoded.append(_doc->get_encoding()->encode_codepoint(c));
			}
			_doc->on_insert(_carets, encoded, this);
		}
		/// Checks if there are editing actions available for undo-ing, and calls \ref buffer::undo() if there is.
		///
		/// \return \p true if an action has been reverted.
		bool try_undo() {
			if (_doc->get_buffer().can_undo()) {
				buffer::modifier modifier(_doc->get_buffer(), this);
				_interaction_manager.on_edit_operation();
				modifier.undo();
				return true;
			}
			return false;
		}
		/// Checks if there are editing actions available for redo-ing, and calls \ref buffer::redo() if there is.
		///
		/// \return \p true if an action has been restored.
		bool try_redo() {
			if (_doc->get_buffer().can_redo()) {
				buffer::modifier modifier(_doc->get_buffer(), this);
				_interaction_manager.on_edit_operation();
				modifier.redo();
				return true;
			}
			return false;
		}

		/// Returns the \ref view_formatting associated with this contents_region.
		const view_formatting &get_formatting() const {
			return _fmt;
		}
		/// Folds the given region.
		///
		/// \todo Render carets even if they're in a folded region.
		void add_folded_region(const view_formatting::fold_region &fr) {
			_fmt.add_folded_region(fr);
			_on_folding_changed();
		}
		/// Unfolds the given region.
		void remove_folded_region(folding_registry::iterator f) {
			_fmt.remove_folded_region(f);
			_on_folding_changed();
		}
		/// Unfolds all currently folded regions.
		void clear_folded_regions() {
			_fmt.clear_folded_regions();
			_on_folding_changed();
		}

		/// Returns the range of visual line indices that are visible for the given viewport. Note that the second
		/// line is the one past the last visible line.
		std::pair<std::size_t, std::size_t> get_visible_visual_lines(double top, double bottom) const {
			double lh = get_line_height();
			return std::make_pair(
				static_cast<std::size_t>(std::max(0.0, top / lh)),
				std::min(static_cast<std::size_t>(std::max(0.0, bottom / lh)) + 1, get_num_visual_lines())
			);
		}
		/// Similar to the other \ref get_visible_visual_lines(), except that this function uses the current viewport
		/// of the \ref editor as the parameters.
		std::pair<std::size_t, std::size_t> get_visible_visual_lines() const {
			double top = get_editor().get_vertical_position() - get_padding().top;
			return get_visible_visual_lines(top, top + get_layout().height());
		}
		/// Returns the caret position corresponding to a given position. Note that the offset is relative to the
		/// top-left corner of the document rather than that of this element.
		[[nodiscard]] caret_position hit_test_for_caret_document(vec2d offset) const {
			std::size_t line = static_cast<std::size_t>(std::max(offset.y / get_line_height(), 0.0));
			return _hit_test_at_visual_line(std::min(line, get_num_visual_lines() - 1), offset.x);
		}
		/// Shorthand for \ref hit_test_for_caret_document when the coordinates are relative to this \ref ui::element.
		///
		/// \todo Need changes after adding horizontal scrolling.
		caret_position hit_test_for_caret(vec2d pos) const override {
			return hit_test_for_caret_document(vec2d(
				pos.x + get_editor().get_horizontal_position() - get_padding().left,
				pos.y + get_editor().get_vertical_position() - get_padding().top
			));
		}
		/// Returns the horizontal visual position of a caret.
		///
		/// \return The horizontal position of the caret, relative to the leftmost of the document.
		[[nodiscard]] double get_horizontal_caret_position(caret_position pos) const {
			return _get_caret_pos_x_at_visual_line(_get_visual_line_of_caret(pos), pos.position);
		}


		info_event<>
			/// Invoked when the visual of \ref _doc has changed, e.g., when it is modified, when the
			/// \ref interpretation to which \ref _doc points is changed, when the font has been changed, or when the
			/// theme of \ref _doc is changed.
			content_visual_changed,
			/// Invoked when the \ref interpretation is modified or changed by \ref _set_document().
			content_modified,
			/// Invoked when the set of carets is changed. Note that this does not necessarily mean that the result
			/// of \ref get_carets will change due to temporary carets.
			carets_changed,
			/// Invoked when visuals particular to this view is changed, like folding or word wrapping.
			editing_visual_changed,
			/// Invoked when folded regions are added or removed. Note that this is not called when folded
			/// regions are changed due to edits made from other views.
			///
			/// \todo Invoke this when folded regions are changed due to edits made from other views.
			folding_changed;

		/// Retrieves the setting entry that determines the font families.
		static settings::retriever_parser<std::vector<std::u8string>> &get_backup_fonts_setting(settings&);

		/// The default formatter for invalid codepoints.
		inline static std::u8string format_invalid_codepoint(codepoint value) {
			constexpr static std::size_t _buffer_size = 20;
			static char _buf[_buffer_size];

			// TODO C++20 use std::format
			std::snprintf(_buf, _buffer_size, "[0x%lX]", static_cast<unsigned long>(value));
			return reinterpret_cast<char8_t*>(_buf);
		}

		/// Retrieves the \ref contents_region attached to the given \ref editor.
		inline static contents_region *get_from_editor(editor &edt) {
			return dynamic_cast<contents_region*>(edt.get_contents_region());
		}

		/// Returns the default class used by elements of type \ref contents_region.
		inline static std::u8string_view get_default_class() {
			return u8"code_contents_region";
		}
	protected:
		/// Extracts a \ref caret_position from a \ref caret_set::entry.
		inline static caret_position _extract_position(ui::caret_selection caret_sel, caret_data data) {
			return caret_position(caret_sel.get_caret_position(), data.after_stall);
		}

		using _base = interactive_contents_region_base<caret_set>; ///< The base type.

		std::shared_ptr<interpretation> _doc; ///< The \ref interpretation bound to this contents_region.
		info_event<buffer::begin_edit_info>::token _begin_edit_tok; ///< Used to listen to \ref buffer::begin_edit.
		/// Used to listen to \ref interpretation::end_modification.
		info_event<interpretation::end_modification_info>::token _end_modification_tok;
		/// Used to listen to \ref interpretation::end_edit.
		info_event<interpretation::end_edit_info>::token _end_edit_tok;
		/// Used to listen to \ref interpretation::appearance_changed.
		info_event<interpretation::appearance_changed_info>::token _appearance_changed_tok;

		interaction_manager<caret_set> _interaction_manager; ///< The \ref interaction_manager.
		caret_set _carets; ///< The set of carets.
		double _lines_per_scroll = 3.0; ///< The number of lines to scroll per `tick'.

		folded_region_skipper::fragment_func _fold_fragment_func; ///< Dictates the format of folded regions.
		invalid_codepoint_fragment_func _invalid_cp_func; ///< Used to format invalid codepoints.
		/// The set of font family used to display code. Font families in the back are backups for font families in
		/// front.
		std::vector<std::shared_ptr<ui::font_family>> _font_families;
		double
			_font_size = 12.0, ///< The font size.
			_tab_space_width = 4.0, ///< The maximum width of a tab character as a number of spaces.
			_line_height = 18.0; ///< The height of a line.
		// TODO more entries from https://jkorpela.fi/chars/spaces.html ?
		ui::generic_visual_geometry
			_whitespace_geometry, ///< Geometry rendered for a whitespace.
			_tab_geometry, ///< Geometry rendered for a tab.
			_crlf_geometry, ///< Geometry rendered for a CRLF line break.
			_cr_geometry, ///< Geometry rendered for a CR line break.
			_lf_geometry; ///< Geometry rendered for a LF line break.
		view_formatting _fmt; ///< The \ref view_formatting associated with this contents_region.
		double _view_width = 0.0; ///< The width that word wrap is calculated according to.

		/// Stores data from all tooltip providers used for \ref _tooltip.
		std::vector<std::unique_ptr<tooltip>> _tooltip_data;
		/// Used to listen to \ref element::destroying from \ref _tooltip.
		info_event<>::token _tooltip_destroying_token;
		contents_region_tooltip *_tooltip = nullptr; ///< The tooltip that's currently active.
		caret_position _tooltip_position; ///< The character position of \ref _tooltip.


		/// Sets the \ref interpretation displayed by the contents_region. This should only be called once after this
		/// \ref contents_region is created by the \ref buffer_manager.
		void _set_document(std::shared_ptr<interpretation> newdoc) {
			assert_true_usage(_doc == nullptr, "setting document twice");
			assert_true_usage(newdoc != nullptr, "cannot set empty document");
			_doc = std::move(newdoc);
			_carets.reset();
			_begin_edit_tok = _doc->get_buffer().begin_edit += [this](buffer::begin_edit_info &info) {
				_on_begin_edit(info);
			};
			_end_modification_tok = _doc->end_modification += [this](interpretation::end_modification_info &info) {
				_on_end_modification(info);
			};
			_end_edit_tok = _doc->end_edit += [this](interpretation::end_edit_info &info) {
				_on_end_edit(info);
			};
			_appearance_changed_tok = (
				_doc->appearance_changed += [this](interpretation::appearance_changed_info &info) {
					if (info.type == interpretation::appearance_change_type::layout_and_visual) {
						// TODO handle layout change
					}
					_on_content_visual_changed();
				}
			);
			_fmt = view_formatting(*_doc);
			_on_content_modified();
		}


		/// Returns the visual line that the given caret is on.
		std::size_t _get_visual_line_of_caret(caret_position pos) const {
			auto
				res = _fmt.get_linebreaks().get_visual_line_and_column_and_softbreak_before_or_at_char(pos.position);
			std::size_t unfolded = std::get<0>(res);
			if (
				!pos.after_stall &&
				std::get<2>(res).entry != _fmt.get_linebreaks().begin() &&
				std::get<2>(res).prev_chars == pos.position
			) {
				--unfolded;
			}
			return _fmt.get_folding().unfolded_to_folded_line_number(unfolded);
		}
		/// Given a line index and a horizontal position, returns the closest caret position. Note that the
		/// horizontal position should not include the left padding.
		///
		/// \param line The visual line index.
		/// \param x The horizontal position.
		caret_position _hit_test_at_visual_line(std::size_t line, double x) const;
		/// Returns the horizontal position of a caret. Note that the returned position does not include the left
		/// padding. This function is used when the line of the caret has been previously obtained to avoid repeated
		/// calls to \ref _get_visual_line_of_caret().
		///
		/// \param line The visual line that the caret is on.
		/// \param position The position of the caret in the whole document.
		double _get_caret_pos_x_at_visual_line(std::size_t line, std::size_t position) const;
		/// Returns the position of the caret correponding to the given character position. The returned region has
		/// zero width.
		[[nodiscard]] rectd _get_caret_placement(caret_position pos) {
			std::size_t visline = _get_visual_line_of_caret(pos);
			double lh = get_line_height();
			vec2d topleft = get_client_region().xmin_ymin();
			return rectd::from_xywh(
				topleft.x + _get_caret_pos_x_at_visual_line(visline, pos.position) -
				get_editor().get_horizontal_position(),
				topleft.y + lh * static_cast<double>(visline) - get_editor().get_vertical_position(),
				0.0, lh
			);
		}
		// TODO this function should also take into account the inter-character position of the caret

		/// Called when the vertical position of the document is changed or when the carets have been moved,
		/// to update the caret position used by IMs.
		///
		/// \todo Maybe also take into account the width of the character.
		void _update_window_caret_position() {
			ui::window *wnd = get_window();
			// when selecting with a mouse, it's possible that there are no carets in _carets at all
			if (!_carets.carets.empty() && wnd != nullptr) {
				auto entry = _carets.carets.begin();
				wnd->set_active_caret_position(_get_caret_placement(_extract_position(entry->caret, entry->data)));
			}
		}
		/// Moves the viewport so that the given caret is visible.
		void _make_caret_visible(caret_position caret) {
			std::size_t line = _get_visual_line_of_caret(caret);
			double
				line_height = get_line_height(),
				xpos,
				ypos = static_cast<double>(line) * line_height + get_padding().top;
			xpos = _get_caret_pos_x_at_visual_line(line, caret.position);
			rectd rgn = rectd::from_xywh(xpos, ypos, 0.0, line_height); // TODO maybe include the whole selection
			get_editor().make_region_visible(rgn);
		}

		/// Simply calls \ref _on_carets_changed().
		void _on_temporary_carets_changed() override {
			_on_carets_changed();
		}

		/// Called when \ref buffer::begin_edit is triggered. Prepares \ref _fmt for adjustments by calculating
		/// byte positions of each caret. If the source element is not this element, also calls
		/// \ref interaction_manager::on_edit_operation() since it must not have been called previously.
		void _on_begin_edit(buffer::begin_edit_info &info) {
			if (info.source_element != this) {
				_interaction_manager.on_edit_operation();
			}
			_fmt.prepare_for_edit(*_doc);
		}
		/// Called when \ref interpretation::end_modification is triggered. This function performs fixup on carets,
		/// folded regions, and other positions that may be affected by the modification.
		void _on_end_modification(interpretation::end_modification_info&);
		/// Called when \ref interpretation::end_edit is triggered. Performs necessary adjustments to the view, invokes
		/// \ref content_modified, then calls \ref _on_content_visual_changed.
		void _on_end_edit(interpretation::end_edit_info&);
		/// Called when the associated \ref interpretation has been changed to another or when the contents have been
		/// modified. Invokes \ref content_modified and calls \ref _on_content_visual_changed().
		void _on_content_modified() {
			content_modified.invoke();
			_on_content_visual_changed();
		}
		/// Called when the visual of the \ref interpretation has changed, e.g., when it has been modified, or when
		/// its theme has changed. Invokes \ref content_visual_changed and calls \ref _on_editing_visual_changed().
		void _on_content_visual_changed() {
			content_visual_changed.invoke();
			_on_editing_visual_changed();
		}

		/// Called when the set of carets has changed. This can occur when the user calls \p set_caret, or when
		/// the current selection is being edited by the user. This function updates input method related
		/// information, invokes \ref carets_changed, and calls \ref invalidate_visual.
		void _on_carets_changed() {
			get_window()->interrupt_input_method();
			_update_window_caret_position();
			carets_changed.invoke();
			invalidate_visual();
		}

		/// Called when visuals particular to this single view, such as word wrapping or folding, has changed.
		/// Note that this does not include the changing of carets. Invokes \ref editing_visual_changed, and calls
		/// \ref invalidate_visual.
		void _on_editing_visual_changed() {
			editing_visual_changed.invoke();
			invalidate_visual();
		}
		/// Called when folded regions are added or removed. Invokes \ref folding_changed and calls
		/// \ref _on_editing_visual_changed.
		void _on_folding_changed() {
			folding_changed.invoke();
			_on_editing_visual_changed();
		}

		/// Recalculates word wrapping for the given lines.
		///
		/// \todo Currently not of too much use except for calculating word wrapping for the whole document.
		std::vector<std::size_t> _recalculate_wrapping_region(std::size_t, std::size_t) const;
		/// Adjusts and recalculates caret positions from \ref caret_data::bytepos_first and
		/// \ref caret_data::bytepos_second, after an edit has been made.
		///
		/// \todo Carets after undo-ing and redo-ing.
		void _adjust_recalculate_caret_char_positions(interpretation::end_edit_info &info) {
			interpretation::character_position_converter cvt(*_doc);
			// all byte positions calculated below may not be the exact first byte of the corresponding character,
			// and thus cannot be used as bytepos_first and bytepos_second
			// also, carets may merge into one another
			if (info.buffer_info.source_element == this) {
				if (info.buffer_info.type == edit_type::normal) {
					caret_set newcarets;
					for (const buffer::modification_position &pos : info.buffer_info.positions) {
						std::size_t bp = pos.position + pos.added_range, cp = cvt.byte_to_character(bp);
						newcarets.add(ui::caret_selection(cp), _get_caret_data(caret_position(cp, false)));
					}
					set_carets_keepdata(std::move(newcarets));
				} else {
					// TODO properly fixup edit positions for undo/redo
					caret_set newcarets;
					std::swap(newcarets, _carets);
					for (const auto &mod : info.character_edit_positions) {
						newcarets.on_modify(mod.position, mod.removed_range, mod.added_range);
					}
					set_carets(std::move(newcarets));
				}
			} else {
				caret_set newcarets;
				std::swap(newcarets, _carets);
				for (const auto &mod : info.character_edit_positions) {
					newcarets.on_modify(mod.position, mod.removed_range, mod.added_range);
				}
				set_carets(std::move(newcarets));
			}
		}

		/// Checks if line wrapping needs to be calculated.
		///
		/// \todo Recalculate the alignment of cursors.
		void _check_wrapping_width() { // TODO setting for whether or not word wrapping is enabled
			double cw = get_client_region().width();
			if (std::abs(cw - _view_width) > 0.1) { // TODO magik!
				_view_width = cw;
				{
					performance_monitor mon(u8"recalculate_wrapping");
					_fmt.set_softbreaks(_recalculate_wrapping_region(0, _doc->get_linebreaks().num_chars()));
				}
				_on_editing_visual_changed();
			}
		}
		/// Calculates the horizontal position of a caret, and returns the result as a \ref caret_data.
		[[nodiscard]] caret_data _get_caret_data(caret_position caret) const {
			return caret_data(get_horizontal_caret_position(caret), caret.after_stall);
		}

		/// Moves the given position one character to the left, skipping any folded regions.
		std::size_t _get_previous_caret_position(std::size_t cp) {
			auto res = _fmt.get_folding().find_region_containing_or_first_before_open(cp);
			auto &it = res.entry;
			if (it != _fmt.get_folding().end()) {
				std::size_t fp = res.prev_chars + it->gap, ep = fp + it->range;
				if (ep >= cp && fp < cp) {
					return fp;
				}
			}
			return cp > 0 ? cp - 1 : 0;
		}
		/// Moves the given position one character to the right, skipping any folded regions.
		std::size_t _get_next_caret_position(std::size_t cp) {
			auto res = _fmt.get_folding().find_region_containing_or_first_after_open(cp);
			auto &it = res.entry;
			if (it != _fmt.get_folding().end()) {
				std::size_t fp = res.prev_chars + it->gap, ep = fp + it->range;
				if (fp <= cp && ep > cp) {
					return ep;
				}
			}
			std::size_t nchars = _doc->get_linebreaks().num_chars();
			return cp < nchars ? cp + 1 : nchars;
		}
		/// Moves the caret vertically according to the given information.
		///
		/// \param line The visual line that the caret is on.
		/// \param diff The number of lines to move the caret by. This can be either positive or negative.
		/// \param align The alignment of the caret, similar to \ref caret_data::alignment.
		caret_position _move_caret_vertically(std::size_t line, int diff, double align) {
			if (diff < 0 && static_cast<std::size_t>(-diff) > line) {
				line = 0;
			} else {
				line = std::min(line + diff, get_num_visual_lines() - 1);
			}
			return _hit_test_at_visual_line(line, align);
		}

		/// Closes the tooltip if it's open.
		void _close_tooltip() {
			if (_tooltip) {
				_tooltip_data.clear();
				_tooltip->destroying -= _tooltip_destroying_token;
				get_manager().get_scheduler().mark_for_disposal(*_tooltip);
				_tooltip = nullptr;
			}
		}

		// mouse & keyboard interactions
		/// Calls \ref interaction_manager::on_mouse_move().
		void _on_mouse_move(ui::mouse_move_info &info) override {
			_interaction_manager.on_mouse_move(info);
			_base::_on_mouse_move(info);
		}
		/// Calls \ref interaction_manager::on_mouse_down().
		void _on_mouse_down(ui::mouse_button_info &info) override {
			_interaction_manager.on_mouse_down(info);
			_base::_on_mouse_down(info);
		}
		/// Calls \ref interaction_manager::on_mouse_up().
		void _on_mouse_up(ui::mouse_button_info &info) override {
			_interaction_manager.on_mouse_up(info);
			_base::_on_mouse_up(info);
		}
		/// Calls \ref interaction_manager::on_capture_lost().
		void _on_capture_lost() override {
			_interaction_manager.on_capture_lost();
			_base::_on_capture_lost();
		}
		/// Shows the tooltip. The previously open tooltip is closed.
		void _on_mouse_hover(ui::mouse_hover_info &info) override {
			_base::_on_mouse_hover(info);

			caret_position pos = hit_test_for_caret(info.position.get(*this));
			if (_tooltip) {
				if (_tooltip_position == pos) {
					// don't re-create the popup when we're at the exact same position
					return;
				}
				_close_tooltip();
			}

			_tooltip_position = pos;
			// initialize tooltip contents
			for (auto &provider : get_document().get_tooltip_providers()) {
				if (auto data = provider->request_tooltip(_tooltip_position.position)) {
					_tooltip_data.emplace_back(std::move(data));
				}
			}

			if (!_tooltip_data.empty()) { // create tooltip
				_tooltip = get_manager().create_element<contents_region_tooltip>();
				for (auto &data : _tooltip_data) {
					_tooltip->get_contents_panel().children().add(*data->get_element());
				}
				// listen to the destruction of this tooltip
				_tooltip_destroying_token = _tooltip->destroying += [this]() {
					_tooltip_data.clear();
					_tooltip = nullptr;
				};
				_tooltip->set_target(_get_caret_placement(pos));

				get_window()->children().add(*_tooltip);
				_tooltip->show();
			}
		}

		// visual & layout
		/// Calls \ref _check_wrapping_width to check and recalculate the wrapping.
		void _on_layout_changed() override {
			_check_wrapping_width();
			_base::_on_layout_changed();
		}
		/// Renders all visible text.
		///
		/// \todo Cannot deal with very long lines.
		void _custom_render() const override;

		/// Called when the horizontal or vertical positions of the editor have changed. This function calls
		/// \ref interaction_manager::on_viewport_changed(), adjusts the position of the tooltip if necessary, and
		/// calls \ref _update_window_caret_position().
		void _on_viewport_changed() {
			_interaction_manager.on_viewport_changed();

			if (_tooltip) {
				_tooltip->set_target(_get_caret_placement(_tooltip_position));
			}
			_update_window_caret_position();
		}

		// construction and destruction
		/// Loads font and interaction settings.
		void _initialize() override;

		/// Checks for the \ref carets_changed event.
		bool _register_event(std::u8string_view name, std::function<void()> callback) override {
			return
				_event_helpers::try_register_event(name, u8"carets_changed", carets_changed, callback) ||
				_base::_register_event(name, std::move(callback));
		}
		/// Handles the \p whitespace_geometry, \p tab_geometry, \p crlf_geometry, \p cr_geometry, \p lf_geometry
		/// properties.
		ui::property_info _find_property_path(const ui::property_path::component_list&) const override;

		/// Registers event handlers used to forward viewport update events to \ref _interaction_manager, and to call
		/// \ref _close_tooltip() when the editor lost focus.
		void _on_editor_reference_registered() override {
			_base::_on_editor_reference_registered();

			get_editor().horizontal_viewport_changed += [this]() {
				_on_viewport_changed();
			};
			get_editor().vertical_viewport_changed += [this]() {
				_on_viewport_changed();
			};
			get_editor().lost_focus += [this]() {
				_close_tooltip();
			};
		}

		/// Unregisters from document events and dispose of \ref _tooltip if necessary.
		void _dispose() override {
			if (_doc) {
				_doc->get_buffer().begin_edit -= _begin_edit_tok;
				_doc->end_modification -= _end_modification_tok;
				_doc->end_edit -= _end_edit_tok;
				_doc->appearance_changed -= _appearance_changed_tok;
			}
			_close_tooltip();

			_base::_dispose();
		}
	};
}
