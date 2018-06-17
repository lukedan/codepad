#pragma once

/// \file
/// Structs used to render the contents of a \ref codepad::editor::code::editor.

#include "buffer.h"
#include "document.h"
#include "view.h"
#include "editor.h"

namespace codepad::editor::code {
	/// Iterator used to iterate through characters of part of a \ref document, calculating theme and position
	/// information along the way. This struct is only aware of the \ref document and does not take into
	/// consideration word wrapping and folding. Under the hood, this struct uses a
	/// \ref ui::character_metrics_accumulator to calculate the positions of characters.
	struct character_rendering_iterator {
	public:
		/// Constructor.
		///
		/// \param doc The \ref document to iterate through.
		/// \param lh The height of a line.
		/// \param sp Position of the first character of the range of characters to iterate through.
		/// \param pep Position past the last character of the range of characters to iterate through.
		character_rendering_iterator(const document &doc, double lh, size_t sp, size_t pep) :
			_char_it(doc.at_char(sp)), _theme_it(doc.get_text_theme().get_iter_at(sp)),
			_char_met(editor::get_font(), doc.get_tab_width()),
			_doc(&doc), _cur_pos(sp), _tg_pos(pep), _line_h(lh) {
		}
		/// Constructs this iterator with corresponding parameters of the given \ref editor, and the given range
		/// of characters.
		character_rendering_iterator(const editor &ce, size_t sp, size_t pep) :
			character_rendering_iterator(*ce.get_document(), ce.get_line_height(), sp, pep) {
		}

		/// Returns whether the current character that \ref _char_it points to is a hard linebreak.
		bool is_hard_line_break() const {
			return _char_it.is_linebreak();
		}
		/// Returns whether the iterator has reached the end of the range of characters.
		bool ended() const {
			return _cur_pos > _tg_pos || _char_it.is_end();
		}
		/// Starts to iterate through the range of characters, by calculating the positioning of the first character
		/// (if it is not a hard linebreak).
		void begin() {
			if (!is_hard_line_break()) {
				_char_met.next(_char_it.current_character(), _theme_it.current_theme.style);
			}
		}
		/// Moves to the next character. This function invokes the \ref event corresponding to the current character
		/// before updating all information.
		void next_char() {
			assert_true_usage(!ended(), "incrementing an invalid rendering iterator");
			if (is_hard_line_break()) {
				_on_linebreak(_char_it.current_linebreak());
			} else {
				switching_char.invoke_noret(false, _cur_pos + 1);
			}
			// increment iterators
			++_cur_pos;
			++_char_it;
			_doc->get_text_theme().incr_iter(_theme_it, _cur_pos);
			// update positioning
			_char_met.next(
				is_hard_line_break() ? ' ' : _char_it.current_character(), _theme_it.current_theme.style
			);
		}

		/// Creates a blank region of the given width before the current character.
		///
		/// \return The bounds of the blank region.
		rectd create_blank(double w) {
			_char_met.create_blank_before(w);
			return rectd(_char_met.prev_char_right(), _char_met.char_left(), _cury, _cury + _line_h);
		}
		/// Jumps to the given position. \ref switching_char is invoked first, then the current character is replaced
		/// with the character at that position.
		void jump_to(size_t cp) {
			switching_char.invoke_noret(true, cp);
			_cur_pos = cp;
			_char_it = _doc->at_char(_cur_pos);
			_theme_it = _doc->get_text_theme().get_iter_at(_cur_pos);
			_char_met.replace_current(
				is_hard_line_break() ? ' ' : _char_it.current_character(), _theme_it.current_theme.style
			);
		}
		/// Inserts a soft linebreak before the current character.
		void insert_soft_linebreak() {
			_on_linebreak(line_ending::none);
			_char_met.next(_char_it.current_character(), _theme_it.current_theme.style);
		}

		/// Returns the underlying \ref ui::character_metrics_accumulator.
		const ui::character_metrics_accumulator &character_info() const {
			return _char_met;
		}
		/// Returns a \ref text_theme_data::char_iterator that contains information about the theme of the current
		/// character.
		const text_theme_data::char_iterator &theme_info() const {
			return _theme_it;
		}
		/// Returns the position of the current character in the document.
		size_t current_position() const {
			return _cur_pos;
		}

		/// Returns the vertical offset of the current character.
		double y_offset() const {
			return _cury;
		}
		/// Returns the rounded vertical offset of the current character.
		int rounded_y_offset() const {
			return _rcy;
		}
		/// Returns the height a line occupies.
		double line_height() const {
			return _line_h;
		}

