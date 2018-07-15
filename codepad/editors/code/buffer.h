#pragma once

/// \file
/// Structs used to store the contents of a file.

#include <map>
#include <vector>

#include "../../core/bst.h"

namespace codepad::editor::code {
	/// The type of a line ending.
	enum class line_ending : unsigned char {
		none, ///< Unspecified or invalid. Sometimes also used to indicate EOF or soft linebreaks.
		r, ///< \p \\r, usually used in MacOS.
		n, ///< \p \\n, usually used in Linux.
		rn ///< \p \\r\\n, usually used in Windows.
	};
	/// Returns the corresponding string representation of a \ref line_ending, in UTF-8 encoding.
	inline const char *line_ending_to_static_u8_string(line_ending le) {
		switch (le) {
		case line_ending::r:
			return u8"\r";
		case line_ending::n:
			return u8"\n";
		case line_ending::rn:
			return u8"\r\n";
		default:
			return u8"";
		}
	}
	/// Returns the corresponding string representation of a \ref line_ending, in UTF-32 encoding.
	inline const char32_t *line_ending_to_static_u32_string(line_ending le) {
		switch (le) {
		case line_ending::r:
			return U"\r";
		case line_ending::n:
			return U"\n";
		case line_ending::rn:
			return U"\r\n";
		default:
			return U"";
		}
	}
	/// Returns the length, in codepoints, of the string representation of a \ref line_ending.
	inline size_t get_linebreak_length(line_ending le) {
		switch (le) {
		case line_ending::none:
			return 0;
		case line_ending::n:
			[[fallthrough]];
		case line_ending::r:
			return 1;
		case line_ending::rn:
			return 2;
		}
		return 0;
	}
	/// Stores an encoded string. The string doesn't necessarily end with an '\0'.
	///
	/// \tparam Char The type of the characters.
	/// \tparam Encoding The encoding of the string.
	template <typename Char, template <typename> typename Encoding = auto_utf> struct basic_encoded_string {
	public:
		/// The factor by which the allocated array is enlarged when it's too short.
		constexpr static double space_extend_factor = 1.5;

		/// The type used to store units of the string.
		using value_type = Char;
		/// Iterator type.
		using iterator = Char * ;
		/// Const iterator type.
		using const_iterator = const Char*;
		/// The encoding used by the string.
		using encoding = Encoding<Char>;

		/// Default constructor. Initializes the string to empty.
		basic_encoded_string() = default;
		/// Constructs the string from a string literal. The length of the literal is obtained by calling
		/// \ref get_unit_count, and the number of codepoints is obtained by calling \p encoding::count_codepoints.
		explicit basic_encoded_string(const Char *cs) : basic_encoded_string(cs, cs ? get_unit_count(cs) : 0) {
		}
		/// Constructs the string from a string literal using the given length. The number of codepoints
		/// is obtained by calling \p encoding::count_codepoints.
		basic_encoded_string(const Char *cs, size_t len) : basic_encoded_string(
			cs, len, cs ? encoding::count_codepoints(cs, cs + len) : 0
		) {
		}
		/// Constructs the string from a string literal using the given length and number of codepoints.
		basic_encoded_string(const Char *cs, size_t len, size_t ncp) :
			_chars(_duplicate_str(cs, len)), _len(len), _res(len), _cp(ncp) {
		}
		/// Copy constructor.
		basic_encoded_string(const basic_encoded_string &s) : basic_encoded_string(s._chars, s._len, s._cp) {
		}
		/// Move constructor.
		basic_encoded_string(basic_encoded_string &&s) noexcept :
			_chars(s._chars), _len(s._len), _res(s._res), _cp(s._cp) {
			s._chars = nullptr;
			s._len = 0;
			s._res = 0;
			s._cp = 0;
		}
		/// Copy assignment.
		basic_encoded_string &operator=(const basic_encoded_string &s) {
			if (_res > s._len) {
				std::memcpy(_chars, s._chars, sizeof(Char) * s._len);
			} else {
				std::free(_chars);
				_chars = _duplicate_str(s._chars, s._len);
				_res = s._len;
			}
			_len = s._len;
			_cp = s._cp;
			return *this;
		}
		/// Move assignment.
		basic_encoded_string &operator=(basic_encoded_string &&s) {
			std::swap(_chars, s._chars);
			std::swap(_len, s._len);
			std::swap(_res, s._res);
			std::swap(_cp, s._cp);
			return *this;
		}
		/// Frees (if any) the allocated string.
		~basic_encoded_string() {
			if (_chars) {
				std::free(_chars);
			}
		}

		/// Returns the unit at the given position.
		Char operator[](size_t id) const {
			assert_true_logical(id < _len, "access violation");
			return _chars[id];
		}
		/// Returns the underlying C-style string.
		const Char *data() const {
			return _chars;
		}

		/// Returns the number of units in the string.
		size_t length() const {
			return _len;
		}
		/// Returns the number of codepoints in the string.
		size_t num_codepoints() const {
			return _cp;
		}

		/// Returns an iterator to the beginning of the string.
		iterator begin() {
			return _chars;
		}
		/// Const version of begin().
		const_iterator begin() const {
			return _chars;
		}
		/// Returns an iterator past the end of the string.
		iterator end() {
			return _chars + _len;
		}
		/// Const version of end().
		const_iterator end() const {
			return _chars + _len;
		}

		/// Returns an iterator to the <tt>cp</tt>-th codepoint of the string.
		iterator at_codepoint(size_t cp) {
			iterator b = begin(), e = end();
			encoding::skip_codepoints(b, e, cp);
			return b;
		}
		/// Const version of at_codepoint(size_t).
		const_iterator at_codepoint(size_t cp) const {
			const_iterator b = begin(), e = end();
			encoding::skip_codepoints(b, e, cp);
			return b;
		}

		/// Reserves enough space for the given number of units.
		void reserve(size_t units) {
			if (_res < units) {
				_res = units;
				auto nc = static_cast<Char*>(std::malloc(sizeof(Char) * _res));
				if (_chars) {
					std::memcpy(nc, _chars, sizeof(Char) * _len);
					std::free(_chars);
				}
				_chars = nc;
			}
		}

		/// Returns a substring of the string.
		///
		/// \param beg Position of the substring's first unit in the string.
		/// \param len The length of the substring. It can be larger than the number of available units,
		///            in which case the substring starting at \p beg will be returned.
		basic_encoded_string substring(size_t beg, size_t len = std::numeric_limits<size_t>::max()) const {
			assert_true_logical(beg <= _len, "invalid substring range");
			return basic_encoded_string(_chars + beg, std::min(len, _len - beg));
		}
		/// \overload
		basic_encoded_string substring(const_iterator beg, const_iterator end) const {
			assert_true_logical(end >= beg, "invalid substring range");
			return substring(static_cast<size_t>(beg - begin()), static_cast<size_t>(end - beg));
		}
		/// Similar to substring(const_iterator, const_iterator) const, except that the number of codepoints
		/// of the substring is given.
		basic_encoded_string substring(const_iterator beg, const_iterator end, size_t ncp) const {
			assert_true_logical(end >= beg, "invalid substring range");
			return basic_encoded_string(beg, static_cast<size_t>(end - beg), ncp);
		}
		/// \overload
		basic_encoded_string substring(
			const codepoint_iterator_base<const_iterator> &beg, const codepoint_iterator_base<const_iterator> &end
		) const {
			return basic_encoded_string(
				beg.get_raw_iterator(),
				end.get_raw_iterator() - beg.get_raw_iterator(),
				end.codepoint_position() - beg.codepoint_position()
			);
		}

		/// Appends the given string to the encoded string and increments the number of codepoints by 1.
		void append_as_codepoint(const std::basic_string<Char> &c) {
			append_as_codepoint(c.begin(), c.end());
		}
		/// \overload
		template <typename It> void append_as_codepoint(It beg, It end) {
			++_cp;
			size_t newlen = _len + (end - beg);
			if (newlen > _res) {
				Char *nc = _begin_resize(newlen);
				if (_chars) {
					std::memcpy(nc, _chars, sizeof(Char) * _len);
				}
				_end_resize(nc);
			}
			Char *cc = _chars + _len;
			for (auto i = beg; i != end; ++i, ++cc) {
				*cc = *i;
			}
			_len = newlen;
		}

		/// Inserts the given string at the given position.
		///
		/// \param pos The position at which the string is to be inserted.
		/// \param str The contents of the string.
		/// \param len The length of the string.
		/// \param codepoints The number of codepoints in the string.
		///
		/// \remark It's not safe to insert part of this string into itself.
		void insert(size_t pos, const Char *str, size_t len, size_t codepoints) {
			if (len == 0) {
				return;
			}
			assert_true_logical(pos <= _len, "invalid position");
			_cp += codepoints;
			if (_len + len > _res) { // need to reserve more space
				Char *dst = _begin_resize(_len + len);
				if (_chars) {
					std::memcpy(dst, _chars, sizeof(Char) * pos);
					std::memcpy(dst + pos + len, _chars + pos, sizeof(Char) * (_len - pos));
				}
				std::memcpy(dst + pos, str, sizeof(Char) * len);
				_end_resize(dst);
			} else {
				std::memmove(_chars + pos + len, _chars + pos, sizeof(Char) * (_len - pos));
				std::memmove(_chars + pos, str, sizeof(Char) * len);
			}
			_len += len;
		}
		/// \overload
		void insert(size_t pos, const Char *str, size_t len) {
			insert(pos, str, len, encoding::count_codepoints(str, str + len));
		}
		/// \overload
		void insert(size_t pos, const basic_encoded_string &s) {
			insert(pos, s._chars, s._len, s._cp);
		}
		/// \overload
		void insert(const_iterator pos, const basic_encoded_string &s) {
			insert(static_cast<size_t>(pos - _chars), s);
		}

		/// Erases the specified substring from the string.
		///
		/// \param beg The position of the first unit of the substring.
		/// \param len The length of the substring. <tt>beg + len</tt>
		///            must not exceed the total length of the string.
		/// \param cpc The number of codepoints in the substring.
		void erase(size_t beg, size_t len, size_t cpc) {
			assert_true_logical(beg + len <= _len, "invalid substring length");
			std::memmove(_chars + beg, _chars + beg + len, sizeof(Char) * (_len - (beg + len)));
			_len -= len;
			_cp -= cpc;
		}
		/// \overload
		void erase(const_iterator beg, const_iterator e, size_t cpc) {
			erase(beg - begin(), e - beg, cpc);
		}
		/// \overload
		void erase(const_iterator beg, const_iterator end) {
			erase(beg, end, encoding::count_codepoints(beg, end));
		}
		/// \overload
		void erase(
			const codepoint_iterator_base<const_iterator> &beg,
			const codepoint_iterator_base<const_iterator> &end
		) {
			erase(beg.get_raw_iterator(), end.get_raw_iterator(), end.codepoint_position() - beg.codepoint_position());
		}
		/// Erases the specified substring from the string.
		///
		/// \param beg The position of the first unit of the substring.
		/// \param len The length of the substring. It will be automatically
		///            truncated if it exceeds the maximum possible length.
		void erase(size_t beg, size_t len = std::numeric_limits<size_t>::max()) {
			len = std::min(len, _len - beg);
			auto begit = begin() + beg;
			erase(begit, begit + len);
		}

		/// Replaces the given substring with another string.
		///
		/// \param pos The position of the first unit of the substring that's to be replaced.
		/// \param len The number of units in the substring that's to be replaced.
		/// \param remcps The number of codepoints in the substring that's to be replaced.
		/// \param repstr The new string used to replace the substring.
		/// \param replen The number of units in the new string.
		/// \param repcps The number of codepoints in the new string.
		void replace(size_t pos, size_t len, size_t remcps, const Char *repstr, size_t replen, size_t repcps) {
			assert_true_logical(pos + len <= _len, "invalid substring length");
			_cp += repcps - remcps;
			if (len != replen) {
				_len += replen - len;
				if (len < replen) {
					Char *dst = _begin_resize(_len);
					if (_chars) {
						std::memcpy(dst, _chars, sizeof(Char) * pos);
						std::memcpy(dst + pos + replen, _chars + pos + len, sizeof(Char) * (_len - (pos + replen)));
					}
					std::memcpy(dst + pos, repstr, sizeof(Char) * replen);
					_end_resize(dst);
					return;
				} else {
					if (_chars) {
						std::memmove(
							_chars + pos + len, _chars + pos + replen,
							sizeof(Char) * (_len - (pos + replen))
						);
					}
				}
			}
			std::memcpy(_chars + pos, repstr, sizeof(Char) * replen);
		}
		/// \overload
		void replace(const_iterator pos, const_iterator remend, size_t remcp, const Char *insstr, size_t inslen, size_t inscp) {
			replace(pos - begin(), remend - pos, remcp, insstr, inslen, inscp);
		}
		/// \overload
		void replace(const_iterator pos, const_iterator remend, size_t remcp, const basic_encoded_string &s) {
			replace(pos, remend, remcp, s._chars, s._len, s._cp);
		}
		/// \overload
		void replace(const_iterator pos, const_iterator remend, const Char *insstr, size_t inslen) {
			replace(
				pos, remend, encoding::count_codepoints(pos, remend),
				insstr, inslen, encoding::count_codepoints(insstr, insstr + inslen)
			);
		}
		/// \overload
		void replace(const_iterator pos, const_iterator remend, const basic_encoded_string &s) {
			replace(pos, remend, encoding::count_codepoints(pos, remend), s);
		}
	protected:
		/// Allocates a new piece of memory that can contain the given number of units,
		/// copies the given string into the newly-allocated memory, and returns a pointer to it.
		/// The returned string doesn't end with an '\0'.
		inline static Char *_duplicate_str(const Char *cs, size_t len) {
			if (len == 0) {
				return nullptr;
			}
			auto c = static_cast<Char*>(std::malloc(sizeof(Char) * len));
			std::memcpy(c, cs, sizeof(Char) * len);
			return c;
		}

		/// Called to start resizing the string. Allocates a new piece of memory that can contain
		/// the given number of units, and returns a pointer to the newly allocated memory.
		/// The caller can then fill the memory with new contents of the string.
		Char *_begin_resize(size_t nsz) {
			_res = std::max(nsz, static_cast<size_t>(static_cast<double>(_res) * space_extend_factor));
			return static_cast<Char*>(std::malloc(sizeof(Char) * _res));
		}
		/// Called to finish resizing the string. Frees the old string, and sets the pointer to the
		/// new one.
		///
		/// \param c Pointer to the new string. Normally the one returned by \ref _begin_resize.
		void _end_resize(Char *c) {
			if (_chars) {
				std::free(_chars);
			}
			_chars = c;
		}

		Char *_chars = nullptr; ///< The string data.
		size_t
			_len = 0, ///< The number of units in the string.
			_res = 0, ///< The number of units that the memory can contain.
			_cp = 0; ///< The number of codepoints in the string.
	};
	using encoded_u8string = basic_encoded_string<char, utf8>; ///< UTF-8 encoded string.
	using encoded_u16string = basic_encoded_string<char16_t, utf16>; ///< UTF-16 encoded string.
	using encoded_u32string = basic_encoded_string<char32_t, utf32>; ///< UTF-32 encoded string.

