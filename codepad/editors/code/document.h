#pragma once

/// \file
/// Definiton of file contexts and related classes.

#include <cstddef>
#include <variant>
#include <any>

#include "../../core/event.h"
#include "../../os/font.h"
#include "../../os/filesystem.h"
#include "buffer.h"
#include "buffer_formatting.h"

namespace codepad::editor::code {
	class view_formatting;
	class editor;

	/// A caret and the associated selected region. The first element is the position of the caret,
	/// and the second indicates the other end of the selcted region.
	using caret_selection = std::pair<size_t, size_t>;

	/// The data associated with a \ref caret_selection.
	struct caret_data {
		/// Default constructor.
		caret_data() = default;
		/// Constructor that initializes all fields of this struct.
		caret_data(double align, bool sbnext) : alignment(align), softbreak_next_line(sbnext) {
		}
		double alignment = 0.0; ///< The alignment of the caret when it moves vertically.
		/// Only used when the caret is positioned at a soft linebreak, to determine which line it's on.
		/// \p false if it's on the former line, \p true if it's on the latter.
		bool softbreak_next_line = false;
	};
	/// Stores a set of carets.
	struct caret_set {
		/// The container used to store carets.
		using container = std::map<caret_selection, caret_data>;
		/// An entry in the container, stores a caret and its associated data.
		using entry = std::pair<caret_selection, caret_data>;
		/// Iterator over the carets.
		using iterator = container::iterator;
		/// Const iterator over the carets.
		using const_iterator = container::const_iterator;

		container carets; ///< The carets.

		/// Calls \ref add_caret to add a caret to this set.
		///
		/// \param p The new caret to be added. The caret may be merged with overlapping carets.
		/// \param merged Indicates whether any merging has taken place.
		/// \return An iterator to the added entry.
		container::iterator add(entry p, bool &merged) {
			return add_caret(carets, p, merged);
		}
		/// \overload
		container::iterator add(entry p) {
			return add_caret(carets, p);
		}

		/// Resets the contents of this caret set, leaving only one caret at the beginning of the buffer.
		void reset() {
			carets.clear();
			carets.insert(entry(caret_selection(0, 0), caret_data()));
		}

		/// Adds a caret to the given container, merging it with existing ones when necessary.
		///
		/// \param cont The container.
		/// \param et The caret to be added to the container.
		/// \param merged Indicates whether the caret has been merged with existing ones.
		/// \return An iterator of the container to the inserted caret.
		static container::iterator add_caret(container &cont, entry et, bool &merged);
		/// \overload
		inline static container::iterator add_caret(container &cont, entry et) {
			bool dummy;
			return add_caret(cont, et, dummy);
		}
		/// Tries to merge two carets together. The discrimination between `master' and `slave' carets is
		/// introduced to resolve conflicting caret placement relative to the selection.
		///
		/// \param mc The caret of the `master' \ref caret_selection.
		/// \param ms End of the selected region of the `master' \ref caret_selection.
		/// \param sc The caret of the `slave' \ref caret_selection.
		/// \param ss End of the selected region of the `slave' \ref caret_selection.
		/// \param rc The caret of the resulting \ref caret_selection.
		/// \param rs End of the selected region of the resulting \ref caret_selection.
		/// \return Whether the two carets should be merged.
		static bool try_merge_selection(size_t mc, size_t ms, size_t sc, size_t ss, size_t &rc, size_t &rs);
	};

	/// The positional information of a modification.
	///
	/// \sa modification
	struct modification_position {
		/// Default constructor.
		modification_position() = default;
		/// Initializes all field of this struct.
		modification_position(size_t p, size_t rem, size_t add) :
			removed_range(rem), added_range(add), position(p) {
		}

		size_t
			removed_range = 0, ///< The length of the removed text, in characters.
			added_range = 0, ///< The length of the added text, in characters.
			/// The position where the modification takes place. If multiple modifications are made simultaneously
			/// by multiple carets, this position is obtained after all previous modifications have completed.
			position = 0;
	};

	/// A single modification made to the text by a single caret. A short clip of text (optionally empty)
	/// starting from a certain position is removed, then another clip of text (also optionally empty) is
	/// inserted at the same position.
	struct modification {
		/// Default constructor.
		modification() = default;
		/// Initializes the positional information with that of the given \ref caret_selection.
		explicit modification(caret_selection sel) {
			selected_before = sel.first != sel.second;
			caret_front_before = sel.first < sel.second;
			if (caret_front_before) {
				position.position = sel.first;
				position.removed_range = sel.second - sel.first;
			} else {
				position.position = sel.second;
				position.removed_range = sel.first - sel.second;
			}
		}

		string_buffer::string_type
			removed_content, ///< The text removed by this modification.
			added_content; ///< The text inserted by this modification.
		modification_position position;
		bool
			/// Whether the caret was at the front of the selected region before the modification.
			caret_front_before = false,
			selected_before = false, ///< Whether the removed text was selected before the modification.
			/// Whether the caret is at the front of the selected region after the modification.
			caret_front_after = false,
			selected_after = false; ///< Whether the added text is selected after the modification.
	};
	/// A list of modifications made by multiple carets at the same time.
	using edit = std::vector<modification>;

	/// Information used to adjust the positions of carets or other objects during or after a modification.
	struct caret_fixup_info {
	public:
		/// Struct used to keep track of the process of adjusting positions.
		struct context {
			friend struct caret_fixup_info;
		public:
			/// Default constructor.
			context() = default;
			/// Initializes the context using the given \ref caret_fixup_info.
			explicit context(const caret_fixup_info &src) : _next(src.mods.begin()) {
			}