		/// Invoked before a new line, either one in the original \ref document or a soft linebreak inserted by
		/// \ref insert_soft_linebreak, is encountered.
		event<switch_line_info> switching_line;
		/// Invoked when the current position is about to be changed, either by \ref next_char() or
		/// \ref jump_to(size_t). Note that this should not be used as a criterion for rendering characters as
		/// \ref jump_to(size_t) can be called consecutively.
		event<switch_char_info> switching_char;
	protected:
		document::iterator _char_it; ///< Iterator to the current character in the \ref document.
		/// Iterator to the current entry in the \ref text_theme_data that determines the theme of the text.
		text_theme_data::char_iterator _theme_it;
		ui::character_metrics_accumulator _char_met; ///< Used to calculate the positioning of characters.
		const document *_doc = nullptr; ///< Pointer to the associated \ref document.
		size_t
			_cur_pos = 0, ///< Position of the current character.
			_tg_pos = 0; ///< Records the end of the range of characters.
		double
			_cury = 0.0, ///< Current vertical position relative to the top of the first rendered line.
			_line_h = 0.0; ///< The height of a line.
		int _rcy = 0; ///< Rounded vertical position.

					  /// Invokes \ref switching_line with the given \ref line_ending, then increments y position and
					  /// calls \ref ui::character_metrics_accumulator::reset "reset()" of \ref _char_met.
		void _on_linebreak(line_ending end) {
			switching_line.invoke_noret(end);
			_cury += _line_h;
			_rcy = static_cast<int>(std::round(_cury));
			_char_met.reset();
		}
	};