	/// Stores the contents of a file, etc. The contents is split into chunks and stored in a binary tree.
	/// Each node contains one chunk and some additional data to help navigate to a certain position.
	/// The text is split into chunks in no particular way, and multi-unit codepoints will never be split
	/// into two chunks.
	class string_buffer {
	public:
		/// The maximum number of units there can be in a single chunk.
		constexpr static size_t maximum_units_per_chunk = 1000;

		using char_type = char; ///< The type of stored characters.
		template <typename T> using encoding = utf8<T>; ///< The encoding.
		using encoding_data_t = encoding<char_type>; ///< The encoding and the character type.
		/// The type of \ref basic_encoded_string used to store characters.
		using string_type = basic_encoded_string<char_type, encoding>;

		/// Stores additional data of a node in the tree.
		struct node_data {
		public:
			/// A node of the tree.
			using node = binary_tree_node<string_type, node_data>;

			size_t
				total_length = 0, ///< The total number of units in this subtree.
				total_codepoints = 0; ///< The total number of codepoints in this subtree.

			using length_property = sum_synthesizer::compact_property<
				synthesization_helper::func_value_property<&string_type::length>,
				&node_data::total_length
			>; ///< Property used to obtain the total number of units in a subtree.
			using num_codepoints_property = sum_synthesizer::compact_property<
				synthesization_helper::func_value_property<&string_type::num_codepoints>,
				&node_data::total_codepoints
			>; ///< Property used to obtain the total number of codepoints in a subtree.

