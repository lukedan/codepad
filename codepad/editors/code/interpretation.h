// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of classes used to interpret a \ref codepad::editors::buffer.

#include <map>

#include "../../core/encodings.h"
#include "../buffer.h"
#include "linebreak_registry.h"
#include "theme.h"
#include "caret_set.h"

namespace codepad::editors::code {
	/// Abstract class used to represent an encoding used to interpret a \ref buffer.
	class buffer_encoding {
	public:
		/// Virtual destructor.
		virtual ~buffer_encoding() = default;

		/// Returns the name of this encoding.
		virtual str_view_t get_name() const = 0;
		/// Returns the maximum possible length of a codepoint, in bytes.
		virtual size_t get_maximum_codepoint_length() const = 0;

		/// Moves \p it to the beginning of the next codepoint, where the last byte is indicated by \p end, and
		/// stores the decoded codepoint in \p res.
		///
		/// \return Indicates whether the decoded codepoint is valid.
		virtual bool next_codepoint(
			buffer::const_iterator &it, const buffer::const_iterator &end, codepoint &res
		) const = 0;
		/// Similar to \ref next_codepoint(buffer::const_iterator&, const buffer::const_iterator&, codepoint&) const
		/// except that this function doesn't extract the codepoint itself.
		virtual bool next_codepoint(buffer::const_iterator&, const buffer::const_iterator&) const = 0;
		/// Returns the encoded representation of the given codepoint.
		virtual byte_string encode_codepoint(codepoint) const = 0;
	};
	/// Encoding used to interpret a \ref buffer, based on a class in the \ref codepad::encodings namespace.
	template <typename Encoding> class predefined_buffer_encoding : public buffer_encoding {
		/// Calls \p get_name() in \p Encoding.
		str_view_t get_name() const override {
			return Encoding::get_name();
		}
		/// Calls \p get_maximum_codepoint_length() in \p Encoding.
		size_t get_maximum_codepoint_length() const override {
			return Encoding::get_maximum_codepoint_length();
		}

		/// Calls \p next_codepoint() in \p Encoding.
		bool next_codepoint(
			buffer::const_iterator &it, const buffer::const_iterator &end, codepoint &res
		) const override {
			return Encoding::next_codepoint(it, end, res);
		}
		/// Calls \p next_codepoint() in \p Encoding.
		bool next_codepoint(buffer::const_iterator &it, const buffer::const_iterator &end) const override {
			return Encoding::next_codepoint(it, end);
		}
		/// Calls \p encode_codepoint() in \p Encoding.
		byte_string encode_codepoint(codepoint cp) const override {
			return Encoding::encode_codepoint(cp);
		}
	};

	/// Manages the list of available encodings.
	class encoding_manager {
	public:
		/// Registers a series of built-in encodings, and sets the default encoding to UTF-8.
		encoding_manager() {
			register_builtin_encoding<encodings::utf8>();
			register_builtin_encoding<encodings::utf16<endianness::little_endian>>();
			register_builtin_encoding<encodings::utf16<endianness::big_endian>>();

			set_default(*get_encoding(encodings::utf8::get_name()));
		}

		/// Returns the default encoding.
		const buffer_encoding &get_default() const {
			return *_default;
		}
		/// Sets the default encoding. This will only affect files opened after this operation.
		void set_default(const buffer_encoding &enc) {
			_default = &enc;
		}

		/// Registers the given encoding. This function accepts a rvalue reference so that if the registration fails,
		/// the original \p std::unique_ptr will not be affected.
		///
		/// \return Whether the registration was successful (i.e., no duplicate names were found).
		bool register_encoding(std::unique_ptr<buffer_encoding> &&enc) {
			return _map.try_emplace(str_t(enc->get_name()), std::move(enc)).second;
		}
		/// Registers a built-in encoding.
		///
		/// \return Whether the registration was successful (i.e., no duplicate names were found).
		template <typename Encoding> bool register_builtin_encoding() {
			return register_encoding(std::make_unique<predefined_buffer_encoding<Encoding>>());
		}
		/// Returns the encoding with the given name, or \p nullptr if no such encoding is found.
		const buffer_encoding *get_encoding(str_view_t name) const {
			auto it = _map.find(name);
			if (it != _map.end()) {
				return it->second.get();
			}
			return nullptr;
		}
		/// Returns the full list of registered encodings.
		const std::map<str_t, std::unique_ptr<buffer_encoding>, std::less<>> &get_all_encodings() const {
			return _map;
		}

		/// Returns the global \ref encoding_manager object.
		static encoding_manager &get();
	protected:
		/// The mapping between encoding names and \ref buffer_encoding instances.
		std::map<str_t, std::unique_ptr<buffer_encoding>, std::less<>> _map;
		const buffer_encoding *_default = nullptr; ///< The default encoding.
	};

