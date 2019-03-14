// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Code of the editing component of a \ref codepad::editors::code::editor.

#include <memory>

#include "../../core/bst.h"
#include "../editor.h"
#include "../interaction_modes.h"
#include "caret_set.h"
#include "view.h"

namespace codepad::editors::code {
	/// The main component of a \ref editor that's responsible of text editing. This element should only be used as a
	/// child of a \ref editor.
	///
	/// \todo Proper syntax highlighting, drag & drop, code folding, etc.
	/// \todo Extract caret movement code to somewhere else.
	class contents_region : public interactive_contents_region_base<caret_set> {
	public:
		constexpr static double
			/// The scrolling speed coefficient when the user drags the selection out of the element.
			move_speed_scale = 15.0,
			/// The minimum distance the mouse have to move to start dragging text.
			///
			/// \todo Use system default.
			dragdrop_distance = 5.0;

		/// Sets the \ref interpretation displayed by the contents_region.
		void set_document(std::shared_ptr<interpretation> newdoc) {
			_unbind_document_events();
			_doc = std::move(newdoc);
			_cset.reset();
			if (_doc) {
				_begin_edit_tok = (_doc->get_buffer()->begin_edit += [this](buffer::begin_edit_info &info) {
					_on_begin_edit(info);
				});
				_end_edit_tok = (_doc->end_edit_interpret += [this](buffer::end_edit_info &info) {
					_on_end_edit(info);
				});
				/*_ctx_vis_change_tok = (_doc->visual_changed += [this]() {
					_on_content_visual_changed();
					});*/
				_fmt = view_formatting(*_doc);
			} else { // empty document, only used when the contents_region's being disposed
				_fmt = view_formatting();
			}
			_on_switch_document();
		}
		/// Returns the \ref interpretation currently bound to this contents_region.
		const std::shared_ptr<interpretation> &get_document() const {
			return _doc;
		}

		/// Returns the total number of visual lines.
		size_t get_num_visual_lines() const {
			return _fmt.get_linebreaks().num_visual_lines() - _fmt.get_folding().folded_linebreaks();
		}
		/// Returns the height of a line.
		///
		/// \todo Add customizable line height.
		double get_line_height() const {
			return get_font().maximum_height();
		}

		/// Returns the length of scrolling by one tick.
		double get_vertical_scroll_delta() const override {
			return get_line_height() * _lines_per_scroll;
		}
		/// Returns the length of scrolling by one tick. Currently the same value as that returned by
		/// \ref get_vertical_scroll_delta().
		double get_horizontal_scroll_delta() const override {
			return get_vertical_scroll_delta();
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
			return _cset;
		}
		/// Sets the current carets. The provided set of carets must not be empty. This function calculates the
		/// horizontal positions of all carets, and scrolls the viewport when necessary.
		void set_carets(const std::vector<caret_selection> &cs) {
			assert_true_usage(!cs.empty(), "must have at least one caret");
			caret_set set;
			for (const caret_selection &sel : cs) {
				caret_set::entry et(sel, caret_data());
				et.second.alignment = get_horizontal_caret_position(_extract_position(et));
				set.add(et);
			}
			set_carets_keepdata(std::move(set));
		}
		/// \overload
		///
		/// \note The alignment specified in \p cs is ignored.
		void set_carets(caret_set cs) {
			assert_true_usage(!cs.carets.empty(), "must have at least one caret");
			for (auto &sel : cs.carets) {
				sel.second.alignment = get_horizontal_caret_position(_extract_position(sel));
			}
			set_carets_keepdata(std::move(cs));
		}
		/// Sets tue current carets, keeping any addtional data specified in \p cs. This function scrolls the
		/// viewport when necessary.
		///
		/// \todo Find a better position to scroll to.
		void set_carets_keepdata(caret_set cs) {
			assert_true_logical(!cs.carets.empty(), "must have at least one caret");
			_cset = std::move(cs);
			_make_caret_visible(_extract_position(*_cset.carets.rbegin()));
			_on_carets_changed();
		}

		/// Adds the given caret to this \ref contents_region.
		void add_caret(caret_selection_position c) override {
			auto it = _cset.add(caret_set::entry(
				caret_selection(c.caret, c.selection), caret_data(0.0, c.caret_at_back)
			));
			// the added caret may be merged, so alignment is calculated later
			it->second.alignment = get_horizontal_caret_position(_extract_position(*it));
			_on_carets_changed();
		}
		/// Removes the given caret.
		void remove_caret(caret_set::const_iterator it) override {
			_cset.carets.erase(it);
			_on_carets_changed();
		}
		/// Clears all carets from this \ref contents_region.
		void clear_carets() override {
			_cset.carets.clear();
			_on_carets_changed();
		}
		/// Extracts a \ref caret_selection_position.
		caret_selection_position extract_caret_selection_position(const caret_set::entry &et) const override {
			return caret_selection_position(et.first.first, et.first.second, et.second.softbreak_next_line);
		}