			/// Refreshes the data with the given node.
			inline static void synthesize(node &tn) {
				sum_synthesizer::synthesize<length_property, num_codepoints_property>(tn);
			}
		};
		/// The type of a tree used to store the strings.
		using tree_type = binary_tree<string_type, node_data>;
		/// The type of the tree's nodes.
		using node_type = tree_type::node;

		/// An iterator over the units of the buffer. Declared as a template class to avoid
		/// <tt>const_cast</tt>s.
		///
		/// \tparam TIt The type of the tree's iterators.
		/// \tparam SIt The type of the string's iterators.
		template <typename TIt, typename SIt> struct iterator_base {
			friend class string_buffer;
		public:
			/// Default constructor.
			iterator_base() = default;
			/// Copy constructor for non-const iterators, and converting constructor for const iterators.
			iterator_base(const iterator_base<tree_type::iterator, string_type::iterator> &it) :
				_it(it._it), _s(it._s) {
			}

			/// Pre-increment.
			iterator_base &operator++() {
				if (++_s == _it->end()) {
					_inc_it();
				}
				return *this;
			}
			/// Post-increment.
			const iterator_base operator++(int) {
				iterator_base ov = *this;
				++*this;
				return ov;
			}
			/// Pre-decrement.
			iterator_base &operator--() {
				if (_it == _it.get_container()->end() || _s == _it->begin()) {
					--_it;
					_s = _it->end();
				}
				--_s;
				return *this;
			}
			/// Post-decrement.
			const iterator_base operator--(int) {
				iterator_base ov = *this;
				--*this;
				return ov;
			}

			/// Moves the iterator to point to the next codepoint.
			///
			/// \param c Returns the current codepoint.
			/// \return Whether the current codepoint is valid.
			bool next_codepoint(char32_t &c) {
				bool res = next_codepoint(_s, nullptr, c);
				if (_s == _it.end()) {
					_inc_it();
				}
				return res;
			}
			/// \overload
			bool next_codepoint() {
				bool res = next_codepoint(_s, nullptr);
				if (_s == _it.end()) {
					_inc_it();
				}
				return res;
			}

			/// Equality.
			friend bool operator==(const iterator_base &lhs, const iterator_base &rhs) {
				return lhs._it == rhs._it && lhs._s == rhs._s;
			}
			/// Inequality
			friend bool operator!=(const iterator_base &lhs, const iterator_base &rhs) {
				return !(lhs == rhs);
			}

			/// Returns the unit this iterator points to.
			char_type operator*() const {
				return *_s;
			}
		protected:
			/// Constructs an iterator that points to the given position.
			iterator_base(const TIt &t, const SIt &s) : _it(t), _s(s) {
			}

			/// Moves \ref _it to point to the next node, and updates \ref _s to point to the first unit
			/// of the node, or \p nullptr if \ref _it is at the end.
			void _inc_it() {
				++_it;
				_s = _it != _it.get_container()->end() ? _it->begin() : SIt();
			}

			TIt _it; ///< The tree's iterator.
			SIt _s{}; ///< The \ref basic_encoded_string's iterator.
		};
		/// Iterator type.
		using iterator = iterator_base<tree_type::iterator, string_type::iterator>;
		/// Const iterator type.
		using const_iterator = iterator_base<tree_type::const_iterator, string_type::const_iterator>;

