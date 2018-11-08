// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Structs used to render the contents of a \ref codepad::editor::code::editor.

#include "../buffer.h"
#include "interpretation.h"
#include "view.h"
#include "editor.h"

namespace codepad::editor::code {
	/// Indicates that no token is produced by the current component.
	struct no_token {
	};
	/// Indicates that the next token to be rendered is a character.
	struct character_token {
		/// Default constructor.
		character_token() = default;
		/// Initializes all fields of this struct.
		character_token(codepoint cp, font_style s, colord c) : value(cp), style(s), color(c) {
		}

		codepoint value = 0; ///< The character.
		font_style style = font_style::normal; ///< The style of the character.
		colord color; ///< Color of the character.
	};
	/// Indicates that the next token to be rendered is a linebreak.
	struct linebreak_token {
		/// Default constructor.
		linebreak_token() = default;
		/// Initializes all fields of this struct.
		explicit linebreak_token(line_ending le) : type(le) {
		}

		line_ending type = line_ending::none; ///< The type of this linebreak.
	};
	/// Indicates that the next token to be rendered is an image.
	struct image_gizmo_token {
		// TODO
	};
	/// Indicates that the next token to be rendered is a short clip of text.
	struct text_gizmo_token {
		/// Default constructor.
		text_gizmo_token() = default;
		/// Constructs a text gizmo with the given contents and color, and the default font.
		text_gizmo_token(str_t str, colord c) : contents(std::move(str)), color(c) {
		}
		/// Constructs a text gizmo with the given contents, color, and font.
		text_gizmo_token(str_t str, colord c, std::shared_ptr<const os::font> fnt) :
			contents(std::move(str)), color(c), font(std::move(fnt)) {
		}

		str_t contents; ///< The contents of this token.
		colord color; ///< Color used to render this token.
		/// The font used for the text. If this is empty, the normal font of the editor is used.
		std::shared_ptr<const os::font> font;
	};
	/// Contains information about a token to be rendered.
	using token = std::variant<no_token, character_token, linebreak_token, image_gizmo_token, text_gizmo_token>;
	/// Holds the result of a step of token generation.
	struct token_generation_result {
		/// Constructs this struct to indicate that no token is generated.
		token_generation_result() = default;
		/// Initializes all fields of this struct.
		template <typename T> token_generation_result(T tok, size_t diff) : result(tok), steps(diff) {
		}

		token result; ///< The generated token.
		size_t steps = 0; ///< The number of characters to move forward.
	};