			/// Adds a custom offset, specified by \p mpos, to \ref _diff.
			void append_custom_modification(modification_position mpos) {
				_diff += mpos.added_range - mpos.removed_range;
			}
			/// Adjusts the given position with the offset so far, without updating the context.
			/// Therefore, the result will only be correct if no modification lies between the
			/// position and the last modification recorded by \ref _next (which actually points
			/// to the modification after the last recorded one).
			size_t fix(size_t pos) const {
				return pos + _diff;
			}
		protected:
			/// An iterator to the position of the next modification.
			std::vector<modification_position>::const_iterator _next;
			/// The offset used to adjust positons so far. Note that this value may overflow
			/// if more chars are removed than added, but the final result will still be correct.
			size_t _diff = 0;
		};

		/// Default constructor.
		caret_fixup_info() = default;
		/// Initializes the struct with the given list of modifications.
		explicit caret_fixup_info(const edit &e) {
			mods.reserve(e.size());
			for (const modification &mod : e) {
				mods.push_back(mod.position);
			}
		}

		/// Adjusts the given position, moving it to the minimum valid position.
		/// For the same \ref context, this function should be called only with increasing positions.
		size_t fixup_position_min(size_t cp, context &ctx) const {
			cp = fixup_position(cp, ctx);
			for (auto it = ctx._next; ; --it) {
				if (it->position + it->added_range < cp) {
					break;
				}
				cp = it->position;
				if (it == mods.begin()) {
					break;
				}
			}
			return cp;
		}
		/// Adjusts the given position, moving it to the maximum valid position.
		/// For the same \ref context, this function should be called only with increasing positions.
		size_t fixup_position_max(size_t cp, context &ctx) const {
			cp = fixup_position(cp, ctx);
			for (auto it = ctx._next; it != mods.end(); ++it) {
				if (it->position > cp) {
					break;
				}
				cp = it->position + it->added_range;
			}
			return cp;
		}
		/// Adjusts the given position, while trying to keep the result as close to the original as possible.
		/// For the same \ref context, this function should be called only with increasing positions.
		size_t fixup_position(size_t cp, context &ctx) const {
			for (
				cp = ctx.fix(cp);
				ctx._next != mods.end() && ctx._next->position < cp;
				++ctx._next
				) {
				if (cp <= ctx._next->position + ctx._next->removed_range) {
					return std::min(cp, ctx._next->position + ctx._next->added_range);
				}
				cp = fixup_caret_with_mod(cp, *ctx._next);
				ctx.append_custom_modification(*ctx._next);
			}
			return cp;
		}
		/// Adjusts the given positions, which is assumed to be after the modification, with the
		/// offset in the \ref modification_position.
		inline static size_t fixup_caret_with_mod(size_t cp, const modification_position &mod) {
			return cp + mod.added_range - mod.removed_range;
		}

		/// Records a list of \ref modification_position "modification_positions" corresponding to an \ref edit.
		std::vector<modification_position> mods;
	};

	/// The type of a parameter of the text's theme.
	enum class text_theme_parameter {
		style, ///< The `style' parameter, corresponding to \ref font_style.
		color ///< The `color' parameter.
	};
	/// Specifies the theme of the text.
	struct text_theme_specification {
		/// Default constructor.
		text_theme_specification() = default;
		/// Initializes the struct with the given parameters.
		text_theme_specification(font_style fs, colord c) : style(fs), color(c) {
		}
		font_style style = font_style::normal; ///< The style of the font.
		colord color; ///< The color of the text.
	};
	/// Records a parameter of the theme of the entire buffer. Internally, it keeps a list of
	/// (position, value) pairs, and characters will use the last value specified before it.
	template <typename T> class text_theme_parameter_info {
	public:
		/// Iterator.
		using iterator = typename std::map<size_t, T>::iterator;
		/// Const iterator.
		using const_iterator = typename std::map<size_t, T>::const_iterator;

		/// Default constructor. Adds a default-initialized value to position 0.
		text_theme_parameter_info() : text_theme_parameter_info(T()) {
		}
		/// Constructor that adds the given value to position 0.
		explicit text_theme_parameter_info(T def) {
			_changes.emplace(0, std::move(def));
		}

		/// Clears the parameter of the theme, and adds the given value to position 0.
		void clear(T def) {
			_changes.clear();
			_changes[0] = def;
		}
		/// Sets the parameter of the given range to the given value.
		void set_range(size_t s, size_t pe, T c) {
			assert_true_usage(s < pe, "invalid range");
			auto beg = get_iter_at(s), end = get_iter_at(pe);
			T begv = beg->second, endv = end->second;
			_changes.erase(++beg, ++end);
			if (begv != c) {
				_changes[s] = c;
			}
			if (endv != c) {
				_changes[pe] = endv;
			}
		}
		/// Retrieves the value of the parameter at the given position.
		T get_at(size_t cp) const {
			return get_iter_at(cp)->second;
		}

		/// Returns an iterator to the first pair.
		iterator begin() {
			return _changes.begin();
		}
		/// Returns an iterator past the last pair.
		iterator end() {
			return _changes.end();
		}
		/// Returns an iterator to the pair that determines the parameter at the given position.
		iterator get_iter_at(size_t cp) {
			return --_changes.upper_bound(cp);
		}
		/// Const version of \ref begin().
		const_iterator begin() const {
			return _changes.begin();
		}
		/// Const version of \ref end().
		const_iterator end() const {
			return _changes.end();
		}
		/// Const version of \ref get_iter_at(size_t).
		const_iterator get_iter_at(size_t cp) const {
			return --_changes.upper_bound(cp);
		}

		/// Returns the number of position-value pairs in this parameter.
		size_t size() const {
			return _changes.size();
		}
	protected:
		std::map<size_t, T> _changes; ///< The underlying \p std::map that stores the position-value pairs.
	};
	/// Records the text's theme across the entire buffer.
	struct text_theme_data {
		text_theme_parameter_info<font_style> style; ///< Redords the text's style across the ehtire buffer.
		text_theme_parameter_info<colord> color; ///< Redords the text's color across the ehtire buffer.