		/// Used to iterate through the codepoints of the buffer.
		struct codepoint_iterator {
			friend class string_buffer;
		public:
			/// Default constructor.
			codepoint_iterator() = default;

			/// Obtains the current codepoint.
			char32_t operator*() const {
				return current_codepoint();
			}
			/// Obtains the current codepoint.
			char32_t current_codepoint() const {
				return _cit.current_codepoint();
			}
			/// Returns whether the current codepoint is valid.
			bool current_good() const {
				return _cit.current_good();
			}

			/// Moves the iterator to the next codepoint.
			codepoint_iterator &operator++() {
				if (_cit.next_end()) {
					++_tit;
					_cit = _get_cit();
				} else {
					++_cit;
				}
				return *this;
			}

			/// Returns whether the iterator is at the end of the buffer.
			bool is_end() const {
				return _tit == _tit.get_container()->end();
			}
		protected:
			/// Iterator of codepoints in a single chunk.
			using _str_cp_iter = codepoint_iterator_base<string_type::const_iterator>;

			/// Initializes the iterator to point to the codepoint of the given chunk at the given position.
			///
			/// \param it Iterator to the chunk.
			/// \param at The index of the target codepoint.
			codepoint_iterator(const tree_type::const_iterator &it, size_t at) :
				_tit(it), _cit(_get_cit(at)) {
			}