	/// Interprets a \ref buffer using a given encoding. This struct stores information that can be used to determine
	/// certain boundaries between codepoints.
	class interpretation {
	public:
		/// Maximum number of codepoints in a chunk.
		constexpr static size_t maximum_codepoints_per_chunk = 1000;

		/// Information about a consecutive sequence of codepoints in the buffer.
		struct chunk_data {
			/// Default constructor.
			chunk_data() = default;
			/// Initializes all fields of this struct.
			chunk_data(size_t bytes, size_t cps) : num_bytes(bytes), num_codepoints(cps) {
			}

			size_t
				num_bytes = 0, ///< The number of bytes in this chunk.
				/// The number of codepoints, including both valid and invalid ones, in this chunk.
				num_codepoints = 0;
		};
		/// Contains additional synthesized data of a node.
		struct node_data {
			/// A node of the tree.
			using node = binary_tree_node<chunk_data, node_data>;

			size_t
				total_bytes = 0, ///< The total number of bytes in this subtree.
				total_codepoints = 0; ///< The total number of codepoints in this subtree.

			using num_bytes_property = sum_synthesizer::compact_property<
				synthesization_helper::field_value_property<&chunk_data::num_bytes>,
				&node_data::total_bytes
			>; ///<	Property used to obtain the total number of bytes in a subtree.
			using num_codepoints_property = sum_synthesizer::compact_property<
				synthesization_helper::field_value_property<&chunk_data::num_codepoints>,
				&node_data::total_codepoints
			>; ///< Property used to obtain the total number of codepoints in a subtree.

			/// Refreshes the given node's data.
			inline static void synthesize(node &n) {
				sum_synthesizer::synthesize<num_bytes_property, num_codepoints_property>(n);
			}
		};
		/// The type of a tree used to store the chunks.
		using tree_type = binary_tree<chunk_data, node_data>;
		/// The type of a node in the tree.
		using node_type = tree_type::node;

		/// Used to iterate through codepoints in this interpretation.
		struct codepoint_iterator {
			friend interpretation;
		public:
			/// Default constructor.
			codepoint_iterator() = default;

			/// Moves this iterator to point to the next codepoint.
			///
			/// \return Whether the iterator is past the end of the \ref buffer after this operation.
			bool next() {
				_cur = _next;
				if (_cur != _interp->get_buffer()->end()) {
					_valid = _interp->get_encoding()->next_codepoint(_next, _interp->get_buffer()->end(), _cp);
					return true;
				}
				_cp = 0;
				_valid = false;
				return false;
			}

			/// Returns the current codepoint.
			codepoint get_codepoint() const {
				return _cp;
			}
			/// Returns whether the current codepoint is valid.
			bool is_codepoint_valid() const {
				return _valid;
			}
			/// Returns whether the iterator is past the end of the \ref buffer.
			bool ended() const {
				return _interp == nullptr || _cur == _interp->get_buffer()->end();
			}
			/// Returns the underlying \ref buffer::const_iterator pointing to the first byte of the current
			/// codepoint.
			const buffer::const_iterator &get_raw() const {
				return _cur;
			}

			/// Returns the associated \ref interpretation.
			const interpretation *get_interpretation() const {
				return _interp;
			}
		protected:
			buffer::const_iterator
				_cur, ///< Iterator to the beginning of the current codepoint.
				_next; ///< Iterator to the beginning of the next codepoint.
			const interpretation *_interp = nullptr; ///< The \ref interpretation that created this iterator.
			codepoint _cp = 0; ///< The current codepoint.
			bool _valid = false; ///< Whether the current codepoint is valid.

			/// Constructs an iterator pointing to the given position.
			codepoint_iterator(const buffer::const_iterator &cur, const interpretation &interp) :
				_cur(cur), _next(cur), _interp(&interp) {
				if (_cur != _interp->get_buffer()->end()) {
					_valid = _interp->get_encoding()->next_codepoint(_next, _interp->get_buffer()->end(), _cp);
				}
			}
		};
		/// Used to iterate through characters in an \ref interpretation.
		struct character_iterator {
			friend interpretation;
		public:
			/// Default constructor.
			character_iterator() = default;