		/// Moves all carets. The caller needs to give very precise instructions on how to move them.
		///
		/// \param gd A function-like object that takes a \ref caret_set::entry as input and returns the position
		///           it should move to, also as a \ref caret_set::entry. It is also responsible of providing the
		///           additional data associated with the new caret since this function calls \ref set_carets_keepdata
		///           to update the carets.
		template <typename GetData> void move_carets_raw(const GetData &gd) {
			caret_set ncs;
			for (const auto &et : _cset.carets) {
				ncs.add(gd(et));
			}
			set_carets_keepdata(std::move(ncs));
		}
	protected:
		/// Used by \ref move_carets to interpret and organize the return values of the function-like objects.
		/// This version takes a \ref caret_position -like \p std::pair, calculates the horizontal position of
		/// the caret, and returns the result. \ref caret_position itself is not used because it would be hard
		/// for \ref move_carets to determine the new position.
		caret_set::entry _complete_caret_entry(std::pair<size_t, bool> fst, size_t scnd) {
			return caret_set::entry(
				caret_selection(fst.first, scnd), _get_caret_data(caret_position(fst.first, fst.second))
			);
		}
		/// Used by \ref move_carets to interpret and organize the return values of the function-like objects.
		/// This version takes a position and a \ref caret_data, and returns the combined result.
		caret_set::entry _complete_caret_entry(std::pair<size_t, caret_data> fst, size_t scnd) {
			return caret_set::entry(caret_selection(fst.first, scnd), fst.second);
		}
	public:
		/// Moves all carets to specified positions.
		///
		/// \param gp A function-like object that takes a \ref caret_set::entry as input and returns either: 1) a
		///           <tt>std::pair<size_t, bool></tt>, containing the new position and a boolean value indicating
		///           whether to treat the caret as if it were on the next line had it been at a soft linebreak, or
		///           2) a <tt>std::pair<size_t, caret_data></tt>, containing the new position and related data. This
		///           object is used when \p continueselection is \p true, or when the selection is empty (i.e.,
		///           <tt>caret_selection::first == caret_selection::second</tt>). That is, this object focuses on
		///           moving only the caret and does not care about the selection.
		/// \param sp Similar to \p gp, except that this object is used when \p continueselection is false and the
		///           selection is not empty. In other words, this object is used when the selection should be
		///           cancelled.
		/// \param continueselection Indicate whether the user wishes to keep and edit the selections. If so, the
		///                          non-caret ends of the selections will not be changed, and only the caret ends of
		///                          the selections are changed according to \p gp. Otherwise, selections are
		///                          cancelled according to \p sp.
		/// \remark If \p continueselection is \p true, the non-caret ends of the selections will be kept; otherwise,
		///         they will be set to the same values as the caret ends (the values returned by the function-like
		///         objects), cancelling all selections.
		template <typename GetPos, typename SelPos> void move_carets(
			const GetPos &gp, const SelPos &sp, bool continueselection
		) {
			if (continueselection) {
				move_carets_raw([this, &gp](const caret_set::entry &et) {
					return _complete_caret_entry(gp(et), et.first.second);
				});
			} else {
				move_carets_raw([this, &gp, &sp](const caret_set::entry &et) {
					if (et.first.first == et.first.second) {
						auto x = gp(et);
						return _complete_caret_entry(x, x.first);
					}
					auto x = sp(et);
					return _complete_caret_entry(x, x.first);
				});
			}
		}
		/// Moves all carets to specified positions, treating selections and non-selections the same. This function
		/// is a shorthand for \ref move_carets(const GetPos&, const SelPos&, bool)
		/// "move_carets(gp, gp, continueselection)".
		template <typename GetPos> void move_carets(const GetPos &gp, bool continueselection) {
			return move_carets(gp, gp, continueselection);
		}

		/// Returns whether the contents_region is currently in insert mode.
		bool is_insert_mode() const {
			return _insert;
		}
		/// Sets whether the contents_region is currently in insert mode.
		void set_insert_mode(bool v) {
			_insert = v;
			_reset_caret_animation();
		}
		/// Toggles insert mode.
		void toggle_insert_mode() {
			set_insert_mode(!is_insert_mode());
		}