			/// Returns a \ref _str_cp_iter that points to the codepoint of the current chunk at the given position.
			_str_cp_iter _get_cit(size_t pos = 0) const {
				if (_tit == _tit.get_container()->end()) {
					return _str_cp_iter(nullptr, nullptr, 0);
				}
				return _str_cp_iter(_tit->at_codepoint(pos), _tit->end(), pos);
			}

			tree_type::const_iterator _tit; ///< Iterator to the current chunk.
			_str_cp_iter _cit; ///< Iterator to the current codepoiont.
		};

		/// Returns an iterator to the first unit of the buffer.
		iterator begin() {
			auto t = _t.begin();
			return iterator(t, t.get_value_rawmod().begin());
		}
		/// Const version of begin().
		const_iterator begin() const {
			auto t = _t.begin();
			return const_iterator(t, t->begin());
		}
		/// Returns an iterator past the last unit of the buffer.
		iterator end() {
			return iterator(_t.end(), string_type::iterator());
		}
		/// Const version of end().
		const_iterator end() const {
			return const_iterator(_t.end(), string_type::const_iterator());
		}

		/// Returns an iterator to the first chunk of the buffer.
		tree_type::const_iterator node_begin() const {
			return _t.begin();
		}
		/// Returns an iterator past the last chunk of the buffer.
		tree_type::const_iterator node_end() const {
			return _t.end();
		}

