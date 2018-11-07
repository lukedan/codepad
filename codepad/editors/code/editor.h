#pragma once

/// \file
/// Code of the editing component of a \ref codepad::editor::code::codebox.

#include <memory>

#include "../../core/bst.h"
#include "caret_set.h"
#include "view.h"
#include "codebox.h"

namespace codepad::editor::code {
	/// Contains information about \ref character_rendering_iterator::switching_char.
	struct switch_char_info {
		/// Default constructor.
		switch_char_info() = default;
		/// Initializes all fields of the struct.
		switch_char_info(bool jmp, size_t next) : is_jump(jmp), next_position(next) {
		}

		/// Indicates whether this is caused by a call to \ref character_rendering_iterator::jump_to.
		const bool is_jump = false;
		/// The new position that the \ref character_rendering_iterator will be at.
		const size_t next_position = 0;
	};
	/// Contains information about \ref character_rendering_iterator::switching_line.
	struct switch_line_info {
		/// Default constructor.
		switch_line_info() = default;
		/// Initializes all fields of the struct.
		explicit switch_line_info(line_ending t) : type(t) {
		}

		/// The type of the linebreak that caused this event. If this is \ref line_ending::none,
		/// then it is a soft linebreak.
		const line_ending type = line_ending::none;
	};

	/// Contains information about the position of carets with word wrapping enabled.
	///
	/// \remark Although equality and ordering operators are defined for this struct, it is worth noting that
	///         they may be inaccurate when the \ref position "positions" of both oprands are the same and the
	///         \ref position is not at the same position as a soft linebreak.
	struct caret_position {
		/// Default constructor.
		caret_position() = default;
		/// Initializes all fields of this struct.
		explicit caret_position(size_t pos, bool back = false) : position(pos), at_back(back) {
		}
		/// Extracts the caret position from a \ref caret_set::entry.
		explicit caret_position(const caret_set::entry &entry) :
			position(entry.first.first), at_back(entry.second.softbreak_next_line) {
		}

		/// Equality. This may be inaccurate when both \ref position "positions" are the same.
		friend bool operator==(caret_position lhs, caret_position rhs) {
			return lhs.position == rhs.position && lhs.at_back == rhs.at_back;
		}
		/// Inquality. This may be inaccurate when both \ref position "positions" are the same.
		friend bool operator!=(caret_position lhs, caret_position rhs) {
			return !(lhs == rhs);
		}

		/// Comparison. This may be inaccurate when both \ref position "positions" are the same.
		friend bool operator<(caret_position lhs, caret_position rhs) {
			return lhs.position == rhs.position ? (!lhs.at_back && rhs.at_back) : lhs.position < rhs.position;
		}
		/// Comparison. This may be inaccurate when both \ref position "positions" are the same.
		friend bool operator>(caret_position lhs, caret_position rhs) {
			return rhs < lhs;
		}
		/// Comparison. This may be inaccurate when both \ref position "positions" are the same.
		friend bool operator<=(caret_position lhs, caret_position rhs) {
			return !(rhs < lhs);
		}
		/// Comparison. This may be inaccurate when both \ref position "positions" are the same.
		friend bool operator>=(caret_position lhs, caret_position rhs) {
			return !(lhs < rhs);
		}

		size_t position = 0; ///< The index of the character that this caret is at.
		/// Indicates whether the caret should be considered as being before the character after it, rather than
		/// being after the character before it. For example, if this caret is at the same position as a soft
		/// linebreak, this field determines whether it appears at the end of the first line, or at the beginning
		/// of the second line.
		bool at_back = false;
	};

	/// The main component of a \ref codebox that's responsible of text editing. This element should only be used as a
	/// child of a \ref codebox.
	///
	/// \todo Proper syntax highlighting, drag & drop, code folding, etc.
	class editor : public ui::element {
		friend class codebox;
	public:
		constexpr static double
			/// The scrolling speed coefficient when the user drags the selection out of the element.
			move_speed_scale = 15.0,
			/// The minimum distance the mouse have to move to start dragging text.
			///
			/// \todo Use system default.
			dragdrop_distance = 5.0;