		/// An iterator used to obtain the theme of the text at a certain position.
		struct char_iterator {
			text_theme_specification current_theme; ///< The current theme of the text.
			/// The iterator to the next position-style pair.
			text_theme_parameter_info<font_style>::const_iterator next_style_iterator;
			/// The iterator to the next position-color pair.
			text_theme_parameter_info<colord>::const_iterator next_color_iterator;
		};

		/// Sets the theme of the text in the given range.
		void set_range(size_t s, size_t pe, text_theme_specification tc) {
			color.set_range(s, pe, tc.color);
			style.set_range(s, pe, tc.style);
		}
		/// Returns the theme of the text at the given position.
		text_theme_specification get_at(size_t p) const {
			return text_theme_specification(style.get_at(p), color.get_at(p));
		}
		/// Sets the theme of all text to the given value.
		void clear(const text_theme_specification &def) {
			style.clear(def.style);
			color.clear(def.color);
		}

		/// Returns a \ref char_iterator specifying the text theme at the given position.
		char_iterator get_iter_at(size_t p) const {
			char_iterator rv;
			rv.next_style_iterator = style.get_iter_at(p);
			rv.next_color_iterator = color.get_iter_at(p);
			assert_true_logical(rv.next_style_iterator != style.end(), "empty theme parameter info encountered");
			assert_true_logical(rv.next_color_iterator != color.end(), "empty theme parameter info encountered");
			rv.current_theme = text_theme_specification(rv.next_style_iterator->second, rv.next_color_iterator->second);
			++rv.next_style_iterator;
			++rv.next_color_iterator;
			return rv;
		}
	protected:
		/// Used when incrementing a \ref char_iterator, to check whether a
		/// \ref text_theme_parameter_info::const_iterator needs incrementing,
		/// and to increment it if necessary.
		///
		/// \param cp The new position in the text.
		/// \param it The iterator.
		/// \param fullset The set of position-value pairs.
		/// \param fval The value at the position.
		template <typename T> inline static void _incr_iter_elem(
			size_t cp,
			typename text_theme_parameter_info<T>::const_iterator &it,
			const text_theme_parameter_info<T> &fullset,
			T &fval
		) {
			if (it != fullset.end() && it->first <= cp) {
				fval = it->second;
				++it;
			}
		}
	public:
		/// Moves the given \ref char_iterator to the given position. The position must be immediately after
		/// where the \ref char_iterator was originally at.
		void incr_iter(char_iterator &cv, size_t cp) const {
			_incr_iter_elem(cp, cv.next_color_iterator, color, cv.current_theme.color);
			_incr_iter_elem(cp, cv.next_style_iterator, style, cv.current_theme.style);
		}
	};

	/// Contains information about the modification of a \ref document.
	struct modification_info {
		/// Initializes all fields of this struct.
		modification_info(
			editor *e, caret_fixup_info fi, std::vector<linebreak_registry::text_clip_info> removed_clips
		) : source(e), caret_fixup(std::move(fi)), removed_clips_info(std::move(removed_clips)) {
		}
		/// The \ref editor through which the user makes the modification,
		/// or \p nullptr if the modification is made externally.
		editor *const source;
		/// Used to adjust the positions of carets.
		const caret_fixup_info caret_fixup;
		/// Structure of removed text clips.
		const std::vector<linebreak_registry::text_clip_info> removed_clips_info;
	};

	/// Stores the contents and theme of a text buffer.
	///
	/// \todo Better encoding support. Also consider storing the file in its original encoding.
	class document {
		friend class document_manager;
	public:
		/// The platform-specific preferred line ending.
		///
		/// \todo Convert this into a setting.
#ifdef CP_PLATFORM_WINDOWS
		constexpr static line_ending platform_line_ending = line_ending::rn;
#elif defined(CP_PLATFORM_UNIX)
		constexpr static line_ending platform_line_ending = line_ending::n;
#endif

		/// Auxiliary struct used to set the encoding used to load a file in the constructor.
		template <typename Encoding> struct encoding_tag_t {
			using encoding = Encoding; ///< The encoding.
		};

		/// Initializes a new buffer with the given ID. Users should <em>not</em> call this function
		/// directly; instead, they should obtain \ref document "text_contexts" from the \ref document_manager.
		explicit document(size_t id) : _fileid(std::in_place_type<size_t>, id) {
		}
		/// Initializes a new buffer by loading the file specified by the given path, with the given encoding.
		/// Users should <em>not</em> call this function directly; instead, they should obtain
		/// \ref document "text_contexts" from the \ref document_manager.
		///
		/// \todo Error handling when file loading has failed.
		template <typename Encoding> document(
			const std::filesystem::path &fn, encoding_tag_t<Encoding>
		) : _fileid(std::in_place_type<std::filesystem::path>, fn) {
			using _iter_t = const typename Encoding::char_type*;

			performance_monitor monitor(CP_STRLIT("load file"), 0.1);
			os::file fil(fn, os::access_rights::read, os::open_mode::open);
			if (fil.valid()) {
				os::file_mapping mapping(fil, os::access_rights::read);
				if (mapping.valid()) {
					auto cs = static_cast<const std::byte*>(mapping.get_mapped_pointer());
					insert_text<_iter_t, Encoding>(
						0, reinterpret_cast<_iter_t>(cs),
						reinterpret_cast<_iter_t>(cs + fil.get_size())
						);
					set_default_line_ending(detect_most_used_line_ending());
					return;
				}
			}
			logger::get().log_warning(CP_HERE, "file loading failed: ", fn);
		}
		/// Notifies the \ref document_manager of the disposal of this context, and clears all entries of \ref _tags.
		~document();

		/// Used to iterate through the characters in the context.
		struct iterator {
			friend class document;
		public:
			/// Default constructor.
			iterator() = default;