		/// Returns an iterator to the codepoint at the given position of the buffer.
		iterator at_codepoint(size_t cp) {
			auto t = _t.find_custom(_codepoint_index_finder(), cp);
			if (t == _t.end()) {
				return iterator(t, string_type::iterator());
			}
			return iterator(t, t.get_value_rawmod().at_codepoint(cp));
		}
		/// Const version of at_codepoint().
		const_iterator at_codepoint(size_t cp) const {
			auto t = _t.find_custom(_codepoint_index_finder(), cp);
			if (t == _t.end()) {
				return const_iterator(t, string_type::const_iterator());
			}
			return const_iterator(t, t->at_codepoint(cp));
		}
		/// Returns an iterator to the unit at the given position of the buffer.
		iterator at_unit(size_t cp) {
			auto t = _t.find_custom(_unit_index_finder(), cp);
			if (t == _t.end()) {
				return iterator(t, string_type::iterator());
			}
			return iterator(t, t.get_value_rawmod().begin() + cp);
		}
		/// Const version of at_unit().
		const_iterator at_unit(size_t cp) const {
			auto t = _t.find_custom(_unit_index_finder(), cp);
			if (t == _t.end()) {
				return const_iterator(t, string_type::const_iterator());
			}
			return const_iterator(t, t->begin() + cp);
		}

		/// Returns the position, in units, of an iterator.
		size_t get_position_units(const_iterator i) const {
			return _units_before(i._it.get_node()) + (i._s - i._it->begin());
		}

		/// Returns a \ref codepoint_iterator to the first codepoint of the buffer.
		codepoint_iterator begin_codepoint() const {
			return codepoint_iterator(_t.begin(), 0);
		}
		/// Returns a \ref codepoint_iterator to the codepoint at the given position.
		codepoint_iterator at_codepoint_iterator(size_t pos) const {
			auto it = _t.find_custom(_codepoint_index_finder(), pos);
			return codepoint_iterator(it, pos);
		}
		/// Returns the position, in codepoints, of an \ref codepoint_iterator.
		size_t get_position(const codepoint_iterator &i) const {
			if (i.is_end()) {
				return _t.root() ? _t.root()->synth_data.total_codepoints : 0;
			}
			return _codepoints_before(i._tit.get_node()) + i._cit.codepoint_position();
		}
		/// \overload
		size_t get_position_units(const codepoint_iterator &i) const {
			if (i.is_end()) {
				return _t.root() ? _t.root()->synth_data.total_length : 0;
			}
			auto res = _units_before(i._tit.get_node()) + i._cit.unit_position(i._tit->begin());
			return res;
//			return _units_before(i._tit.get_node()) + i._cit.unit_position(i._tit->begin());
		}