	/// Iterates through a range of text in a \ref interpretation and gathers tokens to be rendered. The template
	/// parameters are extra components used in this process.
	template <typename ...Args> struct rendering_token_iterator;
	/// The most basic specialization of \ref rendering_token_iterator.
	template <> struct rendering_token_iterator<> {
	public:
		/// Constructs this \ref rendering_token_iterator with the given \ref interpretation and starting position.
		rendering_token_iterator(const interpretation &interp, size_t begpos) : _pos(begpos), _interp(interp) {
			_char_it = _interp.at_character(_pos);
			_theme_it = _interp.get_text_theme().get_iter_at(_pos);
		}
		/// Forwards the arguments in the tuple to other constructors.
		template <typename ...Args> rendering_token_iterator(std::tuple<Args...> args) :
			rendering_token_iterator(std::make_index_sequence<sizeof...(Args)>(), args) {
		}
		/// Virtual destructor.
		virtual ~rendering_token_iterator() = default;

		/// Returns the token for the character or linebreak, and advances to the next character.
		token_generation_result generate() {
			if (_char_it.is_linebreak()) {
				return token_generation_result(linebreak_token(_char_it.get_linebreak()), 1);
			}
			const auto &cpit = _char_it.codepoint();
			if (cpit.is_codepoint_valid()) {
				return token_generation_result(character_token(
					cpit.get_codepoint(), _theme_it.current_theme.style, _theme_it.current_theme.color
				), 1);
			}
			return token_generation_result(text_gizmo_token(
				editor::format_invalid_codepoint(cpit.get_codepoint()),
				editor::get_invalid_codepoint_color()
			), 1);
		}
		/// Adjusts the position of \ref _char_it and \ref _theme_it.
		void update(size_t steps) {
			_pos += steps;
			if (steps == 1) {
				_char_it.next();
				_interp.get_text_theme().incr_iter(_theme_it, _pos);
			} else if (steps > 1) {
				_char_it = _interp.at_character(_pos);
				_theme_it = _interp.get_text_theme().get_iter_at(_pos);
			}
		}

		/// Returns the base \ref rendering_token_iterator with no components.
		rendering_token_iterator<> &get_base() {
			return *this;
		}
		/// Returns the current postiion of this iterator. If this called inside the \p update() method of a
		/// component, then the returned position is that before updating.
		size_t get_position() const {
			return _pos;
		}
	protected:
		/// Forwards the arguments in the given tuple to other constructors.
		template <size_t ...Indices, typename MyArgs> rendering_token_iterator(
			std::index_sequence<Indices...>, MyArgs &&tuple
		) : rendering_token_iterator(std::get<Indices>(std::forward<MyArgs>(tuple))...) {
		}

		/// Iterator to the current character in the \ref interpretation.
		interpretation::character_iterator _char_it;
		/// Iterator to the current entry in the \ref text_theme_data that determines the theme of the text.
		text_theme_data::char_iterator _theme_it;
		size_t _pos = 0; ///< The position of character \ref _char_it points to.
		const interpretation &_interp; ///< The associated \ref interpretation.
	};
	/// The specialization of \ref rendering_token_iterator that holds a component.
	template <typename FirstComp, typename ...OtherComps> struct rendering_token_iterator<FirstComp, OtherComps...> :
		protected rendering_token_iterator<OtherComps...> {
	public:
		/// Constructs the iterator using a series of tuples, each containing the arguments for one component.
		template <typename MyArgs, typename ...OtherArgs> rendering_token_iterator(
			MyArgs &&myargs, OtherArgs &&...others
		) : rendering_token_iterator(
			std::make_index_sequence<std::tuple_size_v<std::decay_t<MyArgs>>>(),
			std::forward<MyArgs>(myargs), std::forward<OtherArgs>(others)...
		) {
		}

		/// Returns the \ref token returned by the \p next() method of the current component if it's valid, or the
		/// first valid one returned by following components.
		token_generation_result generate() {
			token_generation_result tok = _mycomp.generate(*this);
			if (!std::holds_alternative<no_token>(tok.result)) {
				return tok;
			}
			return _base::generate();
		}
		/// Updates all components.
		void update(size_t steps) {
			_mycomp.update(*this, steps);
			_base::update(steps);
		}

		/// Generates the next token, then updates using the returned number of steps.
		///
		/// \return The generated token.
		token generate_and_update() {
			token_generation_result res = generate();
			update(res.steps);
			return res.result;
		}

		using rendering_token_iterator<OtherComps...>::get_base;
		using rendering_token_iterator<OtherComps...>::get_position;
	protected:
		/// The base class that contains all components following this one.
		using _base = rendering_token_iterator<OtherComps...>;

		/// Constructer that initializes the current component. The \p std::index_sequence is used to unpack the
		/// \p std::tuple.
		template <
			size_t ...Indices, typename MyArgs, typename FirstOtherArgs, typename ...OtherArgs
		> rendering_token_iterator(
			std::index_sequence<Indices...>, MyArgs &&mytuple, FirstOtherArgs &&other_tuple, OtherArgs &&...others
		) :
			_base(
				std::make_index_sequence<std::tuple_size_v<std::decay_t<FirstOtherArgs>>>(),
				std::forward<FirstOtherArgs>(other_tuple), std::forward<OtherArgs>(others)...
			),
			_mycomp(std::get<Indices>(std::forward<MyArgs>(mytuple))...) {
		}

		FirstComp _mycomp; ///< The current component.
	};