			/// Moves on to the next character.
			void next() {
				if (is_linebreak()) {
					for (size_t i = 0; i < get_line_ending_length(_lbit->ending); ++i) {
						_cpit.next();
					}
					++_lbit;
					_col = 0;
				} else {
					_cpit.next();
					++_col;
				}
			}
			/// Returns whether the current character is a linebreak.
			bool is_linebreak() const {
				return _col == _lbit->nonbreak_chars;
			}
			/// Returns the type of the current line's linebreak.
			line_ending get_linebreak() const {
				return _lbit->ending;
			}
			/// Returns the column where this iterator's at.
			size_t get_column() const {
				return _col;
			}
			/// Returns the underlying \ref codepoint_iterator.
			const codepoint_iterator &codepoint() const {
				return _cpit;
			}
		protected:
			codepoint_iterator _cpit; ///< Iterates through the actual codepoints.
			linebreak_registry::iterator _lbit; ///< Iterates through linebreaks.
			size_t _col = 0; ///< The column where \ref _cpit is at.

			/// Used by \ref interpretation to create an iterator pointing to a specific character.
			character_iterator(
				const codepoint_iterator &cpit, const linebreak_registry::iterator lbit, size_t col
			) : _cpit(cpit), _lbit(lbit), _col(col) {
			}
		};

		/// Similar to \ref linebreak_registry::position_converter, but converts between
		/// positions of codepoints and bytes instead.
		struct codepoint_position_converter {
		public:
			/// Initializes this converter with the corresponding \ref interpretation.
			codepoint_position_converter(const interpretation &interp) : _interp(interp) {
				reset();
			}

			/// Resets this converter so that a new series of queries can be made.
			void reset() {
				_cpchk = _interp._chks.begin();
				_byteit = _interp.get_buffer()->begin();
				_firstcp = _firstbyte = _chkcp = 0;
			}

			/// Returns the position of the first byte of the given codepoint. This should not be used mixedly with
			/// \ref byte_to_codepoint().
			size_t codepoint_to_byte(size_t pos) {
				if (_cpchk == _interp._chks.end()) {
					// if _cpchk is at the end, then all following queries can only query about the end
					return _interp._buf->length();
				}
				if (_firstcp + _cpchk->num_codepoints > pos) {
					pos -= _firstcp;
				} else {
					_firstcp = pos;
					_chkcp = 0;
					_codepoint_pos_converter finder;
					_cpchk = _interp._chks.find_custom(finder, pos);
					_firstcp -= pos;
					_firstbyte = finder.total_bytes;
					_byteit = _interp.get_buffer()->at(_firstbyte);
				}
				for (; _chkcp < pos; ++_chkcp) {
					_interp.get_encoding()->next_codepoint(_byteit, _interp.get_buffer()->end());
				}
				return _byteit.get_position();
			}
			/// Returns the position of the codepoint that contains the given byte. This should not be used mixedly
			/// with \ref codepoint_to_byte().
			size_t byte_to_codepoint(size_t pos) {
				if (_cpchk == _interp._chks.end()) {
					// if _cpchk is at the end, then all following queries can only query about the end
					return _chkcp;
				}
				size_t globalpos = pos;
				if (_firstbyte + _cpchk->num_bytes > pos) {
					pos -= _firstbyte;
				} else {
					_firstbyte = pos;
					_chkcp = 0;
					_byte_pos_converter finder;
					_cpchk = _interp._chks.find_custom(finder, pos);
					_firstbyte -= pos;
					_firstcp = finder.total_codepoints;
					_byteit = _interp.get_buffer()->at(_firstbyte);
				}
				for (; _byteit.get_position() < globalpos; ++_chkcp) {
					_interp.get_encoding()->next_codepoint(_byteit, _interp.get_buffer()->end());
				}
				return _firstcp + _chkcp - (_byteit.get_position() == globalpos ? 0 : 1);
			}
		protected:
			tree_type::const_iterator _cpchk; ///< Iterator to the current chunk in which the codepoint lies.
			buffer::const_iterator _byteit; ///< Iterator to the current byte.
			const interpretation &_interp; ///< The associated \ref interpretation.
			size_t
				_firstcp = 0, ///< The number of codepoints before \ref _cpchk.
				_firstbyte = 0, ///< The number of bytes before \ref _cpchk.
				_chkcp = 0; ///< The position of \ref _byteit in this chunk.
		};
		/// Combines \ref codepoint_position_converter and
		/// \ref linebreak_registry::position_converter to convert between the positions of characters
		/// and bytes.
		struct character_position_converter {
		public:
			/// Initializes this converter with the corresponding \ref interpretation.
			character_position_converter(const interpretation &interp) :
				_char2cp(interp.get_linebreaks()), _cp2byte(interp) {
			}

			/// Resets this converter.
			void reset() {
				_char2cp.reset();
				_cp2byte.reset();
			}

