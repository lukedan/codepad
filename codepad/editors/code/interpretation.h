#pragma once

/// \file
/// Implementation of classes used to interpret a \ref codepad::editor::buffer.

#include <map>

#include "../../core/encodings.h"
#include "../buffer.h"
#include "linebreak_registry.h"
#include "theme.h"
#include "caret_set.h"

namespace codepad::editor::code {
	/// Abstract class used to represent an encoding used to interpret a \ref buffer.
	class buffer_encoding {
	public:
		/// Virtual destructor.
		virtual ~buffer_encoding() = default;

		/// Returns the name of this encoding.
		virtual str_t get_name() const = 0;

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
		str_t get_name() const override {
			return Encoding::get_name();
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
			return _map.try_emplace(enc->get_name(), std::move(enc)).second;
		}
		/// Registers a built-in encoding.
		///
		/// \return Whether the registration was successful (i.e., no duplicate names were found).
		template <typename Encoding> bool register_builtin_encoding() {
			return register_encoding(std::make_unique<predefined_buffer_encoding<Encoding>>());
		}
		/// Returns the encoding with the given name, or \p nullptr if no such encoding is found.
		const buffer_encoding *get_encoding(const str_t &name) const {
			auto it = _map.find(name);
			if (it != _map.end()) {
				return it->second.get();
			}
			return nullptr;
		}
		/// Returns the full list of registered encodings.
		const std::map<str_t, std::unique_ptr<buffer_encoding>> &get_all_encodings() const {
			return _map;
		}