	/// A component that inserts soft linebreaks into the document.
	struct soft_linebreak_inserter {
	public:
		/// Initializes this struct with the given \ref soft_linebreak_registry at the given position.
		soft_linebreak_inserter(const soft_linebreak_registry &reg, size_t pos) : _reg(reg) {
			_reset_position(pos);
		}

		/// Checks and generates a soft linebreak if necessary. \ref _cur_softbreak and \ref _prev_chars are updated
		/// here instead of in \ref update() because this component don't really advance the current position and
		/// will otherwise cause conflicts.
		token_generation_result generate(rendering_token_iterator<> &it) {
			if (_cur_softbreak != _reg.end() && it.get_position() == _prev_chars + _cur_softbreak->length) {
				_prev_chars += _cur_softbreak->length;
				++_cur_softbreak;
				return token_generation_result(linebreak_token(line_ending::none), 0);
			}
			return token_generation_result();
		}
		/// Updates \ref _cur_softbreak according to the given offset.
		void update(rendering_token_iterator<> &it, size_t steps) {
			if (steps > 0 && _cur_softbreak != _reg.end()) { // no update needed if not moved
				size_t targetpos = it.get_position() + steps;
				if (targetpos > _prev_chars + _cur_softbreak->length) {
					// reset once iterator to the next soft linebreak is invalid
					_reset_position(targetpos);
				}
			}
		}
	protected:
		soft_linebreak_registry::iterator _cur_softbreak; ///< Iterator to the next soft linebreak.
		const soft_linebreak_registry &_reg; ///< The registry for soft linebreaks.
		size_t _prev_chars = 0; ///< The number of characters before \ref _cur_softbreak.

		/// Resets \ref _cur_softbreak and \ref _prev_char to the given position.
		void _reset_position(size_t pos) {
			auto softbreak = _reg.get_softbreak_before_or_at_char(pos);
			_prev_chars = softbreak.prev_chars;
			_cur_softbreak = softbreak.entry;
		}
	};
	/// A component that jumps to the ends of folded regions and generates corresponding gizmos.
	struct folded_region_skipper {
	public:
		/// Initializes this struct with the given \ref folding_registry at the given position.
		folded_region_skipper(const folding_registry &reg, size_t pos) : _reg(reg) {
			_reset_position(pos);
		}

		/// Checks and skips the folded region if necessary.
		token_generation_result generate(rendering_token_iterator<> &it) {
			if (_cur_region != _reg.end() && it.get_position() >= _region_start) { // jump
				// TODO gizmo should be customizable
				return token_generation_result(
					text_gizmo_token("...", colord(0.8, 0.8, 0.8, 1.0)),
					_cur_region->range - (it.get_position() - _region_start)
				);
			}
			return token_generation_result();
		}
		/// Updates \ref _cur_region according to the given offset.
		void update(rendering_token_iterator<> &it, size_t steps) {
			if (_cur_region != _reg.end()) {
				size_t targetpos = it.get_position() + steps, regionend = _region_start + _cur_region->range;
				if (targetpos >= regionend) { // advance to the next region and check again
					++_cur_region;
					if (_cur_region != _reg.end()) {
						_region_start = regionend + _cur_region->gap;
						if (_region_start + _cur_region->range <= targetpos) { // nope, still ahead
							_reset_position(targetpos);
						}
					}
				}
			}
		}
	protected:
		folding_registry::iterator _cur_region; ///< Iterator to the next folded region.
		const folding_registry &_reg; ///< The registry for folded regions.
		size_t _region_start = 0; ///< Position of the beginning of the next folded region.