		/// Sets the \ref interpretation displayed by the editor.
		void set_document(std::shared_ptr<interpretation> newdoc) {
			if (_doc) {
				_doc->get_buffer()->begin_edit -= _begin_edit_tok;
				_doc->end_edit_interpret -= _end_edit_tok;
				/*_doc->visual_changed -= _ctx_vis_change_tok;*/
			}
			_doc = move(newdoc);
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
			} else { // empty document, only used when the editor's being disposed
				_fmt = view_formatting();
			}
			_on_switch_document();
		}
		/// Returns the \ref interpretation currently bound to this editor.
		const std::shared_ptr<interpretation> &get_document() const {
			return _doc;
		}

		/// Returns the total number of visual lines, with folding and word wrapping enabled.
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
		double get_scroll_delta() const {
			return get_line_height() * _lines_per_scroll;
		}
		/// Returns the vertical viewport range.
		double get_vertical_scroll_range() const {
			return get_line_height() * static_cast<double>(get_num_visual_lines() - 1) + get_layout().height();
		}

		/// If the mouse is over a selection, displays the normal cursor; otherwise displays the `I-beam' cursor.
		os::cursor get_current_display_cursor() const override {
			if (!_selecting && _is_in_selection(_mouse_cache.position)) {
				return os::cursor::normal;
			}
			return os::cursor::text_beam;
		}