			/// Returns the position of the first byte of the character at the given position. This should not be
			/// used mixedly with \ref byte_to_character().
			size_t character_to_byte(size_t pos) {
				return _cp2byte.codepoint_to_byte(_char2cp.character_to_codepoint(pos));
			}
			/// Returns the position of the character that contains this byte. This should not be used mixedly with
			/// \ref character_to_byte().
			size_t byte_to_character(size_t pos) {
				return _char2cp.codepoint_to_character(_cp2byte.byte_to_codepoint(pos));
			}
		protected:
			/// Used to convert between the positions of characters and codepoints.
			linebreak_registry::position_converter _char2cp;
			/// Used to convert between the positions of codepoints and bytes.
			codepoint_position_converter _cp2byte;
		};

		/// Constructor.
		interpretation(std::shared_ptr<buffer> buf, const buffer_encoding &encoding) :
			_buf(std::move(buf)), _encoding(&encoding) {

			_begin_edit_tok = _buf->begin_edit += [this](buffer::begin_edit_info &info) {
				_on_begin_edit(info);
			};
			_end_edit_tok = _buf->end_edit += [this](buffer::end_edit_info &info) {
				_on_end_edit(info);
			};

			performance_monitor mon("full_decode", performance_monitor::log_condition::always);

			std::vector<chunk_data> chunks;
			linebreak_analyzer lines;
			size_t
				chkbegbytes = 0, chkbegcps = 0, curcp = 0,
				splitcp = maximum_codepoints_per_chunk; // where to split the next chunk
			for (buffer::const_iterator cur = _buf->begin(); cur != _buf->end(); ++curcp) {
				if (curcp >= splitcp) {
					// break chunk before this codepoint
					size_t bytepos = cur.get_position();
					chunks.emplace_back(bytepos - chkbegbytes, curcp - chkbegcps);
					chkbegbytes = bytepos;
					chkbegcps = curcp;
					splitcp = curcp + maximum_codepoints_per_chunk;
				}
				// decode codepoint
				codepoint curc = 0;
				if (!_encoding->next_codepoint(cur, _buf->end(), curc)) { // invalid codepoint?
					curc = 0; // disable linebreak detection
				}
				lines.put(curc);
			}
			// process the last chunk
			lines.finish();
			if (_buf->length() > chkbegbytes) {
				chunks.emplace_back(_buf->length() - chkbegbytes, curcp - chkbegcps);
			}
			// insert into the tree
			_chks.insert_range_before_move(_chks.end(), chunks.begin(), chunks.end());
			_lbs.insert_chars(_lbs.begin(), 0, lines.result());
		}
		/// No copy construction.
		interpretation(const interpretation&) = delete;
		/// No copy assignment.
		interpretation &operator=(const interpretation&) = delete;
		/// Unregisters from \ref buffer::end_edit.
		~interpretation() {
			_buf->begin_edit -= _begin_edit_tok;
			_buf->end_edit -= _end_edit_tok;
		}

		/// Returns a \ref codepoint_iterator pointing at the specified codepoint.
		codepoint_iterator at_codepoint(size_t pos) const {
			_codepoint_pos_converter finder;
			_chks.find_custom(finder, pos);
			codepoint_iterator res(_buf->at(finder.total_bytes), *this);
			for (size_t i = 0; i < pos; ++i) {
				res.next();
			}
			return res;
		}
		/// Returns a \ref character_iterator pointing at the specified character.
		character_iterator at_character(size_t pos) const {
			auto[colinfo, cp] = _lbs.get_line_and_column_and_codepoint_of_char(pos);
			return character_iterator(at_codepoint(cp), colinfo.line_iterator, colinfo.position_in_line);
		}

		/// Returns the total number of codepoints in this \ref interpretation.
		size_t num_codepoints() const {
			return _chks.root() == nullptr ? 0 : _chks.root()->synth_data.total_codepoints;
		}
		/// Returns the result of \ref linebreak_registry::num_linebreaks() plus one.
		size_t num_lines() const {
			return _lbs.num_linebreaks() + 1;
		}

		/// Returns the \ref buffer that this object interprets.
		const std::shared_ptr<buffer> &get_buffer() const {
			return _buf;
		}
		/// Returns the \ref buffer_encoding used by this object.
		const buffer_encoding *get_encoding() const {
			return _encoding;
		}
		/// Returns the linebreaks in this \ref interpretation.
		const linebreak_registry &get_linebreaks() const {
			return _lbs;
		}
		/// Returns the \ref text_theme_data associated with this \ref interpretation.
		const text_theme_data &get_text_theme() const {
			return _theme;
		}
		/// Returns the default line ending for this \ref interpretation.
		line_ending get_default_line_ending() const {
			return _line_ending;
		}
		/// Sets the default line ending for this \ref interpretation. Note that this does not affect existing text.
		void set_default_line_ending(line_ending end) {
			_line_ending = end;
		}