		/// Resets \ref _cur_region and \ref _region_start to the given position.
		void _reset_position(size_t pos) {
			auto region = _reg.find_region_containing_or_first_after_open(pos);
			_cur_region = region.entry;
			if (_cur_region != _reg.end()) {
				_region_start = region.prev_chars + _cur_region->gap;
			}
		}
	};


	/// Specifies how tokens are measured.
	enum class token_measurement_flags {
		normal = 0, ///< Tokens are measured normally.
		defer_text_gizmo_measurement = 1 ///< Gizmos are not measured. Use \ref get_character() to add them manually.
	};
	/// Computes the metrics of each character in a clip of text.
	struct text_metrics_accumulator {
	public:
		/// Initializes this struct with the given font, line height, and tab size.
		text_metrics_accumulator(const ui::font_family &fnt, double line_height, double tab_size) :
			_char(fnt, tab_size), _line_height(line_height) {
		}

		/// Computes the metrics for the next token.
		template <token_measurement_flags Flags = token_measurement_flags::normal> void next(const token &tok) {
			if (std::holds_alternative<linebreak_token>(tok)) {
				_y += _line_height;
				_last_length = _char.char_right();
			}
			measure_token<Flags>(_char, tok);
		}
		/// Adds the given \ref token to the \ref ui::character_metrics_accumulator.
		template <token_measurement_flags Flags = token_measurement_flags::normal> inline static void measure_token(
			ui::character_metrics_accumulator &metrics, const token &tok
		) {
			if (std::holds_alternative<character_token>(tok)) {
				auto &chartok = std::get<character_token>(tok);
				metrics.next_char(chartok.value, chartok.style);
			} else if (std::holds_alternative<image_gizmo_token>(tok)) { // TODO

			} else if (std::holds_alternative<text_gizmo_token>(tok)) {
				if constexpr (!test_bits_any(Flags, token_measurement_flags::defer_text_gizmo_measurement)) {
					auto &texttok = std::get<text_gizmo_token>(tok);
					metrics.next_gizmo(ui::text_renderer::measure_plain_text(
						texttok.contents, texttok.font ? texttok.font : metrics.get_font_family().normal
					).x);
				}
			} else if (std::holds_alternative<linebreak_token>(tok)) {
				metrics.reset();
			}
		}

		/// Returns the height of a line.
		double get_line_height() const {
			return _line_height;
		}
		/// Returns the length of the previous line.
		double get_last_line_length() const {
			return _last_length;
		}
		/// Returns the current vertical position.
		double get_y() const {
			return _y;
		}
		/// Returns the associated \ref ui::character_metrics_accumulator for modification. This is usually used with
		/// measurement flags such as \ref token_measurement_flags::defer_text_gizmo_measurement.
		ui::character_metrics_accumulator &get_modify_character() {
			return _char;
		}
		/// Returns the associated \ref ui::character_metrics_accumulator.
		const ui::character_metrics_accumulator &get_character() const {
			return _char;
		}
	protected:
		ui::character_metrics_accumulator _char; ///< Computes metrics of characters in the current line.
		double
			_y = 0.0, ///< The current vertical position.
			_last_length = 0.0, ///< The length of the previous line.
			_line_height = 0.0; ///< The height of a line.
	};

	/// A standalone component that gathers information about carets to be rendered later.
	struct caret_renderer {
	public:
		/// Constructs this struct with the given \ref caret_set, position, and a boolean indicating whether the last
		/// linebreak was a soft linebreak.
		caret_renderer(const caret_set::container &set, size_t pos, bool soft) : _carets(set) {
			// find the current caret
			_cur_caret = _carets.lower_bound(std::make_pair(pos, 0));
			if (_cur_caret != _carets.begin()) {
				auto prev = _cur_caret;
				--prev;
				if (prev->first.second > pos) {
					_cur_caret = prev;
				}
			}
			_next_caret = _cur_caret;
			if (_cur_caret != _carets.end()) {
				++_next_caret;
				_range = std::minmax(_cur_caret->first.first, _cur_caret->first.second);
				if (pos >= _range.first) { // midway in the selected region
					_sel_regions.emplace_back();
					_region_begin = 0.0;
					_in_selection = true;
				}
			}
			_last_soft_linebreak = soft;
		}