			/// Returns the current character that this iterator points to.
			/// If the iterator points to a linebreak, it will return the first codepoint of the linebreak.
			///
			/// \sa current_linebreak()
			char32_t current_character() const {
				return _cit.current_codepoint();
			}
			/// Returns the type of the current line's linebreak.
			line_ending current_linebreak() const {
				return _lbit->ending;
			}
			/// Returns whether the current codepoint is in valid format.
			///
			/// \sa string_buffer::codepoint_iterator::current_good()
			bool current_good() const {
				return _cit.current_good();
			}

			/// Pre-increment. Moves the iterator to point to the next character.
			iterator &operator++() {
				if (is_linebreak()) {
					for (size_t i = get_linebreak_length(_lbit->ending); i > 0; --i) {
						++_cit;
					}
					++_lbit;
					_col = 0;
				} else {
					++_cit;
					++_col;
				}
				return *this;
			}
			/// Post-increment.
			const iterator operator++(int) {
				iterator oldv = *this;
				++*this;
				return oldv;
			}

			/// Returns the column that the iterator is at.
			size_t get_column() const {
				return _col;
			}
			/// Returns the number of characters on the current line, excluding the linebreak.
			size_t get_line_length() const {
				return _lbit->nonbreak_chars;
			}

			/// Returns whether the iterator is currently at a linebreak. Note that EOF is also treated
			/// as a linebreak.
			bool is_linebreak() const {
				return _col == _lbit->nonbreak_chars;
			}
			/// Returns whether the iterator is at the end of the text context.
			bool is_end() const {
				return _cit.is_end();
			}
		protected:
			/// Protected constructor that \ref document uses to create an iterator pointing to the given
			/// position. If \p lb is \ref linebreak_registry::end(), it will be automatically moved forward
			/// to the last element.
			iterator(string_buffer::codepoint_iterator c, linebreak_registry::iterator lb, size_t col) :
				_cit(c), _lbit(lb), _col(col) {
				if (_lbit == _lbit.get_container()->end() && _lbit != _lbit.get_container()->begin()) {
					// lb is past the end, move it back
					--_lbit;
					_col = _lbit->nonbreak_chars;
				}
			}

			string_buffer::codepoint_iterator _cit; ///< Iterator to the codepoint.
			/// Iterator to the current line. For iterators created by \ref document "text_contexts",
			/// this will never be \ref linebreak_registry::end(), even if \ref is_end() is true.
			linebreak_registry::iterator _lbit;
			size_t _col = 0; ///< The column this iterator is currently at.
		};

		/// Saves this text context to the associated file, regardless of whether this has changed.
		/// This file context must have been associated with a file.
		void save() {
			assert_true_usage(std::holds_alternative<std::filesystem::path>(_fileid), "file name unknown");
			save_to_file(std::get<std::filesystem::path>(_fileid));
		}
		/// Associates the given file name with this file context and saves this file context to it.
		/// This file context must not have been previously associated with a file.
		void save_new(const std::filesystem::path&);
		/// Saves the contents of this file context to the given file.
		void save_to_file(const std::filesystem::path &fn) const {
			std::ofstream fout(fn.u8string(), std::ios::binary);
			for (auto i = _str.node_begin(); i != _str.node_end(); ++i) {
				fout.write(
					reinterpret_cast<const char*>(i->data()),
					i->length() * sizeof(string_buffer::char_type)
				);
			}
		}

		/// Returns the \ref line_ending that's most used in the text.
		line_ending detect_most_used_line_ending() const {
			size_t n[3]{};
			for (auto &line : _lbr) {
				if (line.ending != line_ending::none) {
					++n[static_cast<int>(line.ending) - 1];
				}
			}
			return static_cast<line_ending>((std::max_element(n, n + 3) - n) + 1);
		}
		/// Sets the default line ending used by this text context, which is used whenever a
		/// new line is inserted into the text.
		void set_default_line_ending(line_ending l) {
			_le = l;
		}
		/// Returns the default line ending currently in use.
		line_ending get_default_line_ending() const {
			return _le;
		}

		/// Sets the width of a tab character. The width is given with respect to the width of a space character,
		/// i.e., 4.0 means that a tab character has the same width of 4.0 spaces at most.
		void set_tab_width(double v) {
			_tab_w = v;
			visual_changed.invoke();
		}
		/// Returns the width of a tab character.
		double get_tab_width() const {
			return _tab_w;
		}

		/// Returns an iterator to the character at the given position.
		iterator at_char(size_t pos) const {
			auto pr = _lbr.get_line_and_column_and_codepoint_of_char(pos);
			return iterator(_str.at_codepoint_iterator(pr.second), pr.first.line_iterator, pr.first.column);
		}

		/// Returns the number of lines in the text context.
		size_t num_lines() const {
			return _lbr.num_linebreaks() + 1;
		}
		/// Returns the number of characters in the text context.
		size_t num_chars() const {
			return _lbr.num_chars();
		}

		/// Returns a substring of the text. The positions are specified in characters.
		///
		/// \param beg The position of the substring's first character.
		/// \param end The position past the substring's last character.
		string_buffer::string_type substring(size_t beg, size_t end) const {
			return _str.substring(
				_str.at_codepoint_iterator(_lbr.position_char_to_codepoint(beg)),
				_str.at_codepoint_iterator(_lbr.position_char_to_codepoint(end))
			);
		}