		/// Returns the global \ref encoding_manager object.
		static encoding_manager &get();
	protected:
		/// The mapping between encoding names and \ref buffer_encoding instances.
		std::map<str_t, std::unique_ptr<buffer_encoding>> _map;
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
					for (size_t i = 0; i < get_linebreak_length(_lbit->ending); ++i) {
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
				for (; _byteit.get_position() <= pos; ++_chkcp) {
					_interp.get_encoding()->next_codepoint(_byteit, _interp.get_buffer()->end());
				}
				return _firstcp + _chkcp - 1;
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
		interpretation(const std::shared_ptr<buffer> &buf, const buffer_encoding &encoding) :
			_buf(buf), _encoding(&encoding) {
			_begin_edit_tok = _buf->begin_edit += [this](buffer::begin_edit_info &info) {
				_on_begin_edit(info);
			};
			_end_edit_tok = _buf->end_edit += [this](buffer::end_edit_info &info) {
				_on_end_edit(info);
			};

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
			chunks.emplace_back(_buf->length() - chkbegbytes, curcp - chkbegcps);
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
			return character_iterator(at_codepoint(cp), colinfo.line_iterator, colinfo.column);
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


		/// Invoked when an edit has been made to the underlying \ref buffer, after this \ref interpretation has
		/// finished updating.
		event<buffer::end_edit_info> end_edit_interpret;
	protected:
		tree_type _chks; ///< Chunks used to speed up navigation.
		text_theme_data _theme; ///< Theme of the text.
		linebreak_registry _lbs; ///< Records all linebreaks.

		const std::shared_ptr<buffer> _buf; ///< The underlying \ref buffer.
		event<buffer::begin_edit_info>::token _begin_edit_tok; ///< Used to listen to \ref buffer::begin_edit.
		event<buffer::end_edit_info>::token _end_edit_tok; ///< Used to listen to \ref buffer::end_edit.
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
		using _byte_finder = sum_synthesizer::index_finder<node_data::num_bytes_property>;
		/// Used to find the chunk in which the i-th byte is located, and the number of codepoints before that chunk.
		struct _byte_pos_converter {
			/// The underlying \ref sum_synthesizer::index_finder.
			using finder = _byte_finder;
			/// Interface for \ref binary_tree::find_custom.
			int select_find(const node_type &n, size_t &c) {
				return finder::template select_find<node_data::num_bytes_property>(n, c, total_codepoints);
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


		/// Called when an edit is about to be made to \ref _buf.
		///
		/// \todo Prepare \ref _theme for fixup.
		void _on_begin_edit(buffer::begin_edit_info&) {

		}
		/// Called when an edit has been made to \ref _buf.
		void _on_end_edit(buffer::end_edit_info &info) {
			size_t
				lastbyte = 0, // index past the last byte of the last refreshed chunk
				lastcp = 0; // index of first codepoint past the last refreshed chunk
			tree_type::const_iterator lastchk = _chks.end();
			for (const buffer::modification_position &mod : info.positions) {
				size_t modend = mod.position + mod.added_range;
				if (modend < lastbyte) { // no need to re-decode
					continue;
				}
				buffer::const_iterator bit;
				size_t cppos = lastcp;
				if (mod.position > lastbyte) { // start decoding from this chunk
					size_t chkpos = mod.position;
					lastchk = _chks.find_custom(_byte_finder(), chkpos);
					size_t chkbegcp = 0;
					sum_synthesizer::sum_before<node_data::num_codepoints_property>(lastchk, chkbegcp);
					lastcp = cppos = chkbegcp;
					lastbyte = mod.position - chkpos;
				} // otherwise start decoding from last position (start of chunk)
				bit = _buf->at(lastbyte);

				codepoint cp = 0;
				// find first modified codepoint
				for (; bit != _buf->end() && bit.get_position() <= mod.position; ++cppos) {
					if (!_encoding->next_codepoint(bit, _buf->end(), cp)) {
						cp = 0;
					}
				}
				size_t
					firstmodcp = cppos - 1, // index of the first modified codepoint
					splitcp = lastcp + maximum_codepoints_per_chunk;
				std::vector<chunk_data> chks;
				linebreak_analyzer lines;
				for (; bit != _buf->end() && bit.get_position() < modend; ++cppos) { // decode new content
					if (cppos >= splitcp) { // break chunk
						size_t bytepos = bit.get_position();
						chks.emplace_back(bytepos - lastbyte, cppos - lastcp);
						lastbyte = bytepos;
						lastcp = cppos;
						splitcp = lastcp + maximum_codepoints_per_chunk;
					}
					lines.put(cp);
					if (!_encoding->next_codepoint(bit, _buf->end(), cp)) {
						cp = 0;
					}
				}

				// find the next old codepoint boundary
				size_t
					tgpos = mod.position + mod.removed_range, // position of the next old codepoint boundary
					tgckpos = tgpos;
				tree_type::const_iterator chkit = _chks.find_custom(_byte_finder(), tgckpos);
				tgpos += mod.added_range - mod.removed_range - tgckpos; // adjust position to that after the edit
				if (tgckpos > 0) {
					assert_true_logical(chkit != _chks.end(), "invalid modification position");
					tgpos += chkit->num_bytes;
					++chkit;
				}
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
						lines.put(cp);
						if (!_encoding->next_codepoint(bit, _buf->end(), cp)) {
							cp = 0;
						}
					}
					if (bit.get_position() == tgpos) { // boundary found, stop
						break;
					}
					assert_true_logical(chkit != _chks.end(), "faulty decoder");
					tgpos += chkit->num_bytes;
					++chkit;
				}
				// handle last chunk
				lines.finish_with(cp);
				size_t bytepos = bit.get_position();
				chks.emplace_back(bytepos - lastbyte, cppos - lastcp);
				lastbyte = bytepos;
				lastcp = cppos;

				// apply changes
				// chunks
				_chks.erase(lastchk, chkit);
				_chks.insert_range_before_move(chkit, chks.begin(), chks.end());
				lastchk = chkit;
				// lines
				_lbs.erase_codepoints(firstmodcp, lastcp);
				_lbs.insert_codepoints(firstmodcp, lines.result());
			}
			end_edit_interpret.invoke(info);
		}
	};
}