		/// Called after a token is generated and the corresponding metrics has been updated.
		void on_update(
			const rendering_token_iterator<> &iter, const text_metrics_accumulator &metrics,
			const token_generation_result &tok
		) {
			if (tok.steps > 0) { // about to move forward; this is the only chance
				_check_generate_carets_all<false>(iter, metrics, tok.result);
				_update_selection(iter, metrics, tok.result);
				_last_soft_linebreak = false;
			} else {
				if (std::holds_alternative<linebreak_token>(tok.result)) { // soft linebreak
					assert_true_logical(
						std::get<linebreak_token>(tok.result).type == line_ending::none,
						"hard linebreak with zero length"
					);
					_check_generate_carets_all<true>(iter, metrics, tok.result);
					_last_soft_linebreak = true;
				} else { // other stuff like (pure) gizmos
					// TODO
					_last_soft_linebreak = false;
				}
			}
			if (_in_selection && std::holds_alternative<linebreak_token>(tok.result)) {
				_update_selection_linebreak(metrics, std::get<linebreak_token>(tok.result));
			}
		}
		/// Called after all visible text has been laid out. This function exists to ensure that carets are rendered
		/// properly at the very end of the document.
		void finish(const rendering_token_iterator<> &iter, const text_metrics_accumulator &metrics) {
			if (_in_selection) { // close selected region
				_sel_regions.back().emplace_back(
					_region_begin, metrics.get_character().char_right(),
					metrics.get_y(), metrics.get_y() + metrics.get_line_height()
				);
			}
			if (_cur_caret != _carets.end() && _cur_caret->first.first == iter.get_position()) {
				// render the last caret
				_caret_rects.emplace_back(rectd::from_xywh(
					metrics.get_character().char_right(), metrics.get_y(),
					metrics.get_character().get_font_family().normal->get_char_entry(' ').advance,
					metrics.get_line_height()
				));
			}
		}

		/// Returns the bounding boxes of all carets.
		std::vector<rectd> &get_caret_rects() {
			return _caret_rects;
		}
		/// Returns the layout of all selected regions.
		std::vector<std::vector<rectd>> &get_selection_rects() {
			return _sel_regions;
		}
	protected:
		std::vector<rectd> _caret_rects; ///< The positions of all carets.
		std::vector<std::vector<rectd>> _sel_regions; ///< The positions of selected regions.
		caret_set::const_iterator
			_cur_caret, ///< Iterator to the current caret.
			_next_caret; ///< Iterator to the next caret.
		std::pair<size_t, size_t> _range; ///< The range of \ref _cur_caret.
		double _region_begin = 0.0; ///< Position of the leftmost border of the current selected region on this line.
		const caret_set::container &_carets; ///< The set of carets.
		bool
			_in_selection = false, ///< Indicates whether the current position is selected.
			_last_soft_linebreak = false; ///< Indicates whether the last token was a soft linebreak.