		// edit operations
		/// Inserts the input text at each caret.
		void on_text_input(str_view_t text) override {
			_interaction_manager.on_edit_operation();
			// encode added content
			byte_string str;
			const std::byte
				*it = reinterpret_cast<const std::byte*>(text.data()),
				*end = it + text.size();
			while (it != end) {
				codepoint cp;
				if (encodings::utf8::next_codepoint(it, end, cp)) {
					str.append(_doc->get_encoding()->encode_codepoint(cp));
				} else {
					logger::get().log_warning(CP_HERE, "skipped invalid byte sequence in input");
				}
			}
			_doc->on_insert(_cset, str, this);
		}
		/// Calls \ref interpretation::on_backspace() with the current set of carets.
		void on_backspace() {
			_interaction_manager.on_edit_operation();
			_doc->on_backspace(_cset, this);
		}
		/// Calls \ref interpretation::on_delete() with the current set of carets.
		void on_delete() {
			_interaction_manager.on_edit_operation();
			_doc->on_delete(_cset, this);
		}
		/// Called when the user presses the `enter' key to insert a line break at all carets.
		void on_return() {
			_interaction_manager.on_edit_operation();
			std::u32string_view le = line_ending_to_string(_doc->get_default_line_ending());
			byte_string encoded;
			for (char32_t c : le) {
				encoded.append(_doc->get_encoding()->encode_codepoint(c));
			}
			_doc->on_insert(_cset, encoded, this);
		}
		/// Checks if there are editing actions available for undo-ing, and calls \ref undo if there is.
		///
		/// \return \p true if an action has been reverted.
		bool try_undo() {
			if (_doc->get_buffer()->can_undo()) {
				_interaction_manager.on_edit_operation();
				_doc->get_buffer()->undo(this);
				return true;
			}
			return false;
		}
		/// Checks if there are editing actions available for redo-ing, and calls \ref redo if there is.
		///
		/// \return \p true if an action has been restored.
		bool try_redo() {
			if (_doc->get_buffer()->can_redo()) {
				_interaction_manager.on_edit_operation();
				_doc->get_buffer()->redo(this);
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
		std::pair<size_t, size_t> get_visible_visual_lines(double top, double bottom) const {
			double lh = get_line_height();
			return std::make_pair(
				static_cast<size_t>(std::max(0.0, top / lh)),
				std::min(static_cast<size_t>(std::max(0.0, bottom / lh)) + 1, get_num_visual_lines())
			);
		}
		/// Similar to the other \ref get_visible_visual_lines(), except that this function uses the current viewport of the
		/// \ref editor as the parameters.
		std::pair<size_t, size_t> get_visible_visual_lines() const {
			double top = editor::get_encapsulating(*this)->get_vertical_position() - get_padding().top;
			return get_visible_visual_lines(top, top + get_layout().height());
		}
		/// Returns the caret position corresponding to a given position. Note that the offset is relative to the
		/// top-left corner of the document rather than that of this element.
		caret_position hit_test_for_caret_document(vec2d offset) const {
			size_t line = static_cast<size_t>(std::max(offset.y / get_line_height(), 0.0));
			return _hit_test_at_visual_line(std::min(line, get_num_visual_lines() - 1), offset.x);
		}
		/// Shorthand for \ref hit_test_for_caret_document when the coordinates are relative to this \ref ui::element.
		///
		/// \todo Need changes after adding horizontal scrolling.
		caret_position hit_test_for_caret(vec2d pos) const override {
			return hit_test_for_caret_document(vec2d(
				pos.x, pos.y + editor::get_encapsulating(*this)->get_vertical_position()
			));
		}
		/// Returns the horizontal visual position of a caret.
		///
		/// \return The horizontal position of the caret, relative to the leftmost of the document.
		double get_horizontal_caret_position(caret_position pos) const {
			return _get_caret_pos_x_at_visual_line(_get_visual_line_of_caret(pos), pos.position);
		}

		/// Moves all carets one character to the left. If \p continueselection is \p false, then all carets that have
		/// selected regions will be placed at the front of the selection.
		///
		/// \param continueselection Indicates whether selected regions should be kept.
		void move_all_carets_left(bool continueselection) {
			move_carets([this](const caret_set::entry &et) {
				return std::make_pair(_move_caret_left(et.first.first), true);
			}, [](const caret_set::entry &et) -> std::pair<size_t, bool> {
				if (et.first.first < et.first.second) {
					return {et.first.first, et.second.softbreak_next_line};
				}
				return {et.first.second, true};
			}, continueselection);
		}
		/// Moves all carets one character to the right. If \p continueselection is \p false, then all carets that have
		/// selected regions will be placed at the back of the selection.
		///
		/// \param continueselection Indicates whether selected regions should be kept.
		void move_all_carets_right(bool continueselection) {
			move_carets([this](const caret_set::entry &et) {
				return std::make_pair(_move_caret_right(et.first.first), false);
			}, [](const caret_set::entry &et) -> std::pair<size_t, bool> {
				if (et.first.first > et.first.second) {
					return {et.first.first, et.second.softbreak_next_line};
				}
				return {et.first.second, false};
			}, continueselection);
		}
		/// Moves all carets vertically by a given number of lines.
		void move_all_carets_vertically(int offset, bool continue_selection) {
			move_carets([this, offset](const caret_set::entry &et) {
				auto res = _move_caret_vertically(
					_get_visual_line_of_caret(_extract_position(et)), offset, et.second.alignment
				);
				return std::make_pair(res.position, caret_data(et.second.alignment, res.at_back));
			}, [this, offset](const caret_set::entry &et) {
				size_t ml;
				double bl;
				if ((et.first.first > et.first.second) == (offset > 0)) {
					ml = _get_visual_line_of_caret(_extract_position(et));
					bl = et.second.alignment;
				} else {
					ml = _get_visual_line_of_caret(caret_position(et.first.second));
					bl = get_horizontal_caret_position(caret_position(et.first.second));
				}
				auto res = _move_caret_vertically(ml, offset, bl);
				return std::make_pair(res.position, caret_data(bl, res.at_back));
			}, continue_selection);
		}
		/// Moves all carets one line up.
		void move_all_carets_up(bool continue_selection) {
			move_all_carets_vertically(-1, continue_selection);
		}
		/// Moves all carets one line down.
		void move_all_carets_down(bool continue_selection) {
			move_all_carets_vertically(1, continue_selection);
		}
		/// Moves all carets to the beginning of the lines they're at.
		///
		/// \param continueselection Indicates whether selected regions should be kept.
		void move_all_carets_to_line_beginning(bool continueselection) {
			move_carets([this](const caret_set::entry &et) {
				return std::make_pair(
					_fmt.get_linebreaks().get_beginning_char_of_visual_line(
						_fmt.get_folding().folded_to_unfolded_line_number(
							_get_visual_line_of_caret(_extract_position(et))
						)
					).first, true
				);
			}, continueselection);
		}
		/// Moves all carets to the beginning of the lines they're at, with folding and word wrapping enabled,
		/// skipping all spaces in the front of the line.
		///
		/// \param continueselection Indicates whether selected regions should be kept.
		void move_all_carets_to_line_beginning_advanced(bool continueselection) {
			move_carets([this](const caret_set::entry &et) {
				size_t
					visline = _get_visual_line_of_caret(_extract_position(et)),
					unfolded = _fmt.get_folding().folded_to_unfolded_line_number(visline);
				auto linfo = _fmt.get_linebreaks().get_line_info(unfolded);
				size_t begp = std::max(linfo.first.first_char, linfo.second.prev_chars), exbegp = begp;
				if (linfo.first.first_char >= linfo.second.prev_chars) {
					size_t nextsb =
						linfo.second.entry == _fmt.get_linebreaks().end() ?
						std::numeric_limits<size_t>::max() :
						linfo.second.prev_chars + linfo.second.entry->length;
					for (
						auto cit = _doc->at_character(begp);
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
				return std::make_pair(et.first.first == exbegp ? begp : exbegp, true);
			}, continueselection);
		}
		/// Moves all carets to the end of the lines they're at, with folding and word wrapping enabled.
		///
		/// \param continueselection Indicates whether selected regions should be kept.
		void move_all_carets_to_line_ending(bool continueselection) {
			move_carets([this](caret_set::entry cp) {
				return std::make_pair(
					_fmt.get_linebreaks().get_past_ending_char_of_visual_line(
						_fmt.get_folding().folded_to_unfolded_line_number(
							_get_visual_line_of_caret(_extract_position(cp)) + 1
						) - 1
					).first, caret_data(std::numeric_limits<double>::max(), false)
				);
			}, continueselection);
		}
		/// Cancels all selected regions.
		void cancel_all_selections() {
			caret_set ns;
			for (const auto &caret : _cset.carets) {
				ns.add(caret_set::entry(
					caret_selection(caret.first.first, caret.first.first), caret.second)
				);
			}
			set_carets_keepdata(std::move(ns));
		}

		info_event<>
			/// Invoked when the visual of \ref _doc has changed, e.g., when it is modified, when the
			/// \ref document to which \ref _doc points is changed, or when the theme of \ref _doc is changed.
			content_visual_changed,
			content_modified, ///< Invoked when the \ref document is modified or changed by \ref set_document.
			/// Invoked when the set of carets is changed. Note that this does not necessarily mean that the result
			/// of \ref get_carets will change.
			carets_changed,
			/// Invoked when visuals particular to this view is changed, like folding or word wrapping.
			editing_visual_changed,
			/// Invoked when folded regions are added or removed. Note that this is not called when folded
			/// regions are changed due to edits made from other views.
			///
			/// \todo Invoke this when folded regions are changed due to edits made from other views.
			folding_changed;

		/// Returns the string representation of an invalid codepoint.
		///
		/// \todo This should be customizable.
		inline static str_t format_invalid_codepoint(codepoint value) {
			constexpr static size_t _buffer_size = 20;
			static char _buf[_buffer_size];

			std::snprintf(_buf, _buffer_size, "[0x%lX]", static_cast<unsigned long>(value));
			return _buf;
		}

		/// Returns the color of invalid codepoints.
		///
		/// \todo This should be customizable.
		inline static colord get_invalid_codepoint_color() {
			return colord(1.0, 0.2, 0.2, 1.0);
		}

		/// Sets the \ref ui::font_family used by all contents_region instances. The caller must make sure in advance that
		/// global variables relating to fonts have been properly initialized.
		inline static void set_font(const ui::font_family &ff) {
			_get_appearance().family = ff;
		}
		/// Returns the \ref ui::font_family used by all contents_region instances. The caller must make sure in advance that
		/// global variables relating to fonts have been properly initialized.
		inline static const ui::font_family &get_font() {
			return _get_appearance().family;
		}

		/// Casts the obtained \ref components_region_base to the correct type.
		inline static contents_region *get_from_editor(editor &edt) {
			return dynamic_cast<contents_region*>(edt.get_contents_region());
		}

		/// Returns the default class used by elements of type \ref contents_region.
		inline static str_view_t get_default_class() {
			return CP_STRLIT("code_contents_region");
		}
		/// Returns the class used by carets under `insert' mode.
		inline static str_view_t get_insert_caret_class() {
			return CP_STRLIT("code_insert_caret");
		}
		/// Returns the class used by carets under `overwrite' mode.
		inline static str_view_t get_overwrite_caret_class() {
			return CP_STRLIT("code_overwrite_caret");
		}
		/// Returns the class used by selected regions.
		inline static str_view_t get_contents_region_selection_class() {
			return CP_STRLIT("code_selection");
		}
	protected:
		/// Extracts a \ref _caret_position from a \ref caret_set::entry.
		inline static caret_position _extract_position(const caret_set::entry &entry) {
			return caret_position(entry.first.first, entry.second.softbreak_next_line);
		}

		std::shared_ptr<interpretation> _doc; ///< The \ref interpretation bound to this contents_region.
		info_event<buffer::begin_edit_info>::token _begin_edit_tok; ///< Used to listen to \ref buffer::begin_edit.
		info_event<buffer::end_edit_info>::token _end_edit_tok; ///< Used to listen to \ref buffer::end_edit.

		interaction_manager<caret_set> _interaction_manager; ///< The \ref interaction_manager.
		caret_set _cset; ///< The set of carets.
		double _lines_per_scroll = 3.0; ///< The number of lines to scroll per `tick'.
		bool _insert = true; ///< Indicates whether the contents_region is in `insert' mode.

		ui::visual_configuration
			_caret_cfg, ///< Used to render all carets.
			_sel_cfg;  ///< Used to render rectangular parts of selected regions.
		/// Element state of carets and selections.
		ui::element_state_id _editrgn_state = ui::normal_element_state_id;

		view_formatting _fmt; ///< The \ref view_formatting associated with this contents_region.
		double _view_width = 0.0; ///< The width that word wrap is calculated according to.

		/// Contains additional configuration of editors' appearance.
		///
		/// \todo Make this into a setting?
		struct _appearance_config {
			ui::font_family family; ///< The \ref ui::font_family used to display text.
		};
		/// Returns the global \ref _appearance_config.
		static _appearance_config &_get_appearance();

		/// Returns the visual line that the given caret is on.
		size_t _get_visual_line_of_caret(caret_position pos) const {
			auto
				res = _fmt.get_linebreaks().get_visual_line_and_column_and_softbreak_before_or_at_char(pos.position);
			size_t unfolded = std::get<0>(res);
			if (
				!pos.at_back &&
				std::get<2>(res).entry != _fmt.get_linebreaks().begin() &&
				std::get<2>(res).prev_chars == pos.position
				) {
				--unfolded;
			}
			return _fmt.get_folding().unfolded_to_folded_line_number(unfolded);
		}
		/// Given a line index and a horizontal position, returns the closest caret position.
		///
		/// \param line The visual line index.
		/// \param x The horizontal position.
		caret_position _hit_test_at_visual_line(size_t line, double x) const;
		/// Returns the horizontal position of a caret. This function is used when the line of the caret has been
		/// previously obtained to avoid repeated calls to \ref _get_line_of_caret.
		///
		/// \param line The visual line that the caret is on.
		/// \param position The position of the caret in the whole document.
		double _get_caret_pos_x_at_visual_line(size_t line, size_t position) const;

		/// Called when the vertical position of the document is changed or when the carets have been moved,
		/// to update the caret position used by IMs.
		///
		/// \todo Maybe also take into account the width of the character.
		void _update_window_caret_position() {
			ui::window_base *wnd = get_window();
			// when selecting with a mouse, it's possible that there are no carets in _cset at all
			if (!_cset.carets.empty() && wnd != nullptr) {
				auto entry = _cset.carets.begin();
				size_t visline = _get_visual_line_of_caret(_extract_position(*entry));
				double lh = get_line_height();
				vec2d pos = get_client_region().xmin_ymin();
				wnd->set_active_caret_position(rectd::from_xywh(
					pos.x + _get_caret_pos_x_at_visual_line(visline, entry->first.first),
					pos.y + lh * static_cast<double>(visline) -
					editor::get_encapsulating(*this)->get_vertical_position(), 0.0, lh
				));
			}
		}
		/// Moves the viewport so that the given caret is visible.
		void _make_caret_visible(caret_position caret) { // TODO
			/*editor *cb = component_helper::get_editor(*this);
			double fh = get_line_height();
			size_t
				ufline = _get_line_of_caret(caret),
				fline = _fmt.get_folding().unfolded_to_folded_line_number(ufline);
			vec2d np(
				_get_caret_pos_x_at_visual_line(fline, caret.position),
				static_cast<double>(fline + 1) * fh + get_padding().top
			);
			cb->make_point_visible(np);
			np.y -= fh;
			cb->make_point_visible(np);*/
		}

		/// Unbinds events from \ref _doc if necessary.
		void _unbind_document_events() {
			if (_doc) {
				_doc->get_buffer()->begin_edit -= _begin_edit_tok;
				_doc->end_edit_interpret -= _end_edit_tok;
				/*_doc->visual_changed -= _ctx_vis_change_tok;*/
			}
		}

		/// Simply calls \ref _on_carets_changed().
		void _on_temporary_carets_changed() override {
			_on_carets_changed();
		}

		/// Called when \ref buffer::begin_edit is triggered. Prepares \ref _cset for adjustments by calculating
		/// byte positions of each caret. If the source element is not this element, also calls
		/// \ref interaction_manager::on_edit_operation() since it must not have been called previously.
		void _on_begin_edit(buffer::begin_edit_info &info) {
			if (info.source_element != this) {
				_interaction_manager.on_edit_operation();
			}
			_cset.calculate_byte_positions(*_doc);
			_fmt.prepare_for_edit(*_doc);
		}
		/// Called when \ref buffer::end_edit is triggered. Performs necessary adjustments to the view, invokes
		/// \ref content_modified, then calls \ref _on_content_visual_changed.
		void _on_end_edit(buffer::end_edit_info&);
		/// Called when the associated \ref document has been changed to another. Invokes \ref content_modified
		/// and calls \ref _on_content_visual_changed.
		void _on_switch_document() {
			content_modified.invoke();
			_on_content_visual_changed();
		}
		/// Called when the visual of the \ref document has changed, e.g., when it has been modified, swapped, or
		/// when its \ref document::_theme "theme" has changed. Invokes \ref content_visual_changed and calls
		/// \ref _on_editing_visual_changed.
		void _on_content_visual_changed() {
			content_visual_changed.invoke();
			_on_editing_visual_changed();
		}

		/// Called when the set of carets has changed. This can occur when the user calls \p set_caret, or when
		/// the current selection is being edited by the user. This function updates input method related
		/// information, resets the caret animation, invokes \ref carets_changed, and calls \ref invalidate_visual.
		void _on_carets_changed() {
			get_window()->interrupt_input_method();
			_update_window_caret_position();
			_reset_caret_animation();
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
		std::vector<size_t> _recalculate_wrapping_region(size_t, size_t) const;
		/// Adjusts and recalculates caret positions from \ref caret_data::bytepos_first and
		/// \ref caret_data::bytepos_second, after an edit has been made.
		///
		/// \todo Carets after undo-ing and redo-ing.
		void _adjust_recalculate_caret_char_positions(buffer::end_edit_info &info) {
			caret_set newcarets;
			interpretation::character_position_converter cvt(*_doc);
			// all byte positions calculated below may not be the exact first byte of the corresponding character,
			// and thus cannot be used as bytepos_first and bytepos_second
			// also, carets may merge into one another
			if (info.source_element == this && info.type == edit_type::normal) {
				for (const buffer::modification_position &pos : info.positions) {
					size_t bp = pos.position + pos.added_range, cp = cvt.byte_to_character(bp);
					caret_set::entry et({cp, cp}, _get_caret_data(caret_position(cp, false)));
					newcarets.add(et);
				}
			} else {
				buffer::position_patcher patcher(info.positions);
				for (auto &pair : _cset.carets) {
					size_t bp1 = pair.second.bytepos_first, bp2 = pair.second.bytepos_second, cp1, cp2;
					if (bp1 == bp2) {
						bp1 = bp2 = patcher.patch<buffer::position_patcher::strategy::back>(bp1);
						cp1 = cp2 = cvt.byte_to_character(bp1);
					} else {
						if (bp1 < bp2) {
							bp1 = patcher.patch<buffer::position_patcher::strategy::back>(bp1);
							cp1 = cvt.byte_to_character(bp1);
							bp2 = patcher.patch<buffer::position_patcher::strategy::back>(bp2);
							cp2 = cvt.byte_to_character(bp2);
						} else {
							bp2 = patcher.patch<buffer::position_patcher::strategy::back>(bp2);
							cp2 = cvt.byte_to_character(bp2);
							bp1 = patcher.patch<buffer::position_patcher::strategy::back>(bp1);
							cp1 = cvt.byte_to_character(bp1);
						}
					}
					caret_set::entry et(
						{cp1, cp2}, _get_caret_data(caret_position(cp1, pair.second.softbreak_next_line))
					);
					newcarets.add(et);
				}
			}
			set_carets_keepdata(std::move(newcarets));
		}

		/// Checks if line wrapping needs to be calculated.
		///
		/// \todo Recalculate the alignment of cursors.
		void _check_wrapping_width() {
			double cw = get_client_region().width();
			if (std::abs(cw - _view_width) > 0.1) { // TODO magik!
				_view_width = cw;
				{
					performance_monitor mon(CP_STRLIT("recalculate_wrapping"));
					_fmt.set_softbreaks(_recalculate_wrapping_region(0, _doc->get_linebreaks().num_chars()));
				}
				_on_editing_visual_changed();
			}
		}
		/// Calculates the horizontal position of a caret, and returns the result as a \ref caret_data.
		caret_data _get_caret_data(caret_position caret) const {
			return caret_data(get_horizontal_caret_position(caret), caret.at_back);
		}

		/// Moves the given position one character to the left, skipping any folded regions.
		size_t _move_caret_left(size_t cp) {
			auto res = _fmt.get_folding().find_region_containing_or_first_before_open(cp);
			auto &it = res.entry;
			if (it != _fmt.get_folding().end()) {
				size_t fp = res.prev_chars + it->gap, ep = fp + it->range;
				if (ep >= cp && fp < cp) {
					return fp;
				}
			}
			return cp > 0 ? cp - 1 : 0;
		}
		/// Moves the given position one character to the right, skipping any folded regions.
		size_t _move_caret_right(size_t cp) {
			auto res = _fmt.get_folding().find_region_containing_or_first_after_open(cp);
			auto &it = res.entry;
			if (it != _fmt.get_folding().end()) {
				size_t fp = res.prev_chars + it->gap, ep = fp + it->range;
				if (fp <= cp && ep > cp) {
					return ep;
				}
			}
			size_t nchars = _doc->get_linebreaks().num_chars();
			return cp < nchars ? cp + 1 : nchars;
		}
		/// Moves the caret vertically according to the given information.
		///
		/// \param line The visual line that the caret is on.
		/// \param diff The number of lines to move the caret by. This can be either positive or negative.
		/// \param align The alignment of the caret, similar to \ref caret_data::alignment.
		caret_position _move_caret_vertically(size_t line, int diff, double align) {
			if (diff < 0 && static_cast<size_t>(-diff) > line) {
				line = 0;
			} else {
				line = std::min(line + diff, get_num_visual_lines() - 1);
			}
			return _hit_test_at_visual_line(line, align);
		}

		// mouse & keyboard interactions
		/// Calls \ref _update_mouse_selection to update the current selection and mouse position, then
		/// starts drag-dropping if the mouse has moved far enough from where it's pressed.
		void _on_mouse_move(ui::mouse_move_info &info) override {
			_interaction_manager.on_mouse_move(info);
			element::_on_mouse_move(info);
		}
		/// If the primary mouse button is pressed, and the mouse is not in a selected region, then starts editing
		/// the current selected region. If the mouse is inside a selected region, starts monitoring the user's
		/// actions for drag-dropping. Otherwise, if the tertiary mouse button is pressed, starts block selection.
		///
		/// \todo Implement block selection.
		void _on_mouse_down(ui::mouse_button_info &info) override {
			_interaction_manager.on_mouse_down(info);
			element::_on_mouse_down(info);
		}
		/// Calls \ref _on_mouse_lbutton_up if the primary mouse button is released.
		void _on_mouse_up(ui::mouse_button_info &info) override {
			_interaction_manager.on_mouse_up(info);
			element::_on_mouse_up(info);
		}
		/// Calls \ref _on_mouse_lbutton_up.
		void _on_capture_lost() override {
			_interaction_manager.on_capture_lost();
			element::_on_capture_lost();
		}
		/// Updates mouse selection.
		void _on_update() override {
			_interaction_manager.on_update();
			element::_on_update();
		}

		// visual & layout
		/// Sets the correct class of \ref _caret_cfg, resets the animation of carets, and schedules this element for
		/// updating.
		void _reset_caret_animation() {
			_caret_cfg = ui::visual_configuration(get_manager().get_class_visuals().get_or_default(
				is_insert_mode() ? get_insert_caret_class() : get_overwrite_caret_class()
			), _editrgn_state);
			get_manager().get_scheduler().schedule_visual_config_update(*this);
		}
		/// Updates \ref _editrgn_state.
		void _update_misc_region_state() {
			if (auto *edt = editor::get_encapsulating(*this)) {
				ui::element_state_id sid = edt->get_state() & get_manager().get_predefined_states().focused;
				if (sid != _editrgn_state) {
					_editrgn_state = sid;
					_caret_cfg.on_state_changed(_editrgn_state);
					_sel_cfg.on_state_changed(_editrgn_state);
					get_manager().get_scheduler().schedule_visual_config_update(*this);
				}
			}
		}

		/// Registers handlers of events of the \ref editor.
		void _on_logical_parent_constructed() override {
			element::_on_logical_parent_constructed();
			auto *edt = editor::get_encapsulating(*this);

			edt->got_focus += [this]() {
				_update_misc_region_state();
			};
			edt->lost_focus += [this]() {
				_update_misc_region_state();
			};

			edt->horizontal_viewport_changed += [this]() {
				_interaction_manager.on_viewport_changed();
			};
			edt->vertical_viewport_changed += [this]() {
				_interaction_manager.on_viewport_changed();
			};
		}
		/// Updates caret and selection visuals.
		void _on_update_visual_configurations(ui::animation_update_info &info) override {
			element::_on_update_visual_configurations(info);
			info.update_configuration(_caret_cfg);
			info.update_configuration(_sel_cfg);
		}

		/// Calls \ref _check_wrapping_width to check and recalculate the wrapping.
		void _on_layout_changed() override {
			_check_wrapping_width();
			element::_on_layout_changed();
		}
		/// Renders all visible text.
		///
		/// \todo Cannot deal with very long lines.
		void _custom_render() override;

		// misc
		/// Sets the element to non-focusable, calls \ref _reset_caret_animation(), and initializes \ref _sel_cfg.
		void _initialize(str_view_t cls, const ui::element_metrics &metrics) override {
			element::_initialize(cls, metrics);

			_can_focus = false;

			_interaction_manager.set_contents_region(*this);
			// TODO
			_interaction_manager.activators().emplace_back(new interaction_modes::mouse_prepare_drag_mode_activator<caret_set>());
			_interaction_manager.activators().emplace_back(new interaction_modes::mouse_single_selection_mode_activator<caret_set>());

			_reset_caret_animation();
			_sel_cfg = ui::visual_configuration(
				get_manager().get_class_visuals().get_or_default(get_contents_region_selection_class())
			);
		}
		/// Unbinds the current document.
		void _dispose() override {
			_unbind_document_events();
			element::_dispose();
		}
	};

	/// Helper functions used to obtain the \ref contents_region associated with elements.
	namespace component_helper {
		/// Returns both the \ref editor and the \ref contents_region. If the returned \ref contents_region is not
		/// \p nullptr, then the returned \ref editor also won't be \p nullptr.
		inline std::pair<editor*, contents_region*> get_core_components(const ui::element &elem) {
			editor *edt = editor::get_encapsulating(elem);
			if (edt) {
				return {edt, contents_region::get_from_editor(*edt)};
			}
			return {nullptr, nullptr};
		}

		/// Returns the \ref contents_region that corresponds to the given \ref ui::element.
		inline contents_region *get_contents_region(const ui::element &elem) {
			return get_core_components(elem).second;
		}
	}
}