		/// Called when the user presses `backspace' to modify the underlying \ref buffer. If there's only one caret
		/// at the very beginning of the document, no modification will be done.
		void on_backspace(caret_set &carets, ui::element *src) {
			if (
				carets.carets.size() > 1 ||
				carets.carets.begin()->first.first != carets.carets.begin()->first.second ||
				carets.carets.begin()->first.first != 0
				) {
				carets.calculate_byte_positions(*this);
				std::vector<_precomp_mod_positions> pos = _precomp_mod_backspace(carets);
				buffer::scoped_normal_modifier mod(*_buf, src);
				for (const _precomp_mod_positions &modpos : pos) {
					mod.modify(modpos.begin, modpos.length, byte_string());
				}
			}
		}
		/// Called when the user presses `delete' to modify the underlying \ref buffer. If there's only one caret
		/// at the very end of the document, no modification will be done.
		void on_delete(caret_set &carets, ui::element *src) {
			if (
				carets.carets.size() > 1 ||
				carets.carets.begin()->first.first != carets.carets.begin()->first.second ||
				carets.carets.begin()->first.first != get_linebreaks().num_chars()
				) {
				carets.calculate_byte_positions(*this);
				std::vector<_precomp_mod_positions> pos = _precomp_mod_delete(carets);
				buffer::scoped_normal_modifier mod(*_buf, src);
				for (const _precomp_mod_positions &modpos : pos) {
					mod.modify(modpos.begin, modpos.length, byte_string());
				}
			}
		}
		/// Called when the user enters a short clip of text to modify the underlying \ref buffer.
		void on_insert(caret_set &carets, const byte_string &contents, ui::element *src) {
			carets.calculate_byte_positions(*this);
			std::vector<_precomp_mod_positions> pos = _precomp_mod_insert(carets);
			buffer::scoped_normal_modifier mod(*_buf, src);
			for (const _precomp_mod_positions &modpos : pos) {
				mod.modify(modpos.begin, modpos.length, contents);
			}
		}


		/// Checks the integrity of this \ref interpretation by re-interpreting the underlying \ref buffer. This
		/// function is mainly intended to be used for debugging.
		///
		/// \return Indicates whether the data in \ref _lbs and \ref _chks are valid and correct.
		bool check_integrity() const {
			bool error = false;

			for (const chunk_data &chk : _chks) {
				if (chk.num_codepoints == 0 || chk.num_bytes == 0) {
					error = true;
					logger::get().log_error(CP_HERE) << "empty chunk encountered";
				}
			}

			buffer::const_iterator it = _buf->begin();
			auto chk = _chks.begin();
			size_t bytesbefore = 0;
			linebreak_analyzer linebreaks;
			while (it != _buf->end() && chk != _chks.end()) {
				codepoint cp;
				if (!_encoding->next_codepoint(it, _buf->end(), cp)) {
					cp = 0;
				}
				linebreaks.put(cp);
				while (chk != _chks.end()) {
					size_t chkend = chk->num_bytes + bytesbefore;
					if (it.get_position() < chkend) {
						break;
					}
					if (it.get_position() > chkend) {
						error = true;
						logger::get().log_error(CP_HERE) <<
							"codepoint boundary mismatch at byte " << chkend <<
							": expected " << it.get_position();
					}
					bytesbefore = chkend;
					++chk;
				}
			}
			if (it != _buf->end() || chk != _chks.end()) {
				error = true;
				if (it != _buf->end()) {
					logger::get().log_error(CP_HERE) <<
						"document length mismatch: chunks ended abruptly at byte " << it.get_position() <<
						", expected " << _buf->length();
				} else {
					logger::get().log_error(CP_HERE) <<
						"document length mismatch: got " <<
						(_chks.empty() ? 0 : _chks.root()->synth_data.total_bytes) << " bytes, expected " <<
						_buf->length() << " bytes";
				}
			}
			linebreaks.finish();

			auto explineit = linebreaks.result().begin();
			auto gotlineit = _lbs.begin();
			size_t line = 0;
			while (explineit != linebreaks.result().end() && gotlineit != _lbs.end()) {
				if (gotlineit->nonbreak_chars != explineit->nonbreak_chars) {
					error = true;
					logger::get().log_error(CP_HERE) <<
						"line length mismatch at line " << line << ", starting at codepoint " <<
						_lbs.get_beginning_codepoint_of(gotlineit) << ": expected " << explineit->nonbreak_chars <<
						", got " << gotlineit->nonbreak_chars;
				}
				if (gotlineit->ending != explineit->ending) {
					error = true;
					logger::get().log_error(CP_HERE) <<
						"linebreak type mismatch at line " << line << ", starting at codepoint " <<
						_lbs.get_beginning_codepoint_of(gotlineit) << ": expected " <<
						static_cast<int>(explineit->ending) << ", got " << static_cast<int>(gotlineit->ending);
				}
				++explineit;
				++gotlineit;
				++line;
			}
			if (_lbs.num_linebreaks() + 1 != linebreaks.result().size()) {
				error = true;
				logger::get().log_error(CP_HERE) <<
					"number of lines mismatch: got " << _lbs.num_linebreaks() + 1 <<
					", expected " << linebreaks.result().size();
			}

			return !error;
		}