	namespace _details {
		/// Used to test if a class has member `check'.
		template <typename T> class is_checker {
		protected:
			using _yes = std::int8_t; ///< Type that indicates a positive result.
			using _no = std::int64_t; ///< Type that indicates a negative result.
									  /// If \p C has a member named `check', this function will match.
			template <typename C> inline static _yes _test(decltype(&C::check));
			/// If \p C doesn't have a member named `check', this function will match.
			template <typename C> inline static _no _test(...);
		public:
			/// The result, \p true if \p T has a member named `check'.
			constexpr static bool value = sizeof(_test<T>(0)) == sizeof(_yes);
		};
		/// Shorthand for \ref is_checker<T>::value.
		template <typename T> constexpr static bool is_checker_v = is_checker<T>::value;
	}
	/// Used to iterate through a part of a \ref document using a \ref character_rendering_iterator, with additional
	/// addons that modify the behavior, specified in the template argument list. Each addon provides either:
	///     1. A `check' member that returns a \p bool indicating whether the addon has made changes to the
	///        \ref character_rendering_iterator such that the `check' members need to be invoked from the beginning
	///        again immediately. These memebers will be invoked repeatedly before
	///        <tt>rendering_iterator<F, Others...>::begin()</tt> or
	///        <tt>rendering_iterator<F, Others...>::next()</tt> returns, in the order that the addons are specified
	///        in the template argument list, until all invocations return \p false. If one invocation returns
	///        \p true, all addons after it are ignored and the `check' members are invoked in order from the start
	///        once again.
	///     2. A `switching_char' member and a `switching_line' member. These two members are called whenever the
	///        corresponding events of \ref character_rendering_iterator (i.e.,
	///        \ref character_rendering_iterator::switching_line and
	///        \ref character_rendering_iterator::switching_line) are invoked, in the order that the addons are
	///        specifed in the template argument list.
	template <typename ...AddOns> struct rendering_iterator;
	/// Specialization of \ref rendering_iterator with no addons. This struct contains the
	/// \ref character_rendering_iterator.
	template <> struct rendering_iterator<> {
	public:
		/// Initializes the underlying \ref character_rendering_iterator with the contents of the given \p tuple.
		rendering_iterator(const std::tuple<const editor&, size_t, size_t> &p) :
			rendering_iterator(std::get<0>(p), std::get<1>(p), std::get<2>(p)) {
		}
		/// Initializes the underlying \ref character_rendering_iterator with the given arguments.
		rendering_iterator(const editor &ce, size_t sp, size_t pep) : _citer(ce, sp, pep) {
		}
		/// Virtual destructor.
		virtual ~rendering_iterator() = default;

		/// Indicates whether the iterator has reached the end of the given range of characters.
		///
		/// \sa character_rendering_iterator::ended() const
		bool ended() const {
			return _citer.ended();
		}

		/// Returns the underlying \ref character_rendering_iterator.
		character_rendering_iterator &char_iter() {
			return _citer;
		}
	protected:
		/// This constructor is called when a \ref rendering_iterator with addons is constructed. The argument \p arg is
		/// a \p std::tuple containing parameters used to initialize \ref _citer, and the \p std::index_sequence is
		/// ignored.
		template <size_t ...I, typename T> rendering_iterator(
			std::index_sequence<I...>, T &&arg
		) : rendering_iterator(std::forward<T>(arg)) {
		}

		character_rendering_iterator _citer; ///< The underlying \ref character_rendering_iterator.

											 /// Calls \ref character_rendering_iterator::begin().
		void _begin() {
			_citer.begin();
		}
		/// Calls \ref character_rendering_iterator::next_char().
		void _next() {
			_citer.next_char();
		}

		/// \sa rendering_iterator<F, Others...>::_check(_check_helper<false>)
		bool _check_all() {
			return false;
		}
		/// \sa rendering_iterator<F, Others...>::_switching_char(switch_char_info&, _check_helper<true>)
		void _switching_char_all(switch_char_info&) {
		}
		/// \sa rendering_iterator<F, Others...>::_switching_line(switch_line_info&, _check_helper<true>)
		void _switching_line_all(switch_line_info&) {
		}
	};
	/// The specialization of \ref rendering_iterator that contains an addon of type \p F. This struct inherits from
	/// \ref rendering_iterator<Others...> to include all other addons.
	template <
		typename F, typename ...Others
	> struct rendering_iterator<F, Others...> :
		protected rendering_iterator<Others...> {
	public:
		/// Initializes all addons and the underlying \ref character_rendering_iterator. Each argument is a
		/// <tt>std::tuple</tt> that contains the arguments for the corresponding addon, or the
		/// \ref character_rendering_iterator if it's the last argument. The contents of the <tt>tuple</tt>s are
		/// unpacked and passed to the corresponding object's constructor. Note that the addons are constructed
		/// in reverse order in which they appear in the template argument list.
		template <typename FCA, typename ...OCA> rendering_iterator(FCA &&f, OCA &&...ot) : rendering_iterator(
			std::make_index_sequence<std::tuple_size_v<std::decay_t<FCA>>>(),
			std::forward<FCA>(f), std::forward<OCA>(ot)...
		) {
			_root_base::char_iter().switching_char += [this](switch_char_info &pi) {
				_switching_char_all(pi);
			};
			_root_base::char_iter().switching_line += [this](switch_line_info &pi) {
				_switching_line_all(pi);
			};
		}

		/// Starts to iterate through the characters.
		void begin() {
			_root_base::_begin();
			while (_check_all()) {
			}
		}
		/// Moves to the next character.
		void next() {
			_root_base::_next();
			while (_check_all()) {
			}
		}

		/// Returns the addon of the given type.
		///
		/// \tparam T The type of the requested addon.
		template <typename T> T &get_addon() {
			return _get_addon_impl(_get_helper<T>());
		}

		using rendering_iterator<>::ended;
		using rendering_iterator<>::char_iter;
	private:
		/// The base class that contains all addons but the first one.
		using _direct_base = rendering_iterator<Others...>;
		/// The final base class that contains the \ref character_rendering_iterator.
		using _root_base = rendering_iterator<>;

		/// Helper struct for getting addons.
		template <typename T> struct _get_helper {
		};
		/// Invoked by \ref get_addon when the addon of the requested type is not held by this struct. Passes the
		/// call to the base class.
		template <typename T> T &_get_addon_impl(_get_helper<T>) {
			return _direct_base::template get_addon<T>();
		}
		/// Invoked by \ref get_addon when the addon of the requested type is held by this struct.
		F &_get_addon_impl(_get_helper<F>) {
			return _curaddon;
		}

		/// Helper struct used to determine if a class is a checker (i.e., has a `check' member).
		template <bool V> struct _check_helper {
		};
		/// The specialization of \ref _check_helper for the addon held by this struct.
		using _cur_check_helper = _check_helper<_details::is_checker_v<F>>;
		/// Called when the current addon is a checker. Invokes \p F::check.
		bool _check(_check_helper<true>) {
			return _curaddon.check(_root_base::_citer);
		}
		/// Called when the current addon is not a checker. Invokes \p F::switching_char.
		void _switching_char(switch_char_info &info, _check_helper<false>) {
			_curaddon.switching_char(_root_base::char_iter(), info);
		}
		/// Called when the current addon is not a checker. Invokes \p F::switching_line.
		void _switching_line(switch_line_info &info, _check_helper<false>) {
			_curaddon.switching_line(_root_base::char_iter(), info);
		}
		// disabled funcs
		/// Called when the current addon is not a checker. Simply returns \p false.
		bool _check(_check_helper<false>) {
			return false;
		}
		/// Called when the current addon is a checker. Does nothing.
		void _switching_char(switch_char_info&, _check_helper<true>) {
		}
		/// Called when the current addon is a checker. Does nothing.
		void _switching_line(switch_line_info&, _check_helper<true>) {
		}

		F _curaddon; ///< The current addon.
	protected:
		/// Constructer that initializes the current addon. The \p std::index_sequence is used to help unpack
		/// the \p std::tuple.
		template <size_t ...I, typename FCA, typename SCA, typename ...OCA> rendering_iterator(
			std::index_sequence<I...>, FCA &&f, SCA &&s, OCA &&...ot
		) : _direct_base(
			std::make_index_sequence<std::tuple_size_v<std::decay_t<SCA>>>(),
			std::forward<SCA>(s), std::forward<OCA>(ot)...
		), _curaddon(std::get<I>(std::forward<FCA>(f))...) {
		}

		/// For each addon, calls its `check' method if it has one, and returns \p true immediately if any
		/// invocation returns \p true. Finally, if none returns \p true, this function returns \p false.
		bool _check_all() {
			if (!_check(_cur_check_helper())) {
				return _direct_base::_check_all();
			}
			return true;
		}
		/// Calls \p switching_char() for all addons that are not checkers, in the order in which they are specified
		/// in the template argument list.
		void _switching_char_all(switch_char_info &info) {
			_switching_char(info, _cur_check_helper());
			_direct_base::_switching_char_all(info);
		}
		/// Calls \p switching_line() for all addons that are not checkers, in the order in which they are specified
		/// in the template argument list.
		void _switching_line_all(switch_line_info &info) {
			_switching_line(info, _cur_check_helper());
			_direct_base::_switching_line_all(info);
		}
	};