		/// Sets the contents of the buffer.
		///
		/// \param getcodepoint A callable object that accepts a reference to a \p char32_t as a
		///                     parameter, in which it stores the retrieved codepoint, and returns
		///                     a bool indicating whether the retrieval is successful, i.e., whether
		///                     there are available codepoints to retrieve.
		template <typename T> void set(const T &getcodepoint) {
			std::vector<string_type> strings;
			string_type current;
			current.reserve(maximum_units_per_chunk);
			for (char32_t cc = '\0'; getcodepoint(cc); ) {
				std::string chars = encoding_data_t::encode_codepoint(cc);
				if (current.length() + chars.length() > maximum_units_per_chunk) {
					strings.push_back(std::move(current));
					current = string_type();
					current.reserve(maximum_units_per_chunk);
				}
				current.append_as_codepoint(chars);
			}
			if (current.length() > 0) {
				strings.push_back(std::move(current));
			}
			_t.clear();
			_t.insert_range_before_move(nullptr, strings.begin(), strings.end());
		}
		/// Returns a substring of the buffer.
		string_type substring(const codepoint_iterator &beg, const codepoint_iterator &end) const {
			if (beg._tit == _t.end()) {
				return string_type();
			}
			if (beg._tit == end._tit) { // in the same chunk
				return beg._tit->substring(beg._cit, end._cit);
			}
			string_type result;
			result.reserve(get_position_units(end) - get_position_units(beg));
			result.insert(
				0, beg._cit.get_raw_iterator(),
				beg._tit->end() - beg._cit.get_raw_iterator(),
				beg._tit->num_codepoints() - beg._cit.codepoint_position()
			); // insert the part in the first chunk
			tree_type::const_iterator it = beg._tit;
			for (++it; it != end._tit; ++it) { // insert full chunks
				result.insert(result.end(), *it);
			}
			if (end._tit != _t.end()) {
				result.insert(
					result.length(), end._tit->begin(),
					end._cit.get_raw_iterator() - end._tit->begin(),
					end._cit.codepoint_position()
				); // insert the part in the last chunk
			}
			return result;
		}
		/// Erases a substring from the buffer.
		void erase(const codepoint_iterator &beg, const codepoint_iterator &end) {
			node_type *bnode = beg._tit.get_node(), *enode = end._tit.get_node();
			if (bnode == nullptr) {
				return;
			}
			if (beg._tit == end._tit) { // same chunk
				_t.get_modifier_for(bnode)->erase(beg._cit, end._cit);
				_try_merge_small_nodes(bnode);
				return;
			}
			_t.erase(bnode->next(), enode); // erase full chunks
			_t.get_modifier_for(bnode)->erase(
				beg._cit.get_raw_iterator(), bnode->value.end(),
				bnode->value.num_codepoints() - beg._cit.codepoint_position()
			); // erase the part in the first chunk
			if (enode) {
				_t.get_modifier_for(enode)->erase(
					enode->value.begin(), end._cit.get_raw_iterator(),
					end._cit.codepoint_position()
				); // erase the part in the last chunk
				_try_merge_small_nodes(enode);
			}
			_try_merge_small_nodes(bnode);
		}
		/// Inserts an array of codepoints at the given position.
		///
		/// \param pos The position at which the string will be inserted.
		/// \param getcodepoint See set(const T&).
		template <typename T> void insert(const codepoint_iterator &pos, const T &getcodepoint) {
			node_type *node2ins = pos._tit.get_node(), *node2upd = nullptr;
			string_type afterstr, *curstr;
			std::vector<string_type> strs; // the buffer for (not all) inserted codepoints
			if (node2ins != nullptr && pos._cit.get_raw_iterator() != pos._tit->begin()) {
				// insert at the middle of a chunk
				size_t ncp = node2ins->value.num_codepoints() - pos._cit.codepoint_position();
				// save the second part & truncate the chunk
				afterstr = node2ins->value.substring(pos._cit.get_raw_iterator(), node2ins->value.end(), ncp);
				node2ins->value.erase(pos._cit.get_raw_iterator(), node2ins->value.end(), ncp);
				node2upd = node2ins;
				node2ins = node2ins->next();
				curstr = &node2upd->value;
			} else if (pos._tit == _t.begin()) {
				// insert at the beginning of a chunk, no need to split
				string_type st;
				st.reserve(maximum_units_per_chunk);
				strs.push_back(std::move(st));
				curstr = &strs.back();
			} else {
				// insert at the very end
				tree_type::const_iterator it = pos._tit;
				--it;
				node2upd = it.get_node();
				curstr = &node2upd->value;
			}
			for (char32_t cc = '\0'; getcodepoint(cc); ) { // insert codepoints
				auto chars = string_buffer::encoding_data_t::encode_codepoint(cc);
				if (curstr->length() + chars.length() > maximum_units_per_chunk) {
					// curstr would be too long, add a new string
					string_type ss;
					ss.reserve(maximum_units_per_chunk);
					strs.push_back(std::move(ss));
					curstr = &strs.back();
				}
				curstr->append_as_codepoint(chars); // append codepoint to curstr
			}
			if (afterstr.length() > 0) {
				// at the middle of a chunk, add the second part to the strings
				if (curstr->length() + afterstr.length() <= maximum_units_per_chunk) {
					curstr->insert(curstr->end(), afterstr);
				} else {
					strs.push_back(std::move(afterstr)); // curstr is not changed
				}
			} else if (curstr->length() == 0) {
				// a string was appended, but no codepoint is added afterwards
				assert_true_logical(!strs.empty(), "corrupted string buffer");
				strs.pop_back();
			}
			_t.refresh_synthesized_result(node2upd);
			_t.insert_range_before_move(node2ins, strs.begin(), strs.end()); // insert the strings
			// try to merge small nodes
			node_type
				*lstnode = node2ins ? node2ins->prev() : _t.max(),
				*llstnode = lstnode ? lstnode->prev() : nullptr;
			_try_merge_small_nodes(lstnode);
			_try_merge_small_nodes(llstnode);
		}