		/// Generates a caret at the current location.
		void _generate_caret(const text_metrics_accumulator &metrics, bool linebreak) {
			if (linebreak) {
				_caret_rects.push_back(rectd::from_xywh(
					metrics.get_last_line_length(),
					metrics.get_y() - metrics.get_line_height(), // y has already been updated
					metrics.get_character().get_font_family().normal->get_char_entry(' ').advance,
					metrics.get_line_height()
				));
			} else {
				_caret_rects.push_back(rectd::from_xywh(
					metrics.get_character().char_left(), metrics.get_y(),
					metrics.get_character().char_width(), metrics.get_line_height()
				));
			}
		}
		/// Checks and generates a caret if necessary, given an iterator to a caret. The iterator must not point
		/// past at the end of the container.
		template <bool AtSoftbreak> void _check_generate_caret_single(
			const rendering_token_iterator<> &iter, const text_metrics_accumulator &metrics,
			[[maybe_unused]] const token &tok, const caret_set::const_iterator &it
		) {
			if (it->first.first == iter.get_position()) {
				if constexpr (AtSoftbreak) {
					if (!it->second.softbreak_next_line) {
						_generate_caret(metrics, true);
					}
				} else {
					if (!_last_soft_linebreak || it->second.softbreak_next_line) {
						_generate_caret(metrics, std::holds_alternative<linebreak_token>(tok));
					}
				}
			}
		}
		/// Checks and generates carets for \ref _cur_caret and \ref _next_caret if necessary.
		template <bool AtSoftbreak> void _check_generate_carets_all(
			const rendering_token_iterator<> &iter, const text_metrics_accumulator &metrics, const token &tok
		) {
			if (_cur_caret != _carets.end()) {
				_check_generate_caret_single<AtSoftbreak>(iter, metrics, tok, _cur_caret);
				if (_next_caret != _carets.end()) {
					_check_generate_caret_single<AtSoftbreak>(iter, metrics, tok, _next_caret);
				}
			}
		}

		/// Updates selected regions, but \ref _in_selection is changed at most once. Thus this function should be
		/// called twice to obtain the correct result. \ref _cur_caret must not be past the end of the set of carets.
		///
		/// \return Indicates whether the state has been changed.
		bool _update_selection_state(
			const rendering_token_iterator<> &it, const text_metrics_accumulator &metrics, const token &tok
		) {
			if (_in_selection) {
				if (it.get_position() >= _range.second) {
					// finish this region
					if (std::holds_alternative<linebreak_token>(tok)) {
						_sel_regions.back().emplace_back(
							_region_begin, metrics.get_last_line_length(),
							metrics.get_y() - metrics.get_line_height(), metrics.get_y()
						);
					} else {
						_sel_regions.back().emplace_back(
							_region_begin, metrics.get_character().prev_char_right(),
							metrics.get_y(), metrics.get_y() + metrics.get_line_height()
						);
					}
					// move on to the next caret
					_cur_caret = _next_caret;
					if (_cur_caret != _carets.end()) {
						++_next_caret;
						_range = std::minmax(_cur_caret->first.first, _cur_caret->first.second);
					}
					_in_selection = false;
					return true;
				}
			} else {
				if (it.get_position() >= _range.first) {
					// start this region
					_sel_regions.emplace_back();
					_region_begin =
						std::holds_alternative<linebreak_token>(tok) ?
						metrics.get_last_line_length() :
						metrics.get_character().char_left();
					_in_selection = true;
					return true;
				}
			}
			return false;
		}
		/// Updates selected regions. This function calls \ref _update_selection_state() twice.
		void _update_selection(
			const rendering_token_iterator<> &it, const text_metrics_accumulator &metrics, const token &tok
		) {
			if (_cur_caret != _carets.end()) {
				if (_update_selection_state(it, metrics, tok)) {
					if (_cur_caret != _carets.end()) {
						_update_selection_state(it, metrics, tok);
					}
				}
			}
		}
		/// Breaks the current selected region when a linebreak is encountered. \ref _in_selection must be \p true
		/// when this function is called.
		void _update_selection_linebreak(const text_metrics_accumulator &metrics, const linebreak_token &tok) {
			double xmax = metrics.get_last_line_length();
			if (tok.type != line_ending::none) { // hard linebreak, append a space
				xmax += metrics.get_character().get_font_family().normal->get_char_entry(' ').advance;
			}
			_sel_regions.back().emplace_back(
				_region_begin, xmax, metrics.get_y() - metrics.get_line_height(), metrics.get_y()
			);
			_region_begin = 0.0;
		}
	};
}