		/// Inserts a short clip of text at the given position. This function does not invoke \ref modified,
		/// and it doesn't record this modification in \ref _edithist.
		///
		/// \return A \ref linebreak_registry::text_clip_info containing information about the text that has been
		///         inserted.
		template <typename Iter, typename Encoding> linebreak_registry::text_clip_info insert_text(
			size_t cp, Iter beg, Iter end
		) {
			codepoint_iterator_base<Iter, Encoding> it(beg, end);
			auto pos = _lbr.get_line_and_column_and_codepoint_of_char(cp);
			char32_t last = '\0'; // the last codepoint
			linebreak_registry::text_clip_info clip_stats;
			linebreak_registry::line_info curl;
			_str.insert(_str.at_codepoint_iterator(pos.second), [&](char32_t &c) {
				if (it.at_end()) { // no more chars
					return false;
				}
				c = it.current_codepoint();
				if (c == '\n' || last == '\r') { // linebreak detected
					if (c == '\n') { // either \r\n or \n
						curl.ending = last == '\r' ? line_ending::rn : line_ending::n;
					} else { // \r, but at previous char
						curl.ending = line_ending::r;
					}
					clip_stats.append_line(curl);
					curl = linebreak_registry::line_info();
				}
				if (c != '\r' && c != '\n') { // non-break character
					++curl.nonbreak_chars;
				}
				last = c;
				++it;
				return true;
				});
			if (last == '\r') { // the last character is \r, which was not immediately handled
				curl.ending = line_ending::r;
				clip_stats.append_line(curl);
				curl = linebreak_registry::line_info();
			}
			clip_stats.append_line(curl);
			_lbr.insert_chars(pos.first.line_iterator, pos.first.column, clip_stats);
			return clip_stats;
		}
		/// \overload
		linebreak_registry::text_clip_info insert_text(size_t cp, const string_buffer::string_type &str) {
			return insert_text<
				string_buffer::string_type::const_iterator, string_buffer::encoding_data_t
			>(cp, str.begin(), str.end());
		}

		/// Erases a short clip of text at the given position. This function does not invoke \ref modified,
		/// and it doesn't record this modification in \ref _edithist.
		///
		/// \return A \ref linebreak_registry::text_clip_info containing information about the text that has been
		///         inserted.
		linebreak_registry::text_clip_info delete_text(size_t p1, size_t p2) {
			auto
				p1i = _lbr.get_line_and_column_and_codepoint_of_char(p1),
				p2i = _lbr.get_line_and_column_and_codepoint_of_char(p2);
			_str.erase(
				_str.at_codepoint_iterator(p1i.second),
				_str.at_codepoint_iterator(p2i.second)
			);
			return _lbr.erase_chars(
				p1i.first.line_iterator, p1i.first.column, p2i.first.line_iterator, p2i.first.column
			);
		}

		/// Creates a \ref view_formatting associated with this text context and returns it.
		view_formatting create_view_formatting();

		/// Returns the \ref text_theme_data of the text.
		const text_theme_data &get_text_theme() const {
			return _theme;
		}
		/// Sets the theme of the text.
		void set_text_theme(text_theme_data td) {
			_theme = std::move(td);
			visual_changed.invoke();
		}

		/// Returns the underlying \ref string_buffer.
		const string_buffer &get_string_buffer() const {
			return _str;
		}
		/// Returns the underlying \ref linebreak_registry.
		const linebreak_registry &get_linebreak_registry() const {
			return _lbr;
		}

		/// Returns whether there are operations available to undo.
		bool can_undo() const {
			return _curedit > 0;
		}
		/// Returns whether there are operations available to redo.
		bool can_redo() const {
			return _curedit < _edithist.size();
		}
		/// Undoes the last edit and sets the carets accordingly.
		/// Must only be called when \ref can_undo() returns \p true.
		///
		/// \param source The \ref editor that initiated this action.
		void undo(editor *source);
		/// Redoes the last reverted edit and sets the carets accordingly.
		/// Must only be called when \ref can_undo() returns \p true.
		///
		/// \param source The \ref editor that initiated this action.
		void redo(editor *source);
		/// Records the given \ref edit as the last edit action. All edits that have previously been
		/// reverted with \ref undo(editor*) will be lost.
		void append_edit_data(edit e) {
			if (_curedit == _edithist.size()) {
				_edithist.push_back(std::move(e));
			} else {
				_edithist[_curedit] = std::move(e);
				_edithist.erase(_edithist.begin() + _curedit + 1, _edithist.end());
			}
			++_curedit;
		}
		/// Returns the edit history, including actions that have been reverted with \ref undo(editor*).
		const std::vector<edit> &get_edits() const {
			return _edithist;
		}
		/// Returns the index past the last edit that has been made and hasn't been undone.
		size_t get_current_edit_index() const {
			return _curedit;
		}

		/// Returns the data used to identify this file context. If it holds a \p size_t, then this file context
		/// has yet been saved to a file; otherwise, it holds the path to the file.
		const std::variant<size_t, std::filesystem::path> &get_file_id() const {
			return _fileid;
		}

		/// Returns the tag associated with the given index.
		///
		/// \sa document_manager::allocate_tag
		std::any &get_tag(size_t index) const {
			return _tags.at(index);
		}

		/// Invoked when the visual of the text context has been changed without any modification to the text,
		/// i.e., when tab size or text theme has been changed.
		event<void> visual_changed;
		/// Invoked when the text has been modified. \ref document doesn't invoke this event itself; this is
		/// only invoked by \ref document_modifier.
		event<modification_info> modified;
	protected:
		string_buffer _str; ///< Stores the contents of the text context.
		linebreak_registry _lbr; ///< Stores information about the lines of the text context.
		text_theme_data _theme; ///< The theme of the text, shared among all open views.
		std::vector<edit> _edithist; ///< The edit history.

		/// Additional data specific to each document used by other components, plugins, etc.
		mutable std::vector<std::any> _tags;
		/// Used to identify the file context. Also stores the path to the file associated with this context.
		std::variant<size_t, std::filesystem::path> _fileid;

		double _tab_w = 4.0; ///< Tab width.
		size_t _curedit = 0; ///< The index past the last edit that has been made and hasn't been undone.
		line_ending _le = platform_line_ending; ///< The default line ending.
	};

	/// Used to modify a \ref document at multiple different locations. The modifications must be made
	/// in increasing order of their positions.
	struct document_modifier {
	public:
		/// Creates a modifier for the given \ref document.
		explicit document_modifier(document &doc) : _doc(&doc) {
		}