		/// Returns the number of units that this buffer contains.
		size_t length() const {
			node_type *n = _t.root();
			return n ? n->synth_data.total_length : 0;
		}
		/// Returns the number of codepoints that this buffer contains.
		size_t num_codepoints() const {
			node_type *n = _t.root();
			return n ? n->synth_data.total_codepoints : 0;
		}

		/// Clears the contents of this buffer.
		void clear() {
			_t.clear();
		}
	protected:
		/// Used to find the chunk in which the codepoint at the given index lies.
		using _codepoint_index_finder = sum_synthesizer::index_finder<node_data::num_codepoints_property>;
		/// Used to find the chunk in which the unit at the given index lies.
		using _unit_index_finder = sum_synthesizer::index_finder<node_data::length_property>;

		/// Wrapper of \ref sum_synthesizer::sum_before with only one property.
		template <typename Prop> size_t _synth_sum_before(node_type *n) const {
			size_t v = 0;
			sum_synthesizer::sum_before<Prop>(_t.get_const_iterator_for(n), v);
			return v;
		}
		/// Shorthand for \ref _synth_sum_before<node_data::num_codepoints_property>.
		size_t _codepoints_before(node_type *n) const {
			return _synth_sum_before<node_data::num_codepoints_property>(n);
		}
		/// Shorthand for \ref _synth_sum_before<node_data::units_property>.
		size_t _units_before(node_type *n) const {
			return _synth_sum_before<node_data::length_property>(n);
		}

		/// Merges a node with one or more of its neighboring nodes if their total length are
		/// smaller than the maximum value.
		///
		/// \todo Find better merging strategy.
		void _try_merge_small_nodes(node_type *n) {
			if (n == nullptr) {
				return;
			}
			size_t nvl = n->value.length();
			if (nvl * 2 > maximum_units_per_chunk) {
				return;
			}
			node_type *prev = n->prev();
			if (prev && prev->value.length() + nvl < maximum_units_per_chunk) {
				_t.get_modifier_for(prev)->insert(prev->value.end(), n->value);
				_t.erase(n);
				return;
			}
			node_type *next = n->next();
			if (next && next->value.length() + nvl < maximum_units_per_chunk) {
				_t.get_modifier_for(n)->insert(n->value.end(), next->value);
				_t.erase(next);
				return;
			}
		}

		tree_type _t; ///< The underlying binary tree that stores all the chunks.
	};
}