		/// Returns the current set of carets.
		const caret_set &get_carets() const {
			return _cset;
		}
		/// Sets the current carets. The provided set of carets must not be empty. This function calculates the
		/// horizontal positions of all carets, and scrolls the viewport when necessary.
		void set_carets(const std::vector<caret_selection> &cs) {
			assert_true_usage(!cs.empty(), "must have at least one caret");
			caret_set set;
			for (const caret_selection &sel : cs) {
				caret_set::entry et(sel, caret_data());
				et.second.alignment = get_horizontal_caret_position(et);
				set.add(et);
			}
			set_carets_keepdata(std::move(set));
		}
		/// \overload
		///
		/// \remark The alignment specified in \p cs is ignored.
		void set_carets(caret_set cs) {
			assert_true_usage(!cs.carets.empty(), "must have at least one caret");
			for (auto &sel : cs.carets) {
				sel.second.alignment = get_horizontal_caret_position(sel);
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
			_make_caret_visible(caret_position(*_cset.carets.rbegin()));
			_on_carets_changed();
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

		/// Returns whether the editor is currently in insert mode.
		bool is_insert_mode() const {
			return _insert;
		}
		/// Sets whether the editor is currently in insert mode.
		void set_insert_mode(bool v) {
			_insert = v;
			_reset_caret_animation();
		}
		/// Toggles insert mode.
		void toggle_insert_mode() {
			set_insert_mode(!_insert);
		}

		/// Calls \ref interpretation::on_backspace() with the current set of carets.
		void on_backspace() {
			if (_selecting) {
				_end_mouse_selection();
			}
			_doc->on_backspace(_cset, this);
		}
		/// Calls \ref interpretation::on_delete() with the current set of carets.
		void on_delete() {
			if (_selecting) {
				_end_mouse_selection();
			}
			_doc->on_delete(_cset, this);
		}
		/// Checks if there are editing actions available for undo-ing, and calls \ref undo if there is.
		///
		/// \return \p true if an action has been reverted.
		bool try_undo() {
			if (_doc->get_buffer()->can_undo()) {
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
				_doc->get_buffer()->redo(this);
				return true;
			}
			return false;
		}

		/// Returns the \ref view_formatting associated with this editor.
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

		/// Returns the range of line indices that are visible for the given viewport, with word wrapping enabled but
		/// folding disabled. Note that the second line is the one past the last visible line.
		std::pair<size_t, size_t> get_visible_lines(double top, double bottom) const {
			double lh = get_line_height();
			return std::make_pair(_fmt.get_folding().folded_to_unfolded_line_number(
				static_cast<size_t>(std::max(0.0, top / lh))
			), std::min(
				_fmt.get_folding().folded_to_unfolded_line_number(
					static_cast<size_t>(std::max(0.0, bottom / lh)) + 1
				),
				_fmt.get_linebreaks().num_visual_lines()
			));
		}
		/// Similar to \ref get_visible_lines(double, double) const, except that this function uses the current viewport
		/// of this editor as the parameters.
		std::pair<size_t, size_t> get_visible_lines() const {
			double top = _get_box()->get_vertical_position() - get_padding().top;
			return get_visible_lines(top, top + get_layout().height());
		}
		/// Returns the caret position corresponding to a given position. Note that the offset is relative to the
		/// top-left corner of the document rather than the editor.
		caret_position hit_test_for_caret(vec2d offset) const {
			size_t line = static_cast<size_t>(std::max(offset.y / get_line_height(), 0.0));
			line = std::min(_fmt.get_folding().folded_to_unfolded_line_number(line), get_num_visual_lines() - 1);
			return _hit_test_unfolded_linebeg(line, offset.x);
		}
		/// Returns the horizontal visual position of a caret.
		///
		/// \return The horizontal position of the caret, relative to the leftmost of the document.
		double get_horizontal_caret_position(caret_position pos) const {
			size_t line = _get_line_of_caret(pos);
			line = _fmt.get_folding().get_beginning_line_of_folded_lines(line);
			return _get_caret_pos_x_unfolded_linebeg(line, pos.position);
		}
		/// \overload
		///
		/// \param et Contains information about the caret. Only the caret position and
		///           \ref caret_data::softbreak_next_line are used.
		double get_horizontal_caret_position(const caret_set::entry &et) const {
			return get_horizontal_caret_position(caret_position(et.first.first, et.second.softbreak_next_line));
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
		/// Moves all carets one line up. If \p continueselection is \p false, then all carets that have selected
		/// regions will be placed at the front of the selection, before being moved up. If a caret is at the topmost
		/// line and its selection is not cancelled, it will not be moved.
		///
		/// \param continueselection Indicates whether selected regions should be kept.
		void move_all_carets_up(bool continueselection) {
			move_carets([this](const caret_set::entry &et) {
				caret_position res = _move_caret_vertically(
					_get_line_of_caret(caret_position(et)), -1, et.second.alignment
				);
				return std::make_pair(res.position, caret_data(et.second.alignment, res.at_back));
			}, [this](const caret_set::entry &et) {
				size_t ml = _get_line_of_caret(caret_position(et));
				double bl = et.second.alignment;
				if (et.first.second < et.first.first) {
					ml = _get_line_of_caret(caret_position(et.first.second));
					bl = get_horizontal_caret_position(caret_position(et.first.second));
				}
				auto res = _move_caret_vertically(ml, -1, bl);
				return std::make_pair(res.position, caret_data(bl, res.at_back));
			}, continueselection);
		}
		/// Moves all carets one line down. If \p continueselection is \p false, then all carets that have selected
		/// regions will be placed at the back of the selection, before being moved down. If a caret is already at the
		/// bottommost line and its selection is not cancelled, it will not be moved.
		///
		/// \param continueselection Indicates whether selected regions should be kept.
		void move_all_carets_down(bool continueselection) {
			move_carets([this](const caret_set::entry &et) {
				auto res = _move_caret_vertically(_get_line_of_caret(caret_position(et)), 1, et.second.alignment);
				return std::make_pair(res.position, caret_data(et.second.alignment, res.at_back));
			}, [this](const caret_set::entry &et) {
				size_t ml = _get_line_of_caret(caret_position(et));
				double bl = et.second.alignment;
				if (et.first.second > et.first.first) {
					ml = _get_line_of_caret(caret_position(et.first.second));
					bl = get_horizontal_caret_position(caret_position(et.first.second));
				}
				auto res = _move_caret_vertically(ml, 1, bl);
				return std::make_pair(res.position, caret_data(bl, res.at_back));
			}, continueselection);
		}
		/// Moves all carets to the beginning of the lines they're at, with folding and word wrapping enabled.
		///
		/// \param continueselection Indicates whether selected regions should be kept.
		void move_all_carets_to_line_beginning(bool continueselection) {
			move_carets([this](const caret_set::entry &et) {
				return std::make_pair(
					_fmt.get_linebreaks().get_beginning_char_of_visual_line(
						_fmt.get_folding().get_beginning_line_of_folded_lines(
							_get_line_of_caret(caret_position(et))
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
					line = _get_line_of_caret(caret_position(et)),
					reall = _fmt.get_folding().get_beginning_line_of_folded_lines(line);
				auto linfo = _fmt.get_linebreaks().get_line_info(reall);
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
						_fmt.get_folding().get_past_ending_line_of_folded_lines(
							_get_line_of_caret(caret_position(cp))
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

		event<void>
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

		/// Sets the \ref ui::font_family used by all editor instances. The caller must make sure in advance that
		/// global variables relating to fonts have been properly initialized.
		inline static void set_font(const ui::font_family &ff) {
			_get_appearance().family = ff;
		}
		/// Returns the \ref ui::font_family used by all editor instances. The caller must make sure in advance that
		/// global variables relating to fonts have been properly initialized.
		inline static const ui::font_family &get_font() {
			return _get_appearance().family;
		}

		/// Sets the number of lines per `tick' scrolled.
		inline static void set_num_lines_per_scroll(double v) {
			_lines_per_scroll = v;
		}
		/// Sets the number of lines per `tick' scrolled.
		inline static double get_num_lines_per_scroll() {
			return _lines_per_scroll;
		}

		/// Returns the default class used by elements of type \ref editor.
		inline static str_t get_default_class() {
			return CP_STRLIT("editor");
		}
		/// Returns the class used by carets under `insert' mode.
		inline static str_t get_insert_caret_class() {
			return CP_STRLIT("insert_caret");
		}
		/// Returns the class used by carets under `overwrite' mode.
		inline static str_t get_overwrite_caret_class() {
			return CP_STRLIT("overwrite_caret");
		}
		/// Returns the class used by selected regions.
		inline static str_t get_editor_selection_class() {
			return CP_STRLIT("editor_selection");
		}
	protected:
		std::shared_ptr<interpretation> _doc; ///< The \ref interpretation bound to this editor.
		event<void>::token _ctx_vis_change_tok; ///< Token used when handling \ref document::visual_changed.
		event<buffer::begin_edit_info>::token _begin_edit_tok; ///< Used to listen to \ref buffer::begin_edit.
		event<buffer::end_edit_info>::token _end_edit_tok; ///< Used to listen to \ref buffer::end_edit.

		caret_set _cset; ///< The set of carets.
		vec2d _predrag_pos; ///< The position where the user presses the left mouse button.
		/// Records the offset of the mouse when the user drags the selection to scroll the viewport.
		double _scrolldiff = 0.0;
		/// Records the caret end's position of the selection, which the user is dragging with the mouse.
		size_t _sel_end;
		bool
			_insert = true, ///< Indicates whether the editor is in `insert' mode.
			/// Indicates whether the user, after pressing the left mouse button, can proceed to drag fragments of
			/// text if he moves the mouse further than \ref dragdrop_distance.
			_predrag = false,
			_selecting = false; ///< Indicates whehter the user is making a selection using the mouse.

		caret_position _mouse_cache; ///< The position that the mouse is currently hovering above.

		ui::visual_configuration
			_caret_cfg, ///< Used to render all carets.
			_sel_cfg;  ///< Used to render rectangular parts of selected regions.
		ui::element_state_id _editrgn_state; ///< State that's applied to regions (carets, selections, etc.).

		view_formatting _fmt; ///< The \ref view_formatting associated with this editor.
		double _view_width = 0.0; ///< The width that word wrap is calculated according to.

		/// Contains additional configuration of editors' appearance.
		///
		/// \todo Make this into a setting?
		struct _appearance_config {
			ui::font_family family; ///< The \ref ui::font_family used to display text.
		};
		static double _lines_per_scroll; ///< The number of lines per `tick' scrolled.
		/// Returns the global \ref _appearance_config.
		static _appearance_config &_get_appearance();

		/// Returns the \ref codebox that this editor is currently a child of.
		codebox *_get_box() const {
			return dynamic_cast<codebox*>(logical_parent());
		}

		/// Returns the line that the given caret is on, with wrapping considered. Folding is not taken into account,
		/// so callers may need to consult with <tt>_fmt.get_folding()</tt>.
		size_t _get_line_of_caret(caret_position pos) const {
			auto
				res = _fmt.get_linebreaks().get_visual_line_and_column_and_softbreak_before_or_at_char(pos.position);
			if (
				!pos.at_back &&
				std::get<2>(res).entry != _fmt.get_linebreaks().begin() &&
				std::get<2>(res).prev_chars == pos.position
				) {
				return std::get<0>(res) - 1;
			}
			return std::get<0>(res);
		}
		/// Given a line index and a horizontal position, returns the closest caret position.
		///
		/// \param line The line index, with word wrapping enabled but folding disabled. Note that this function
		///             still takes folding into account.
		/// \param x The horizontal position.
		caret_position _hit_test_unfolded_linebeg(size_t line, double x) const;
		/// Returns the horizontal position of a caret. This function is used when the line of the caret has been
		/// previously obtained to avoid repeated calls to \ref _get_line_of_caret. Note that calling
		/// \ref folding_registry::get_beginning_line_of_folded_lines may be necessary to obtain the correct line
		/// number.
		///
		/// \param line The line that the caret is on, with only wrapping considered (i.e., folding is not
		///             considered). If a group of lines is joined by folding, this must be the first line.
		/// \param position The position of the caret in the whole document.
		double _get_caret_pos_x_unfolded_linebeg(size_t line, size_t position) const;
		/// Returns the horizontal position of a caret. This function is used when the line of the caret has been
		/// previously obtained to avoid repeated calls to \ref _get_line_of_caret.
		///
		/// \param line The line that the caret is on, with both wrapping and folding considered.
		/// \param position The position of the caret in the whole document.
		double _get_caret_pos_x_folded_linebeg(size_t line, size_t position) const {
			return _get_caret_pos_x_unfolded_linebeg(
				_fmt.get_folding().folded_to_unfolded_line_number(line), position
			);
		}

		/// Called when the vertical position of the document is changed or when the carets have been moved,
		/// to update the caret position used by IMs.
		///
		/// \todo Maybe also take into account the width of the character.
		void _update_window_caret_position() {
			os::window_base *wnd = get_window();
			// when selecting with a mouse, it's possible that there are no carets in _cset at all
			if (!_cset.carets.empty() && wnd != nullptr) {
				auto entry = _cset.carets.begin();
				size_t wrapline = _get_line_of_caret(caret_position(*entry));
				size_t visline = _fmt.get_folding().unfolded_to_folded_line_number(wrapline);
				double lh = get_line_height();
				vec2d pos = get_client_region().xmin_ymin();
				wnd->set_active_caret_position(rectd::from_xywh(
					pos.x + _get_caret_pos_x_unfolded_linebeg(wrapline, entry->first.first),
					pos.y + lh * static_cast<double>(visline) - _get_box()->get_vertical_position(), 0.0, lh
				));
			}
		}
		/// Moves the viewport so that the given caret is visible.
		void _make_caret_visible(caret_position caret) {
			codebox *cb = _get_box();
			double fh = get_line_height();
			size_t
				ufline = _get_line_of_caret(caret),
				fline = _fmt.get_folding().unfolded_to_folded_line_number(ufline);
			vec2d np(
				_get_caret_pos_x_folded_linebeg(fline, caret.position),
				static_cast<double>(fline + 1) * fh + get_padding().top
			);
			cb->make_point_visible(np);
			np.y -= fh;
			cb->make_point_visible(np);
		}
		/// Sets the correct class of \ref _caret_cfg, resets the animation of carets, and schedules this element for
		/// updating.
		void _reset_caret_animation() {
			_caret_cfg = ui::visual_configuration(ui::manager::get().get_class_visuals().get_visual_or_default(
				_insert ? get_insert_caret_class() : get_overwrite_caret_class()
			), _editrgn_state);
			ui::manager::get().schedule_visual_config_update(*this);
		}
		/// Shorthand for \ref hit_test_for_caret when the coordinates are relative to the client region.
		///
		/// \todo Need changes after adding horizontal scrolling.
		caret_position _hit_test_for_caret_client(vec2d pos) const {
			return hit_test_for_caret(vec2d(pos.x, pos.y + _get_box()->get_vertical_position()));
		}

		/// Called when \ref buffer::begin_edit is triggered. Prepares \ref _cset for adjustments by calculating
		/// byte positions of each caret.
		void _on_begin_edit(buffer::begin_edit_info&) {
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

		/// Called to change \ref _editrgn_state.
		void _set_editrgn_state(ui::element_state_id st) {
			if (_editrgn_state != st) {
				_editrgn_state = st;
				_caret_cfg.on_state_changed(_editrgn_state);
				_sel_cfg.on_state_changed(_editrgn_state);
				ui::manager::get().schedule_visual_config_update(*this);
			}
		}
		/// Called by the parent \ref codebox when it gets focus. Adjusts the states of caret animations.
		void _on_codebox_got_focus() {
			_set_editrgn_state(with_bits_set(_editrgn_state, ui::manager::get().get_predefined_states().focused));
		}
		/// Called by the parent \ref codebox when it loses focus. Adjusts the states of caret animations.
		void _on_codebox_lost_focus() {
			_set_editrgn_state(with_bits_unset(_editrgn_state, ui::manager::get().get_predefined_states().focused));
		}

		/// Called when the user starts to make a selection using the mouse.
		void _begin_mouse_selection(size_t startpos) {
			assert_true_logical(!_selecting, "_begin_mouse_selection() called when selecting");
			_selecting = true;
			_sel_end = startpos;
			_on_carets_changed();
			get_window()->set_mouse_capture(*this);
		}
		/// Called when the user finishes making a selection using the mouse or when it's interrupted.
		void _end_mouse_selection() {
			assert_true_logical(_selecting, "_end_mouse_selection() called when not selecting");
			_selecting = false;
			// the added caret may be merged, so alignment is calculated later
			auto it = _cset.add(caret_set::entry(
				caret_selection(_mouse_cache.position, _sel_end), caret_data(0.0, _mouse_cache.at_back)
			));
			it->second.alignment = get_horizontal_caret_position(*it);
			_on_carets_changed();
			get_window()->release_mouse_capture();
		}

		/// Tests if the given position belongs to a selected region. Carets that have no selected regions
		/// are ignored.
		///
		/// \tparam Cmp Used to determine whether being at the boundaries of selected regions counts as being
		///             inside the region.
		template <typename Cmp = std::less_equal<>> bool _is_in_selection(size_t cp) const {
			auto cur = _cset.carets.lower_bound(std::make_pair(cp, cp));
			if (cur != _cset.carets.begin()) {
				--cur;
			}
			Cmp cmp;
			while (cur != _cset.carets.end() && std::min(cur->first.first, cur->first.second) <= cp) {
				if (cur->first.first != cur->first.second) {
					auto minmaxv = std::minmax(cur->first.first, cur->first.second);
					if (cmp(minmaxv.first, cp) && cmp(cp, minmaxv.second)) {
						return true;
					}
				}
				++cur;
			}
			return false;
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

		/// Updates \ref _mouse_cache and the current selection that's being edited with the mouse.
		///
		/// \param pos The mouse's position relative to the window.
		void _update_mouse_selection(vec2d pos) {
			vec2d
				rtextpos = pos - get_client_region().xmin_ymin(), clampedpos = rtextpos,
				relempos = pos - get_layout().xmin_ymin();
			_scrolldiff = 0.0f;
			if (relempos.y < 0.0) {
				clampedpos.y -= relempos.y;
				_scrolldiff = relempos.y;
				ui::manager::get().schedule_update(*this);
			} else {
				double h = get_layout().height();
				if (relempos.y > h) {
					clampedpos.y += h - relempos.y;
					_scrolldiff = relempos.y - get_layout().height();
					ui::manager::get().schedule_update(*this);
				}
			}
			auto newmouse = _hit_test_for_caret_client(clampedpos);
			if (_selecting && newmouse != _mouse_cache) {
				_on_carets_changed();
			}
			_mouse_cache = newmouse;
		}
		/// Called when the user finishes editing the current selected region, or when drag-dropping is cancelled.
		void _on_mouse_lbutton_up() {
			if (_selecting) {
				_end_mouse_selection();
			} else if (_predrag) {
				_predrag = false;
				caret_position hitp = _hit_test_for_caret_client(_predrag_pos - get_client_region().xmin_ymin());
				_cset.carets.clear();
				_cset.carets.insert(caret_set::entry(
					caret_selection(hitp.position, hitp.position), _get_caret_data(hitp)
				));
				_on_carets_changed();
				get_window()->release_mouse_capture();
			} else {
				return;
			}
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
		/// \param line The line that the caret is on, with wrapping enabled but folding disabled.
		/// \param diff The number of lines to move the caret by. This can be either positive or negative.
		/// \param align The alignment of the caret, similar to \ref caret_data::alignment.
		caret_position _move_caret_vertically(size_t line, int diff, double align) {
			line = _fmt.get_folding().unfolded_to_folded_line_number(line);
			if (diff < 0 && -diff > static_cast<int>(line)) {
				line = 0;
			} else {
				line = _fmt.get_folding().folded_to_unfolded_line_number(line + diff);
				line = std::min(line, get_num_visual_lines() - 1);
			}
			return _hit_test_unfolded_linebeg(line, align);
		}

		/// Inserts the input text at each caret.
		///
		/// \todo Handle linebreaks.
		void _on_keyboard_text(ui::text_info &info) override {
			if (_selecting) {
				_end_mouse_selection();
			}
			// encode added content
			byte_string str;
			const std::byte
				*it = reinterpret_cast<const std::byte*>(info.content.c_str()),
				*end = it + info.content.size();
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
		/// Calls \ref _update_mouse_selection to update the current selection and mouse position, then
		/// starts drag-dropping if the mouse has moved far enough from where it's pressed.
		///
		/// \todo Implement drag & drop.
		void _on_mouse_move(ui::mouse_move_info &info) override {
			_update_mouse_selection(info.new_position);
			if (_predrag) {
				if ((info.new_position - _predrag_pos).length_sqr() > dragdrop_distance * dragdrop_distance) {
					_predrag = false;
					logger::get().log_info(CP_HERE, "starting drag & drop of text");
					get_window()->release_mouse_capture();
					// TODO start drag drop
				}
			}
			element::_on_mouse_move(info);
		}
		/// If the primary mouse button is pressed, and the mouse is not in a selected region, then starts editing
		/// the current selected region. If the mouse is inside a selected region, starts monitoring the user's
		/// actions for drag-dropping. Otherwise, if the tertiary mouse button is pressed, starts block selection.
		///
		/// \todo Implement block selection.
		/// \todo Use customizable modifiers.
		void _on_mouse_down(ui::mouse_button_info &info) override {
			_mouse_cache = _hit_test_for_caret_client(info.position - get_client_region().xmin_ymin());
			if (info.button == os::input::mouse_button::primary) {
				if (!_is_in_selection(_mouse_cache.position)) {
					if (test_bits_any(info.modifiers, modifier_keys::shift)) {
						auto it = _cset.carets.end();
						--it;
						caret_selection cs = it->first;
						_cset.carets.erase(it);
						_begin_mouse_selection(cs.second);
					} else {
						if (!test_bits_any(info.modifiers, modifier_keys::control)) {
							_cset.carets.clear();
						}
						_begin_mouse_selection(_mouse_cache.position);
					}
				} else {
					_predrag_pos = info.position;
					_predrag = true;
					get_window()->set_mouse_capture(*this);
				}
			} else if (info.button == os::input::mouse_button::tertiary) {
				// TODO block selection
			}
			element::_on_mouse_down(info);
		}
		/// Calls \ref _on_mouse_lbutton_up if the primary mouse button is released.
		void _on_mouse_up(ui::mouse_button_info &info) override {
			if (info.button == os::input::mouse_button::primary) {
				_on_mouse_lbutton_up();
			}
		}
		/// Calls \ref _on_mouse_lbutton_up.
		void _on_capture_lost() override {
			_on_mouse_lbutton_up();
			element::_on_capture_lost();
		}
		/// Updates mouse selection.
		void _on_update() override {
			element::_on_update();
			if (_selecting) {
				codebox *editor = _get_box();
				editor->set_vertical_position(
					editor->get_vertical_position() +
					move_speed_scale * _scrolldiff * ui::manager::get().update_delta_time()
				);
				_update_mouse_selection(get_window()->screen_to_client(
					os::input::get_mouse_position()).convert<double>()
				);
			}
		}

		bool _on_update_visual_configurations(double time) override {
			_caret_cfg.update(time);
			_sel_cfg.update(time);
			return
				element::_on_update_visual_configurations(time) &&
				_caret_cfg.get_state().all_stationary && _sel_cfg.get_state().all_stationary;
		}

		/// Calls \ref _check_wrapping_width to check and recalculate the wrapping.
		void _on_layout_changed() override {
			_check_wrapping_width();
			ui::element::_on_layout_changed();
		}
		/// Renders all visible text.
		///
		/// \todo Cannot deal with very long lines.
		void _custom_render() override;

		/// Sets the default padding, sets the element to non-focusable, and resets the class labels of carets and
		/// selected regions.
		void _initialize(const str_t &cls, const ui::element_metrics &metrics) override {
			element::_initialize(cls, metrics);
			_can_focus = false;
			_reset_caret_animation();
			_sel_cfg = ui::visual_configuration(
				ui::manager::get().get_class_visuals().get_visual_or_default(get_editor_selection_class())
			);
		}
		/// Unbinds the current document.
		void _dispose() override {
			set_document(nullptr);
			element::_dispose();
		}
	};
}