		/// Erases the text starting from \ref modification_position::position, with length
		/// \ref modification_position::removed_range, then inserts \ref modification::added_content at
		/// \ref modification_position::position and adds a new caret calculated from
		/// \ref modification::caret_front_after and \ref modification::selected_after to \ref _newcarets.
		/// Finally, this function records the modification in \ref _edit. This function stores the removed
		/// content in \ref modification::removed_content, and the length of the inserted text, in
		/// characters, in \ref modification_position::added_range.
		void apply_modification_nofixup(modification mod) {
			if (mod.position.removed_range != 0) {
				mod.removed_content = _doc->substring(
					mod.position.position, mod.position.position + mod.position.removed_range
				);
				_removedclips.emplace_back(_doc->delete_text(
					mod.position.position, mod.position.position + mod.position.removed_range
				));
			} else {
				_removedclips.emplace_back();
			}
			if (mod.added_content.length() > 0) {
				mod.position.added_range = _doc->insert_text(mod.position.position, mod.added_content).total_chars;
			}
			_append_fixup_item(mod.position);
			_append_caret(get_caret_selection_after(mod));
			_edit.push_back(std::move(mod));
		}
		/// Calls \ref fixup_caret_position to adjust the caret positions based on modifications made previously by
		/// this modifier, then calls \ref apply_modification_nofixup to apply the given modification.
		///
		/// \remark Should the caller need to use (alter, save, etc.) the position of the caret or inserted/deleted
		///         ranges before applying the modification, call \ref fixup_caret_position() before using them,
		///         them call \ref apply_modification_nofixup instead.
		void apply_modification(modification mod) {
			fixup_caret_position(mod);
			apply_modification_nofixup(std::move(mod));
		}

		/// Reverts a modification made previously. This operation is not recorded in \ref _edit.
		void undo_modification(const modification &mod) {
			size_t
				pos = fixup_caret_position(mod.position.position),
				addend = fixup_caret_position(mod.position.position + mod.position.added_range),
				delend = fixup_caret_position(mod.position.position + mod.position.removed_range);
			if (mod.added_content.length() > 0) {
				_removedclips.emplace_back(_doc->delete_text(pos, addend));
			} else {
				_removedclips.emplace_back();
			}
			if (mod.removed_content.length() > 0) {
				_doc->insert_text(pos, mod.removed_content);
			}
			_append_fixup_item(modification_position(pos, addend - pos, delend - pos));
			_append_caret(get_caret_selection(pos, delend - pos, mod.selected_before, mod.caret_front_before));
		}
		/// Restores a previously reverted modification. This operation is not recorded in \ref _edit.
		void redo_modification(const modification &mod) {
			// the modification already stores adjusted positions
			if (mod.removed_content.length() > 0) {
				_removedclips.emplace_back(_doc->delete_text(
					mod.position.position, mod.position.position + mod.position.removed_range
				));
			} else {
				_removedclips.emplace_back();
			}
			if (mod.added_content.length() > 0) {
				_doc->insert_text(mod.position.position, mod.added_content);
			}
			_append_fixup_item(mod.position);
			_append_caret(get_caret_selection_after(mod));
		}


		/// Returns the \ref caret_selection that should appear after the given modification has been made.
		inline static caret_selection get_caret_selection_after(const modification &mod) {
			return get_caret_selection(
				mod.position.position, mod.position.added_range, mod.selected_after, mod.caret_front_after
			);
		}
		/// Retrieves a \ref caret_selection corresponding to the given parameters.
		///
		/// \param pos The starting position of the text.
		/// \param diff The length of the text.
		/// \param selected Whether the text is selected.
		/// \param caretfront Whether the caret is at the front of the text.
		inline static caret_selection get_caret_selection(
			size_t pos, size_t diff, bool selected, bool caretfront
		) {
			caret_selection res = std::make_pair(pos, pos + diff);
			if (!caretfront) {
				std::swap(res.first, res.second);
			}
			if (!selected) {
				res.second = res.first;
			}
			return res;
		}

		/// Adjusts the given position according to previously accumulated offsets caused by modifications.
		size_t fixup_caret_position(size_t c) const {
			return _cfctx.fix(c);
		}
		/// Adjusts \ref modification_position::position and \ref modification_position::removed_range with
		/// \ref fixup_caret_position(size_t) const.
		void fixup_caret_position(modification &m) const {
			size_t rmend = fixup_caret_position(m.position.position + m.position.removed_range);
			m.position.position = fixup_caret_position(m.position.position);
			m.position.removed_range = rmend - m.position.position;
		}


		/// Performs the default modification that results from typing the given string at the given position
		/// in `insert' mode.
		void on_text_insert(caret_selection cs, string_buffer::string_type s) {
			modification mod(cs);
			mod.caret_front_after = false;
			mod.selected_after = false;
			mod.added_content = std::move(s);
			apply_modification(std::move(mod));
		}
		/// Performs the default modification that results from typing the given string at the given position
		/// in `overwrite' mode.
		void on_text_overwrite(caret_selection cs, string_buffer::string_type s) {
			modification mod(cs);
			fixup_caret_position(mod);
			if (!mod.selected_before) {
				document::iterator it = _doc->at_char(mod.position.position);
				size_t col = it.get_column();
				for (
					codepoint_iterator_base<string_buffer::string_type::const_iterator> cit(s.begin(), s.end());
					!cit.at_end();
					++cit
					) {
					if (!is_newline(cit.current_codepoint()) && col < it.get_line_length()) {
						++mod.position.removed_range;
					}
				}
				mod.caret_front_before = true;
			}
			mod.added_content = std::move(s);
			apply_modification_nofixup(std::move(mod));
		}
		/// Performs the default modification that results from typing the given string at the given position
		/// in the mode given by \p insert.
		void on_text(caret_selection cs, string_buffer::string_type s, bool insert) {
			if (insert) {
				on_text_insert(cs, std::move(s));
			} else {
				on_text_overwrite(cs, std::move(s));
			}
		}
		/// Performs the default modification that results from pressing the `backspace' key.
		void on_backspace(caret_selection cs) {
			modification mod(cs);
			fixup_caret_position(mod);
			if (!mod.selected_before && mod.position.position > 0) {
				// no selection; delete the character before the caret
				--mod.position.position;
				mod.position.removed_range = 1;
				mod.caret_front_before = false;
				mod.selected_before = false;
			}
			mod.caret_front_after = false;
			mod.selected_after = false;
			apply_modification_nofixup(std::move(mod));
		}
		/// Performs the default modification that results from pressing the `delete' key.
		void on_delete(caret_selection cs) {
			modification mod(cs);
			fixup_caret_position(mod);
			if (!mod.selected_before && mod.position.position < _doc->num_chars()) {
				// no selection; delete the character after the caret
				mod.position.removed_range = 1;
				mod.caret_front_before = true;
				mod.selected_before = false;
			}
			mod.caret_front_after = false;
			mod.selected_after = false;
			apply_modification_nofixup(std::move(mod));
		}