		/// Invoked when an edit has been made to the underlying \ref buffer, after this \ref interpretation has
		/// finished updating.
		info_event<buffer::end_edit_info> end_edit_interpret;
		// TODO visual_changed event
	protected:
		tree_type _chks; ///< Chunks used to speed up navigation.
		text_theme_data _theme; ///< Theme of the text.
		linebreak_registry _lbs; ///< Records all linebreaks.

		const std::shared_ptr<buffer> _buf; ///< The underlying \ref buffer.
		info_event<buffer::begin_edit_info>::token _begin_edit_tok; ///< Used to listen to \ref buffer::begin_edit.
		info_event<buffer::end_edit_info>::token _end_edit_tok; ///< Used to listen to \ref buffer::end_edit.
		line_ending _line_ending = line_ending::n; ///< The default line ending for this \ref interpretation.
		const buffer_encoding *const _encoding = nullptr; ///< The encoding used to interpret the \ref buffer.

		/// Used to find the number of bytes before a specified codepoint.
		struct _codepoint_pos_converter {
			/// The underlying \ref sum_synthesizer::index_finder.
			using finder = sum_synthesizer::index_finder<node_data::num_codepoints_property>;
			/// Interface for \ref binary_tree::find_custom.
			int select_find(const node_type &n, size_t &c) {
				return finder::template select_find<node_data::num_bytes_property>(n, c, total_bytes);
			}
			size_t total_bytes = 0; ///< Records the total number of bytes before the resulting chunk.
		};
		/// Used to find the chunk in which the i-th byte is located.
		template <template <typename T> typename Cmp> using _byte_finder =
			sum_synthesizer::index_finder<node_data::num_bytes_property, false, Cmp<size_t>>;
		/// Used to find the chunk in which the i-th byte is located, and the number of codepoints before that chunk.
		template <template <typename T> typename Cmp = std::less> struct _byte_pos_converter {
			/// The underlying \ref sum_synthesizer::index_finder.
			using finder = _byte_finder<Cmp>;
			/// Interface for \ref binary_tree::find_custom.
			int select_find(const node_type &n, size_t &c) {
				return finder::template select_find<node_data::num_codepoints_property>(n, c, total_codepoints);
			}
			size_t total_codepoints = 0; ///< Records the total number of codepoints before the resulting chunk.
		};


		/// Used to store precomputed byte positions of a modification.
		struct _precomp_mod_positions {
			/// Default constructor.
			_precomp_mod_positions() = default;
			/// Initializes all fields of this struct.
			_precomp_mod_positions(size_t beg, size_t len) : begin(beg), length(len) {
			}

			size_t
				begin = 0, ///< Position of the first removed byte.
				length = 0; ///< The number of consecutive removed bytes.
		};

		/// Computes byte positions of the removed contents of an edit for a whole \ref caret_set, when the user
		/// inputs a short clip of text. This function assumes that \ref caret_data::bytepos_first and
		/// \ref caret_data::bytepos_second have already been computed.
		std::vector<_precomp_mod_positions> _precomp_mod_insert(const caret_set &carets) {
			std::vector<_precomp_mod_positions> res;
			for (const caret_set::entry &et : carets.carets) {
				size_t first = et.second.bytepos_first, second = et.second.bytepos_second;
				if (first > second) {
					std::swap(first, second);
				}
				res.emplace_back(first, second - first);
			}
			return res;
		}
		/// Similar to \ref _precomp_mod_insert(), but for when the user presses the `backspace' key.
		std::vector<_precomp_mod_positions> _precomp_mod_backspace(const caret_set &carets) {
			std::vector<_precomp_mod_positions> res;
			character_position_converter conv(*this);
			for (const caret_set::entry &et : carets.carets) {
				size_t first = et.second.bytepos_first, second = et.second.bytepos_second;
				if (et.first.first == et.first.second) {
					if (et.first.first > 0) {
						first = conv.character_to_byte(et.first.first - 1);
					}
				} else {
					if (first > second) {
						std::swap(first, second);
					}
				}
				res.emplace_back(first, second - first);
			}
			return res;
		}
		/// Similar to \ref _precomp_mod_insert(), but for when the user presses the `delete' key.
		std::vector<_precomp_mod_positions> _precomp_mod_delete(const caret_set &carets) {
			std::vector<_precomp_mod_positions> res;
			character_position_converter conv(*this);
			for (const caret_set::entry &et : carets.carets) {
				size_t first = et.second.bytepos_first, second = et.second.bytepos_second;
				if (et.first.first == et.first.second) {
					if (et.first.first < get_linebreaks().num_chars()) {
						second = conv.character_to_byte(et.first.first + 1);
					}
				} else {
					if (first > second) {
						std::swap(first, second);
					}
				}
				res.emplace_back(first, second - first);
			}
			return res;
		}