	/// Addon to \ref rendering_iterator that skips folded regions.
	struct fold_region_skipper {
	public:
		/// Initializer.
		///
		/// \param fold The \ref folding_registry.
		/// \param sp The starting position. The end position is not required.
		fold_region_skipper(const folding_registry &fold, size_t sp) {
			auto fr = fold.find_region_containing_or_first_after_open(sp);
			_nextfr = fr.entry;
			_chars = fr.prev_chars;
		}

		/// Checks if the current position is at the beginning of the folded region \ref _nextfr points to, and if
		/// so, calls \ref character_rendering_iterator::jump_to to jump the end of the folded region and return
		/// \p true to interrupt checking by all following addons and start over. Otherwise, returns \p false.
		///
		/// \todo Add gizmos (or text) for folded regions.
		bool check(character_rendering_iterator &it) {
			size_t npos = it.current_position();
			if (_nextfr != _nextfr.get_container()->end() && _chars + _nextfr->gap == npos) {
				_chars += _nextfr->gap + _nextfr->range;
				it.jump_to(_chars);
				it.create_blank(30.0); // TODO magic number
				_region_positions.emplace_back(it.character_info().char_left(), it.rounded_y_offset());
				++_nextfr;
				return true;
			}
			return false;
		}

		/// Returns the positions of all folded regions.
		const std::vector<vec2d> &get_folded_region_positions() const {
			return _region_positions;
		}
	protected:
		/// The positions of all folded regions, relative to the top left corner of the first rendered line.
		std::vector<vec2d> _region_positions;
		folding_registry::iterator _nextfr; ///< Iterator to the next folded region.
		/// The number of characters before \ref _nextfr, not including
		/// \ref folding_registry::fold_region_node_data::gap.
		size_t _chars = 0;
	};
	/// Inserts soft linebreaks into the document according to a given \ref soft_linebreak_registry. When used with
	/// a \ref fold_region_skipper, this should be put in front of it so that soft linebreaks are checked first.
	struct soft_linebreak_inserter {
	public:
		/// Initializer.
		///
		/// \param reg The given \ref soft_linebreak_registry.
		/// \param sp The starting position. The end position is not required.
		soft_linebreak_inserter(const soft_linebreak_registry &reg, size_t sp) {
			auto pos = reg.get_softbreak_before_or_at_char(sp);
			_next = pos.entry;
			_ncs = pos.prev_chars;
		}