		/// Finishes modifying the text. Adds all recorded modification by calling \ref document::append_edit_data,
		/// sets the carets of \p source accordingly, and invokes \ref document::modified.
		void finish_edit(editor *source) {
			_doc->append_edit_data(std::move(_edit));
			finish_edit_nohistory(source);
		}
		/// Finishes modifying the text without adding recorded modification to the \ref document.
		/// Sets the carets of \p source accordingly, and invokes \ref document::modified.
		void finish_edit_nohistory(editor *source);
	protected:
		document * _doc = nullptr; ///< The \ref document that the user intends to modify.
		edit _edit; ///< Stores the modifications made by this modifier so far.
		/// Stores information used to adjust caret positions (since modifications must be made in increasing
		/// order of their positions).
		caret_fixup_info _cfixup;
		caret_fixup_info::context _cfctx; ///< Used with \ref _cfixup to adjust caret positions.
		caret_set _newcarets; ///< Stores the set of new carets, i.e., carets after the modifications.
		std::vector<linebreak_registry::text_clip_info> _removedclips; ///< Structure of removed text clips.

		/// Called after a modification has been made to add the corresponding information to \ref _cfixup and
		/// \ref _cfctx, which are used to adjust carets of new modifications.
		void _append_fixup_item(modification_position mp) {
			_cfixup.mods.push_back(mp);
			_cfctx.append_custom_modification(mp);
		}
		/// Appends the given caret to \ref _newcarets.
		void _append_caret(caret_selection sel) {
			_newcarets.add(caret_set::entry(sel, caret_data()));
		}
	};