		/// Used by \ref _post_edit_fixup() to look for valid cached codepoint boundaries.
		std::vector<buffer::modification_position>::const_iterator _advance_target_chunk_iterator(
			tree_type::const_iterator &it, size_t &firstbyte, size_t &firstcp,
			std::vector<buffer::modification_position>::const_iterator modit,
			std::vector<buffer::modification_position>::const_iterator modend
		) {
			for (bool done = false; !done; ) {
				firstbyte += it->num_bytes;
				firstcp += it->num_codepoints;
				++it;
				done = true;
				while (modit != modend && firstbyte > modit->position) {
					// handle other modifications after the current one but before the boundary
					if (firstbyte >= modit->position + modit->removed_range) {
						// completely before the boundary, add offset
						firstbyte += modit->added_range - modit->removed_range;
					} else { // boundary is removed, look for the next one
						done = false;
						break;
					}
					++modit;
				}
			}
			return modit;
		}
		/// Attempts to merge the given node with its neighboring chunks, if they're too small. This operation won't
		/// invalidate the given iterator.
		///
		/// \param it Iterator to the target node.
		/// \param merged If the nodes are merged, this will be filled with data of the deleted node.
		/// \return Whether the nodes have been merged.
		/// \todo Find a better merging strategy.
		bool _try_merge_small_chunks(tree_type::const_iterator it, chunk_data &merged) {
			if (it != _chks.end() && it != _chks.begin()) {
				auto prev = it;
				--prev;
				if (it->num_codepoints + prev->num_codepoints < maximum_codepoints_per_chunk) { // merge!
					merged = *prev;
					_chks.erase(prev);
					{
						auto mod = _chks.get_modifier_for(it.get_node());
						mod->num_bytes += merged.num_bytes;
						mod->num_codepoints += merged.num_codepoints;
					}
					return true;
				}
			}
			return false;
		}
#define CP_DEBUG_LOG_POST_EDIT_FIXUP 0
#if CP_DEBUG_LOG_POST_EDIT_FIXUP
		/// Logs the given message if \ref _debug_log_post_edit_fixup_enabled is \p true.
		template <typename ...Args> inline static void _debug_log_post_edit_fixup(Args &&...args) {
			logger::get().log_verbose(CP_HERE, std::forward<Args>(args)...);
		}
#else
		/// Logging is disabled. Does nothing.
		template <typename ...Args> inline static void _debug_log_post_edit_fixup(Args&&...) {
		}
#endif
#undef CP_DEBUG_LOG_POST_EDIT_FIXUP
		/// Adjusts \ref _chks and \ref _lbs after an edit has been made.
		void _post_edit_fixup(buffer::end_edit_info &info) {
			_debug_log_post_edit_fixup("starting post-edit fixup");
			size_t
				lastbyte = 0, // number of bytes before lastchk
				lastcp = 0; // number of codepoints before lastchk
			tree_type::const_iterator lastchk = _chks.begin();
			for (auto modit = info.positions.begin(); modit != info.positions.end(); ) {
				_debug_log_post_edit_fixup(
					"  modification ", modit->position, " -", modit->removed_range, " +", modit->added_range
				);
				size_t
					modaddend = modit->position + modit->added_range,
					modremend = modit->position + modit->removed_range;
				size_t
					cpoffset = _encoding->get_maximum_codepoint_length() - 1,
					// starting from where the interpretation may have been changed
					suspect = std::max(modit->position, cpoffset) - cpoffset;
				if (lastchk != _chks.end() && suspect > lastbyte + lastchk->num_bytes) {
					// skip to the desired chunk
					_debug_log_post_edit_fixup("    adjusting begin position");
					size_t chkpos = suspect;
					// if a modification starts at the beginning of a chunk, then the codepoint barrier is invalid
					_byte_pos_converter<std::less_equal> finder;
					lastchk = _chks.find_custom(finder, chkpos);
					lastcp = finder.total_codepoints;
					lastbyte = suspect - chkpos;
				}
				// otherwise start decoding from last position (start of chunk)
				size_t cppos = lastcp;
				buffer::const_iterator bit = _buf->at(lastbyte);
				_debug_log_post_edit_fixup("    starting from codepoint ", lastcp, ", byte ", lastbyte);

				linebreak_analyzer lines;
				codepoint cp = 0;
				// find first modified codepoint
				for (; bit != _buf->end() && bit.get_position() <= modit->position; ++cppos) {
					if (!_encoding->next_codepoint(bit, _buf->end(), cp)) {
						cp = 0;
					}
					lines.put(cp);
				}
				std::vector<chunk_data> chks;
				size_t
					firstcp = lastcp, // index of the first decoded codepoint
					splitcp = lastcp + maximum_codepoints_per_chunk;
				for (; bit != _buf->end() && bit.get_position() < modaddend; ++cppos) { // decode new content
					if (cppos >= splitcp) { // break chunk
						size_t bytepos = bit.get_position();
						chks.emplace_back(bytepos - lastbyte, cppos - lastcp);
						lastbyte = bytepos;
						lastcp = cppos;
						splitcp = lastcp + maximum_codepoints_per_chunk;
					}
					if (!_encoding->next_codepoint(bit, _buf->end(), cp)) {
						cp = 0;
					}
					lines.put(cp);
				}

				// find the next old codepoint boundary
				size_t tgckpos = modremend; // position of the next old codepoint boundary
				_byte_pos_converter posfinder;
				tree_type::const_iterator chkit = _chks.find_custom(posfinder, tgckpos);
				size_t
					endcp = posfinder.total_codepoints, // (one after) the last codepoint to remove from old _lbs
					tgpos = modaddend - tgckpos; // adjust position to that after the edit
				auto nextmodit = modit;
				++nextmodit;
				if (tgckpos > 0) { // advance if the removed region does not end before the first byte
					assert_true_logical(chkit != _chks.end(), "invalid modification position");
					nextmodit = _advance_target_chunk_iterator(chkit, tgpos, endcp, nextmodit, info.positions.end());
					_debug_log_post_edit_fixup("    removed region ends in the middle of a chunk");
				}
				_debug_log_post_edit_fixup("    targeting byte ", tgpos, ", codepoint ", endcp);
				while (true) {
					// decode till the end of chunk
					for (; bit != _buf->end() && bit.get_position() < tgpos; ++cppos) {
						if (cppos >= splitcp) { // break chunk
							size_t bytepos = bit.get_position();
							chks.emplace_back(bytepos - lastbyte, cppos - lastcp);
							lastbyte = bytepos;
							lastcp = cppos;
							splitcp = lastcp + maximum_codepoints_per_chunk;
						}
						if (!_encoding->next_codepoint(bit, _buf->end(), cp)) {
							cp = 0;
						}
						lines.put(cp);
					}
					if (bit.get_position() == tgpos) { // boundary found, stop
						break;
					}
					assert_true_logical(chkit != _chks.end(), "faulty decoder");
					nextmodit = _advance_target_chunk_iterator(chkit, tgpos, endcp, nextmodit, info.positions.end());
					_debug_log_post_edit_fixup("      no luck, targeting byte ", tgpos, ", codepoint ", endcp);
				}
				// handle last chunk
				lines.finish();
				size_t bytepos = bit.get_position();
				if (bytepos != lastbyte) {
					chks.emplace_back(bytepos - lastbyte, cppos - lastcp);
					lastbyte = bytepos;
					lastcp = cppos;
				} else {
					assert_true_logical(cppos == lastcp, "shouldn't happen");
				}
				_debug_log_post_edit_fixup("    parse finishes at byte ", lastbyte, ", codepoint ", lastcp);

				// apply changes
				// chunks
				_chks.erase(lastchk, chkit);
				_chks.insert_range_before_move(chkit, chks.begin(), chks.end());
				chunk_data cd;
				if (_try_merge_small_chunks(chkit, cd)) {
					lastbyte -= cd.num_bytes;
					lastcp -= cd.num_codepoints;
				}
				lastchk = chkit;
				// lines
				_lbs.erase_codepoints(firstcp, endcp);
				_lbs.insert_codepoints(firstcp, lines.result());

				modit = nextmodit;
			}
			end_edit_interpret.invoke(info);
			_debug_log_post_edit_fixup("successfully finished post-edit fixup");
		}

		/// Called when an edit is about to be made to \ref _buf.
		///
		/// \todo Prepare \ref _theme for fixup.
		void _on_begin_edit(buffer::begin_edit_info&) {

		}
		/// Called when an edit has been made to \ref _buf.
		void _on_end_edit(buffer::end_edit_info &info) {
			_post_edit_fixup(info);
		}
	};
}