		/// Checks if the a soft linebreak is at the current position and if so, inserts one into the rendered text.
		/// This function always returns \p false as there can only be one soft linebreak at a position.
		bool check(character_rendering_iterator &it) {
			if (_next != _next.get_container()->end() && it.current_position() >= _next->length + _ncs) {
				it.insert_soft_linebreak();
				_ncs += _next->length;
				++_next;
				return true;
			}
			return false;
		}

		/// Checks if a soft linebreak is at the current position. This is not guaranteed to be correct during a call
		/// to \p rendering_iterator::begin or \p rendering_iterator::next.
		bool is_soft_linebreak(character_rendering_iterator &it) const {
			return it.current_position() != 0 && it.current_position() == _ncs;
		}
		/// Checks whether there's a linebreak at the current position.
		bool is_linebreak(character_rendering_iterator &it) const {
			return it.is_hard_line_break() || is_soft_linebreak(it);
		}
	protected:
		soft_linebreak_registry::iterator _next; ///< Iterator to the next soft linebreak.
		/// The number of characters before \ref _next, not including
		/// \ref soft_linebreak_registry::node_data::length.
		size_t _ncs;
	};
	/// Integrates both a \ref soft_linebreak_inserter and a \ref fold_region_skipper.
	struct view_formatter {
	public:
		/// Initializer.
		///
		/// \param fmt A \ref view_formatting, of which the \ref soft_linebreak_registry is used to initialize the
		///            \ref soft_linebreak_inserter, and the \ref folding_registry is used to initialize the
		///            \ref fold_region_skipper.
		/// \param sp The starting position.
		view_formatter(const view_formatting &fmt, size_t sp) :
			_lb(fmt.get_linebreaks(), sp), _fold(fmt.get_folding(), sp) {
		}

		/// Calls \ref soft_linebreak_inserter::check, then returns the result of \ref fold_region_skipper::check.
		bool check(character_rendering_iterator &it) {
			_lb.check(it);
			return _fold.check(it);
		}

		/// Returns the underlying \ref fold_region_skipper.
		const fold_region_skipper &get_fold_region_skipper() const {
			return _fold;
		}
		/// Returns the underlying \ref soft_linebreak_inserter.
		const soft_linebreak_inserter &get_soft_linebreak_inserter() const {
			return _lb;
		}
	protected:
		soft_linebreak_inserter _lb; ///< The underlying \ref soft_linebreak_inserter.
		fold_region_skipper _fold; ///< The underlying \ref fold_region_skipper.
	};

	/// An addon to \ref rendering_iterator for rendering carets and selected regions. Note that to obtain correct
	/// results one has to call \ref finish after iterating over the characters and before using the results of
	/// \ref get_caret_rects or \ref get_selection_rects.
	struct caret_renderer {
	public:
		/// Constructor.
		///
		/// \param c The set of carets.
		/// \param sp The starting position.
		/// \param slb Indicates whether the first line starts with a softbreak, i.e., whether a soft linebreak
		///            is at the same position as \p sp.
		caret_renderer(const caret_set::container &c, size_t sp, bool slb) : _end(c.end()), _last_softlb(slb) {
			// find first caret region
			_cur = c.lower_bound(std::make_pair(sp, 0));
			if (_cur != c.begin()) {
				auto prev = _cur;
				--prev;
				if (prev->first.second > sp) {
					_cur = prev;
				}
			}
			// initialize other fields
			_next = _cur;
			if (_cur != _end) {
				++_next;
				_range = std::minmax(_cur->first.first, _cur->first.second);
				if (_range.first < sp) {
					_insel = true;
				}
			}
		}

		/// Calls \ref _on_switching_char to process the change of position.
		void switching_char(const character_rendering_iterator &it, switch_char_info&) {
			_on_switching_char(it);
		}
		/// If caused by a soft linebreak, checks and inserts carets; otherwise, calls \ref _on_switching_char.
		/// This function also splits selected regions.
		void switching_line(const character_rendering_iterator &it, switch_line_info &info) {
			if (info.type == line_ending::none) { // soft linebreak, the actual position is not changed
				if (_cur != _end) {
					if (_cur->first.first == it.current_position() && !_cur->second.softbreak_next_line) {
						_add_caret(it, true);
					}
				}
				if (_next != _end) {
					if (_next->first.first == it.current_position() && !_next->second.softbreak_next_line) {
						_add_caret(it, true);
					}
				}
				_last_softlb = true;
			} else {
				_on_switching_char(it);
			}
			if (_insel) { // split selected regions
				double x = it.character_info().char_left();
				if (info.type != line_ending::none) { // add width for hard linebreak
					x += editor::get_font().get_by_style(
						it.theme_info().current_theme.style
					)->get_char_entry(U' ').advance;
				}
				_curselrects.emplace_back(_line_selection_start, x, it.y_offset(), it.y_offset() + it.line_height());
				_line_selection_start = 0.0;
			}
		}