	/// Stores an array of offsets and data related to each accumulated offset. Internally, the data is stored in a
	/// \ref binary_tree. This struct is designed so that insertion at a certain position, and querying given a
	/// position is completed in sublinear time.
	///
	/// \tparam Len The type used to represent lengths and offsets.
	/// \tparam Data The type of data associated to each offset.
	template <typename Len, typename Data> struct incremental_positional_registry {
	public:
		/// The data of a node that stores a length and the corresponding user data.
		struct node_data {
			/// Default constructor.
			node_data() = default;
			/// Initializes all fields of the struct.
			node_data(Len len, Data obj) : length(len), object(std::move(obj)) {
			}

			Len length = 0; ///< The length.
			Data object; ///< Associated data.
		};
		/// Stores synthesized data associated with each node.
		struct node_synth_data {
			/// The type of nodes.
			using node_type = binary_tree_node<node_data, node_synth_data>;

			Len total_length = 0; ///< The total length of all nodes in the subtree.

			/// The length of a node or subtree, stored in \ref node_data::length and
			/// \ref total_length, respectively.
			using length_property = sum_synthesizer::compact_property<
				synthesization_helper::field_value_property<&node_data::length>,
				&node_synth_data::total_length
			>;

			/// Invokes \ref sum_synthesizer::synthesize to update \ref length_property.
			inline static void synthesize(node_type &n) {
				sum_synthesizer::synthesize<length_property>(n);
			}
		};
		/// The type of a tree.
		using tree_type = binary_tree<node_data, node_synth_data>;
		/// The type of nodes.
		using node_type = typename tree_type::node;
		/// Used to implement \ref iterator and \ref const_iterator.
		///
		/// \tparam Const Whether this is a const iterator.
		template <bool Const> struct iterator_base {
		public:
			/// The type of underlying iterators. This is \ref binary_tree::const_iterator if \p Const is
			/// \p true, otherwise \ref binary_tree::iterator.
			using raw_iterator_t = std::conditional_t<
				Const, typename tree_type::const_iterator, typename tree_type::iterator
			>;
			/// The type of the value returned value when one dereferences an iterator.
			using dereferenced_t = std::conditional_t<Const, const Data, Data>;
			/// Reference to the underlying data.
			using reference = dereferenced_t & ;
			/// Pointer to the underlying data.
			using pointer = dereferenced_t * ;

			/// Default constructor.
			iterator_base() = default;
			/// Constructs an iterator from a \ref raw_iterator_t.
			explicit iterator_base(const raw_iterator_t &it) : _it(it) {
			}
			/// Converts a non-const iterator to a const one.
			iterator_base(const iterator_base<false> &it) : iterator_base(it.get_raw()) {
			}

			/// Returns the \ref node_data::object that the iterator points to.
			reference operator*() const {
				return _it.get_value_rawmod().object;
			}
			/// Accesses the fields of the \ref node_data::object that the iterator points to.
			pointer operator->() const {
				return &operator*();
			}

			/// Equality.
			friend bool operator==(const iterator_base &lhs, const iterator_base &rhs) {
				return lhs._it == rhs._it;
			}
			/// Inequality.
			friend bool operator!=(const iterator_base &lhs, const iterator_base &rhs) {
				return !(lhs == rhs);
			}

			/// Pre-increment.
			iterator_base &operator++() {
				++_it;
				return *this;
			}
			/// Post-increment.
			const iterator_base operator++(int) {
				iterator_base oldv = *this;
				++*this;
				return oldv;
			}

			/// Pre-decrement.
			iterator_base &operator--() {
				--_it;
				return *this;
			}
			/// Post-decrement.
			const iterator_base operator--(int) {
				iterator_base oldv = *this;
				--*this;
				return oldv;
			}

			/// Returns the \ref node_data that the iterator points to.
			const node_data &data() const {
				return *get_raw();
			}
			/// Returns the underlying \ref raw_iterator_t.
			const raw_iterator_t &get_raw() const {
				return _it;
			}
		protected:
			raw_iterator_t _it; ///< The underlying iterator.
		};
		using iterator = iterator_base<false>; ///< Iterators.
		using const_iterator = iterator_base<true>; ///< Const iterators.
		/// Contains an iterator to an entry, and the node's position.
		template <typename It> struct entry_info {
			/// Default constructor.
			entry_info() = default;
			/// Initializes all fields of the struct.
			entry_info(const It &it, Len pos) : iterator(it), node_position(pos) {
			}

			It iterator; ///< Iterator to the entry.
			/// The position of the node. Use \ref entry_position to obtain the position of the entry.
			Len node_position{};

			/// Returns the actual position of the entry.
			Len entry_position() const {
				return node_position + iterator.get_raw()->length;
			}

			/// Movoes to the next entry, updating \ref position accordingly. The caller is responsible of checking
			/// if \ref iterator is at the end of its container.
			void next_nocheck() {
				node_position += iterator.get_raw()->length;
				++iterator;
			}
		};

		/// Inserts the given object at the specified location. The positions of all other objects are kept unchanged.
		///
		/// \param pos Iterator to the object after which the new one will be inserted.
		/// \param offset The new object's offset relative to the position \p pos points to.
		/// \param object The object to insert at the position.
		/// \return Iterator to the inserted object.
		iterator insert_at(const const_iterator &pos, Len offset, Data object) {
			if (pos.get_raw() != _t.end()) {
				assert_true_usage(offset <= pos.data().length, "invalid position");
				_t.get_modifier_for(pos.get_raw().get_node())->length -= offset;
			}
			return iterator(_t.emplace_before(pos.get_raw(), offset, std::move(object)));
		}
		/// \overload
		iterator insert_at(Len pos, Data d) {
			auto it = _t.find_custom(_finder_at_or_after(), pos);
			return insert_at(const_iterator(it), pos, std::move(d));
		}

		/// Erases the given object from the collection while keeping the positions of all other objects unchanged.
		///
		/// \return Iterator to the entry after the one that's been erased.
		iterator erase(const const_iterator &it) {
			assert_true_usage(it.get_raw() != _t.end(), "invalid position");
			auto next = _t.erase(it.get_raw());
			if (next != _t.end()) {
				_t.get_modifier_for(next.get_node())->length += it.data().length;
			}
			return iterator(next);
		}

		/// Returns an iterator to the first object in the registry.
		iterator begin() {
			return iterator(_t.begin());
		}
		/// Const version of \ref begin().
		const_iterator begin() const {
			return const_iterator(_t.begin());
		}
		/// Returns an iterator past the last object in the registry.
		iterator end() {
			return iterator(_t.end());
		}
		/// Const version of \ref end().
		const_iterator end() const {
			return const_iterator(_t.end());
		}

		/// Adjusts the positions of all objects according to the \ref caret_fixup_info.
		///
		/// \todo Define better strategies for determining positions after adjusting them.
		void fixup(std::enable_if_t<std::is_same_v<Len, size_t>, const caret_fixup_info&> fixup) {
			for (const modification_position &mpos : fixup.mods) {
				size_t pos = mpos.position;
				auto it = _t.find_custom(_finder_after(), pos); // pos becomes the offset in the node
				if (it == _t.end()) {
					break;
				}
				size_t nchars = mpos.removed_range + pos;
				if (nchars <= it->length) { // not in deleted range
					it.get_modifier()->length += mpos.added_range - mpos.removed_range;
				} else {
					nchars -= it->length;
					it.get_modifier()->length = pos;
					auto iend = it;
					for (++iend; iend != _t.end(); ++iend) {
						if (iend->length < nchars) {
							nchars -= iend->length;
							iend.get_modifier()->length = 0;
						} else {
							break;
						}
					}
					if (iend != _t.end()) {
						iend.get_modifier()->length += mpos.added_range - nchars;
					}
				}
			}
		}

		/// Finds the first object at or after the given position.
		entry_info<iterator> find_at_or_after(Len pos) {
			_finder_at_or_after finder;
			auto it = _t.find_custom(finder, pos);
			return entry_info<iterator>(iterator(it), finder.position);
		}
		/// \overload
		entry_info<const_iterator> find_at_or_after(Len pos) const {
			_finder_at_or_after finder;
			auto it = _t.find_custom(finder, pos);
			return entry_info<const_iterator>(const_iterator(it), finder.position);
		}

		/// Clears the contents of the registry.
		void clear() {
			_t.clear();
		}
	protected:
		tree_type _t; ///< The underlying \ref binary_tree.

		/// Used to find the object immediately after a position.
		using _finder_after = sum_synthesizer::index_finder<typename node_synth_data::length_property>;
		/// Used to find the object at or immediately after a position. Also calculates the position of the found
		/// node.
		struct _finder_at_or_after {
			using finder = sum_synthesizer::index_finder<
				typename node_synth_data::length_property, false, std::less_equal<Len>
			>; ///< The underlying \ref sum_synthesizer::index_finder.
			/// Interface to \ref binary_tree::find_custom.
			int select_find(const node_type &nn, Len &target) {
				return finder::template select_find<typename node_synth_data::length_property>(nn, target, position);
			}

			Len position{}; ///< Stores the position of the node.
		};
	};
}