		/// Called after having finished iterating over all characters to cut off the selected regions and append
		/// carets.
		void finish(const character_rendering_iterator &it) {
			_on_switching_char(it); // in case a caret is at the end of the document
			if (_insel) {
				_end_selected_region(it);
			}
		}

		/// Returns the boundaries of the character that each caret is at.
		const std::vector<rectd> &get_caret_rects() const {
			return _caretrects;
		}
		/// Returns the boundaries of selected regions. Since a selected region can be made up of multiple
		/// rectangles, rectangles for each selected region is put in a separate list.
		const std::vector<std::vector<rectd>> &get_selection_rects() const {
			return _selrects;
		}
	protected:
		std::vector<std::vector<rectd>> _selrects; ///< The boundaries of selected regions.
		std::vector<rectd>
			_caretrects, ///< The boundaries of the character that each caret is at.
			_curselrects; ///< Boundaries of the current selected region.
		caret_set::const_iterator
			_cur, ///< Iterator to the (about-to-be) current \ref caret_set::entry.
			/// Iterator to the next \ref caret_set::entry. This is used to render carets correctly at soft
			/// linebreaks.
			_next,
			_end; ///< Iterator past the last \ref caret_set::entry.
		/// The selected range of the current caret. Unlike \ref caret_selection, \p first is the start of the range, while
		/// \p second is the end of the range.
		std::pair<size_t, size_t> _range;
		double _line_selection_start = 0.0; ///< The starting position of the selected region for this line.
		bool
			_last_softlb = false, ///< Indicates whether the last operations was a soft linebreak.
			_insel = false; ///< Indicates whether the current position is in a selected region.

		/// Adds a caret to \ref _caretrects.
		///
		/// \param it The iterator.
		/// \param softbreak Indicates whether the caret is at the position of a soft linebreak but not on the next line.
		void _add_caret(const character_rendering_iterator &it, bool softbreak) {
			if (softbreak || it.is_hard_line_break()) { // end of the line, use the width of the space (' ') character
				_caretrects.emplace_back(rectd::from_xywh(
					it.character_info().char_left(), it.y_offset(),
					editor::get_font().normal->get_char_entry(' ').advance, it.line_height()
				));
			} else {
				_caretrects.emplace_back(
					it.character_info().char_left(), it.character_info().char_right(),
					it.y_offset(), it.y_offset() + it.line_height()
				);
			}
		}
		/// Ends the current selected region and moves on to the next one without any checking.
		void _end_selected_region(const character_rendering_iterator &it) {
			if (_cur->first.first == it.current_position()) { // check & add caret at end
				if (!_last_softlb || _cur->second.softbreak_next_line) {
					_add_caret(it, false);
				}
			}
			// add selection rect
			_curselrects.emplace_back(
				_line_selection_start, it.character_info().char_left(),
				it.y_offset(), it.y_offset() + it.line_height()
			);
			_selrects.emplace_back(std::move(_curselrects));
			_curselrects = std::vector<rectd>();
			// move to the next region
			_cur = _next;
			if (_next != _end) {
				++_next;
				_range = std::minmax(_cur->first.first, _cur->first.second);
			}
			_insel = false;
		}
		/// Called when the position that the iterator is at is changing. Checks for the beginning and ending of selected
		/// regions.
		void _on_switching_char(const character_rendering_iterator &it) {
			while (true) {
				if (_insel) {
					if (it.current_position() >= _range.second) { // end current region
						_end_selected_region(it);
					} else {
						break;
					}
				} else {
					if (_cur != _end && it.current_position() >= _range.first) { // start region
						if (it.current_position() == _cur->first.first) { // check & add caret
							// this check is to ensure that duplicate carets are not added for carets without
							// selected regions, since _end_selected_region will be called immediately after
							if (_range.first != _range.second) {
								if (!_last_softlb || _cur->second.softbreak_next_line) {
									_add_caret(it, false);
								}
							}
						}
						_line_selection_start = it.character_info().char_left();
						_insel = true;
					} else {
						break;
					}
				}
			}
			_last_softlb = false;
		}
	};
}
