// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of classes used to interpret a \ref codepad::editors::buffer.

#include <map>
#include <list>
#include <deque>
#include <optional>

#include <codepad/core/encodings.h>
#include <codepad/core/red_black_tree.h>

#include "codepad/editors/buffer.h"
#include "codepad/editors/decoration.h"
#include "linebreak_registry.h"
#include "theme.h"
#include "caret_set.h"

namespace codepad::editors {
	class buffer_manager;
}

namespace codepad::editors::code {
	/// Abstract class used to represent an encoding used to interpret a \ref buffer.
	class buffer_encoding {
	public:
		/// Virtual destructor.
		virtual ~buffer_encoding() = default;

		/// Returns the name of this encoding.
		virtual std::u8string_view get_name() const = 0;
		/// Returns the maximum possible length of a codepoint, in bytes.
		virtual std::size_t get_maximum_codepoint_length() const = 0;

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

		/// \overload
		virtual bool next_codepoint(const std::byte *&it, const std::byte *end, codepoint&) const = 0;
		/// \overload
		virtual bool next_codepoint(const std::byte *&it, const std::byte *end) const = 0;

		/// Returns the encoded representation of the given codepoint.
		virtual byte_string encode_codepoint(codepoint) const = 0;
	};
	/// Encoding used to interpret a \ref buffer, based on a class in the \ref codepad::encodings namespace.
	template <typename Encoding> class predefined_buffer_encoding : public buffer_encoding {
		/// Calls \p get_name() in \p Encoding.
		std::u8string_view get_name() const override {
			return Encoding::get_name();
		}
		/// Calls \p get_maximum_codepoint_length() in \p Encoding.
		std::size_t get_maximum_codepoint_length() const override {
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

		/// Calls \p next_codepoint() in \p Encoding.
		bool next_codepoint(const std::byte *&it, const std::byte *end, codepoint &res) const override {
			return Encoding::next_codepoint(it, end, res);
		}
		/// Calls \p next_codepoint() in \p Encoding.
		bool next_codepoint(const std::byte *&it, const std::byte *end) const override {
			return Encoding::next_codepoint(it, end);
		}

		/// Calls \p encode_codepoint() in \p Encoding.
		byte_string encode_codepoint(codepoint cp) const override {
			return Encoding::encode_codepoint(cp);
		}
	};

	/// Stores the list of available encodings.
	class encoding_registry {
	public:
		/// Registers a series of built-in encodings, and sets the default encoding to UTF-8.
		encoding_registry() {
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
		bool register_encoding(std::unique_ptr<buffer_encoding> enc) {
			return _map.try_emplace(std::u8string(enc->get_name()), std::move(enc)).second;
		}
		/// Registers a built-in encoding.
		///
		/// \return Whether the registration was successful (i.e., no duplicate names were found).
		template <typename Encoding> bool register_builtin_encoding() {
			return register_encoding(std::make_unique<predefined_buffer_encoding<Encoding>>());
		}
		/// Returns the encoding with the given name, or \p nullptr if no such encoding is found.
		const buffer_encoding *get_encoding(std::u8string_view name) const {
			auto it = _map.find(name);
			if (it != _map.end()) {
				return it->second.get();
			}
			return nullptr;
		}
		/// Returns the full list of registered encodings.
		const std::map<std::u8string, std::unique_ptr<buffer_encoding>, std::less<>> &get_all_encodings() const {
			return _map;
		}
	protected:
		/// The mapping between encoding names and \ref buffer_encoding instances.
		std::map<std::u8string, std::unique_ptr<buffer_encoding>, std::less<>> _map;
		const buffer_encoding *_default = nullptr; ///< The default encoding.
	};

	/// A tooltip returned by \ref tooltip_provider::request_tooltip(). A tooltip contains one \ref ui::element that
	/// is added to the popup window containing all tooltips, and is disposed of when the tooltip is closed.
	class tooltip {
	public:
		/// Default virtual destructor.
		virtual ~tooltip() = default;

		/// Returns the \ref ui::element associated with this tooltip. This should return the same value throughout
		/// the lifespan of the tooltip.
		[[nodiscard]] virtual ui::element *get_element() = 0;
	};
	/// A simple tooltip that contains a \ref ui::element that is returned in \ref tooltip::get_element().
	class simple_tooltip : public tooltip {
	public:
		/// Initializes \ref _element.
		explicit simple_tooltip(ui::element &e) : _element(&e) {
		}

		/// Returns \ref _element.
		ui::element *get_element() override {
			return _element;
		}
	protected:
		ui::element *_element = nullptr; ///< The \ref ui::element that is displayed.
	};
	/// A provider for tooltips.
	class tooltip_provider {
	public:
		/// Default virtual destructor.
		virtual ~tooltip_provider() = default;

		/// Requests a tooltip from this provider at the given text position.
		[[nodiscard]] virtual std::unique_ptr<tooltip> request_tooltip(std::size_t) = 0;
	};

	/// Interprets a \ref buffer using a given encoding. This struct stores information that can be used to determine
	/// certain boundaries between codepoints.
	class interpretation : public std::enable_shared_from_this<interpretation> {
		friend buffer_manager;
	public:
		/// Maximum number of codepoints in a chunk.
		constexpr static std::size_t maximum_codepoints_per_chunk = 1000;

		/// Information about a consecutive sequence of codepoints in the buffer.
		struct chunk_data {
			/// Default constructor.
			chunk_data() = default;
			/// Initializes all fields of this struct.
			chunk_data(std::size_t bytes, std::size_t cps) : num_bytes(bytes), num_codepoints(cps) {
			}

			std::size_t
				num_bytes = 0, ///< The number of bytes in this chunk.
				/// The number of codepoints, including both valid and invalid ones, in this chunk.
				num_codepoints = 0;
			red_black_tree::color color = red_black_tree::color::red; ///< The color of this node.
		};
		/// Contains additional synthesized data of a node.
		struct node_data {
			/// A node of the tree.
			using node = binary_tree_node<chunk_data, node_data>;

			std::size_t
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
		using tree_type = red_black_tree::tree<
			chunk_data, red_black_tree::member_red_black_access<&chunk_data::color>, node_data
		>;
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
				if (_cur != _interp->get_buffer().end()) {
					_valid = _interp->get_encoding()->next_codepoint(_next, _interp->get_buffer().end(), _cp);
					return true;
				}
				_cp = 0;
				_valid = false;
				return false;
			}

			/// Returns the current codepoint.
			[[nodiscard]] codepoint get_codepoint() const {
				return _cp;
			}
			/// Returns whether the current codepoint is valid.
			[[nodiscard]] bool is_codepoint_valid() const {
				return _valid;
			}
			/// Returns whether the iterator is past the end of the \ref buffer.
			[[nodiscard]] bool ended() const {
				return _interp == nullptr || _cur == _interp->get_buffer().end();
			}
			/// Returns the underlying \ref buffer::const_iterator pointing to the first byte of the current
			/// codepoint.
			[[nodiscard]] const buffer::const_iterator &get_raw() const {
				return _cur;
			}

			/// Returns the associated \ref interpretation.
			[[nodiscard]] const interpretation &get_interpretation() const {
				return *_interp;
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

				if (_cur != _interp->get_buffer().end()) {
					_valid = _interp->get_encoding()->next_codepoint(_next, _interp->get_buffer().end(), _cp);
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
					for (std::size_t i = 0; i < get_line_ending_length(_lbit->ending); ++i) {
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
			[[nodiscard]] bool is_linebreak() const {
				return _col == _lbit->nonbreak_chars;
			}
			/// Returns the type of the current line's linebreak.
			[[nodiscard]] ui::line_ending get_linebreak() const {
				return _lbit->ending;
			}
			/// Returns the column where this iterator's at.
			[[nodiscard]] std::size_t get_column() const {
				return _col;
			}
			/// Returns the underlying \ref codepoint_iterator.
			[[nodiscard]] const codepoint_iterator &codepoint() const {
				return _cpit;
			}
		protected:
			codepoint_iterator _cpit; ///< Iterates through the actual codepoints.
			linebreak_registry::iterator _lbit; ///< Iterates through linebreaks.
			std::size_t _col = 0; ///< The column where \ref _cpit is at.

			/// Used by \ref interpretation to create an iterator pointing to a specific character.
			character_iterator(
				const codepoint_iterator &cpit, const linebreak_registry::iterator lbit, std::size_t col
			) : _cpit(cpit), _lbit(lbit), _col(col) {
			}
		};

		/// Contains information for the \ref modification_decoded event, with additional information about the
		/// codepoints that are affected. See documentation about the event for more details.
		struct modification_decoded_info {
			/// Initializes all fields of this struct.
			modification_decoded_info(
				linebreak_registry::line_column_info start_lc, linebreak_registry::line_column_info past_end_lc,
				std::size_t start_char, std::size_t past_end_char,
				std::size_t start_cp, std::size_t past_last_erased_cp, std::size_t past_last_inserted_cp,
				std::size_t start_b, std::size_t past_end_b, buffer::end_modification_info &info
			) :
				start_line_column(start_lc), past_end_line_column(past_end_lc),
				start_character(start_char), past_end_character(past_end_char),
				start_codepoint(start_cp),
				past_last_erased_codepoint(past_last_erased_cp),
				past_last_inserted_codepoint(past_last_inserted_cp),
				start_byte(start_b), past_end_byte(past_end_b), buffer_info(info) {
			}

			const linebreak_registry::line_column_info
				start_line_column, ///< Line and column information of \ref start_codepoint.
				past_end_line_column; ///< Line and column information of \ref past_last_erased_codepoint.
			const std::size_t
				/// The character that \ref start_codepoint belongs to. Note that this may not be the first character
				/// affected by this modification due to CRLF merging and splitting. Generally
				/// \ref end_modification_info::start_character would be more useful.
				start_character = 0,
				/// The character that \ref past_last_erased_codepoint belongs to. Note that this may not be the
				/// first character affected by this modification due to CRLF merging and splitting. Generally a
				/// combination of \ref end_modification_info::start_character and
				/// \ref end_modification_info::removed_characters would be more useful.
				past_end_character = 0;
			const std::size_t
				/// The first codepoint that has been affected by this modification. This index should be valid both
				/// before and after the modification.
				start_codepoint = 0,
				/// One past the last codepoint that has been affected by this modification. This index is obtained
				/// **before** the modification.
				past_last_erased_codepoint = 0,
				/// One past the last codepoint that has been affected by this modification. This index is obtained
				/// **after** the modification.
				past_last_inserted_codepoint = 0;
			const std::size_t
				/// The byte index corresponding to \ref start_codepoint. This should be valid both before and after
				/// the modification.
				start_byte = 0,
				/// The byte index corresponding to \ref past_last_erased_codepoint. This index is obtained
				/// **after** the modification. This is also guaranteed to be after the range of inserted bytes, so
				/// the corresponding byte index after the modification (i.e., corresponding to
				/// \ref past_last_erased_codepoint) can be trivially computed using data from \ref buffer_info.
				past_end_byte = 0;
			/// Information about modification of the underlying \ref buffer.
			buffer::end_modification_info &buffer_info;
		};
		/// Contains information for the \ref end_modification event, with additional information about the
		/// codepoints that are affected.
		///
		/// The \ref start_character, \ref removed_characters, and \ref inserted_characters fields indicate the
		/// character range that may have been modified.
		struct end_modification_info {
			/// Initializes all fields of this struct.
			end_modification_info(
				std::size_t start_char, std::size_t rem_chars, std::size_t insert_chars,
				std::size_t erase_end_ln, std::size_t erase_end_col, buffer::end_modification_info &info
			) :
				start_character(start_char), removed_characters(rem_chars), inserted_characters(insert_chars),
				erase_end_line(erase_end_ln), erase_end_column(erase_end_col), buffer_info(info) {
			}

			const std::size_t
				/// The first character that may have been modified. This differs from
				/// \ref modification_decoded_info::start_character in that it accounts for CRLF splitting/merging.
				start_character = 0,
				/// The number of characters that have been erased or modified by the erase operation. This differs
				/// from \ref modification_decoded_info::past_end_character in that it accounts for CRLF
				/// splitting/merging.
				removed_characters = 0,
				/// The number of characters that have been inserted or modified by the insert operation.
				inserted_characters = 0;
			const std::size_t
				/// The line index of character past the last erased character, obtained before the modification.
				/// Line/column information of most other character positions can be obtained directly from the
				/// \ref interpretation.
				erase_end_line = 0,
				/// The column index of character past the last erased character, obtained before the modification.
				/// Line/column information of most other character positions can be obtained directly from the
				/// \ref interpretation.
				erase_end_column = 0;
			/// Information about modification of the underlying \ref buffer.
			buffer::end_modification_info &buffer_info;
		};
		/// Additional information about an edit.
		struct end_edit_info {
			/// Initializes all fields of this struct.
			end_edit_info(buffer::edit_positions edit_positions, buffer::end_edit_info &buf_info) :
				character_edit_positions(std::move(edit_positions)), buffer_info(buf_info) {
			}

			/// Aggregates \ref end_modification_info::start_character,
			/// \ref end_modification_info::removed_characters, \ref end_modification_info::inserted_characters for
			/// all modifications in this edit. Note that these ranges may overlap.
			buffer::edit_positions character_edit_positions;
			/// Original modification information from the underlying \ref buffer.
			buffer::end_edit_info &buffer_info;
		};
		
		/// Indicates what aspects of the document are affected by an appearance change.
		enum class appearance_change_type {
			visual_only, ///< Only the visual of this document has changed - as opposed to \ref layout_and_visual.
			/// The layout of this document may have also changed, due to e.g. gizmos being added, font stretch being
			/// changed, etc.
			layout_and_visual
		};
		/// Information about the change of a document's apperance.
		struct appearance_changed_info {
			/// Initializes \ref type.
			explicit appearance_changed_info(appearance_change_type t) : type(t) {
			}

			/// Indicates what aspects of this document's appearance may have been affected.
			const appearance_change_type type = appearance_change_type::visual_only;
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
				_chunk_iter = _interp._chunks.begin();
				_byte_iter = _interp.get_buffer().begin();
				_firstcp = _firstbyte = _chunk_codepoint_offset = _codepoint_start = 0;
			}

			/// Returns the position of the first byte of the given codepoint. This should not be used mixedly with
			/// \ref byte_to_codepoint().
			[[nodiscard]] std::size_t codepoint_to_byte(std::size_t);
			/// Returns the position of the codepoint that contains the given byte. This should not be used mixedly
			/// with \ref codepoint_to_byte().
			///
			/// \return The first element is the codepoint index, and the second element is the index of the first
			///         byte of that codepoint
			[[nodiscard]] std::pair<std::size_t, std::size_t> byte_to_codepoint(std::size_t);

			/// Returns an iterator into the chunk that the last query was in.
			[[nodiscard]] const tree_type::const_iterator &get_chunk_iterator() const {
				return _chunk_iter;
			}
			/// Returns an current iterator into the \ref buffer after the last query. For codepoint-to-byte queries,
			/// this will exactly be at the previously queried position; for byte-to-codepoint queries, this will be
			/// one codepoint ahead iff the previously queried byte position was not the first byte of a codepoint.
			[[nodiscard]] const buffer::const_iterator &get_buffer_iterator() const {
				return _byte_iter;
			}
			/// Returns the number of codepoints before the chunk returned by \ref get_chunk_iterator().
			[[nodiscard]] std::size_t get_chunk_codepoint_position() const {
				return _firstcp;
			}
			/// Returns the number of bytes before the chunk returned by \ref get_chunk_iterator().
			[[nodiscard]] std::size_t get_chunk_byte_position() const {
				return _firstbyte;
			}
		protected:
			tree_type::const_iterator _chunk_iter; ///< Iterator to the current chunk in which the codepoint lies.
			buffer::const_iterator _byte_iter; ///< Iterator to the current byte.
			const interpretation &_interp; ///< The associated \ref interpretation.
			std::size_t
				_firstcp = 0, ///< The number of codepoints before \ref _chunk_iter.
				_firstbyte = 0, ///< The number of bytes before \ref _chunk_iter.
				_chunk_codepoint_offset = 0, ///< The codepoint position of \ref _byte_iter in this chunk.
				/// The index of the first byte of the codepoint that has just been decoded. This is only used by
				/// \ref byte_to_codepoint().
				_codepoint_start = 0;
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
			std::size_t character_to_byte(std::size_t pos) {
				return _cp2byte.codepoint_to_byte(_char2cp.character_to_codepoint(pos));
			}
			/// Returns the position of the character that contains this byte. This should not be used mixedly with
			/// \ref character_to_byte().
			std::size_t byte_to_character(std::size_t pos) {
				return _char2cp.codepoint_to_character(_cp2byte.byte_to_codepoint(pos).first);
			}
		protected:
			/// Used to convert between the positions of characters and codepoints.
			linebreak_registry::position_converter _char2cp;
			/// Used to convert between the positions of codepoints and bytes.
			codepoint_position_converter _cp2byte;
		};


		/// Returned by \ref add_tooltip_provider(), this can be used to unregister the provider.
		struct tooltip_provider_token {
			friend interpretation;
		public:
			/// Default constructor.
			tooltip_provider_token() = default;
		protected:
			using _iter_t = std::list<std::unique_ptr<tooltip_provider>>::iterator; ///< Iterator type.

			_iter_t _iter; ///< Iterator to the provider.

			/// Initializes \ref _iter.
			explicit tooltip_provider_token(_iter_t it) : _iter(it) {
			}
		};

		/// Used by the \ref decoration_provider_list to notify the \ref interpretation of its changes.
		struct interpretation_ref {
			/// Initializes \ref interp.
			explicit interpretation_ref(interpretation &i) : interp(&i) {
			}

			/// Invokes \ref appearance_changed with \ref appearance_change_type::visual_only.
			void on_list_changed() {
				interp->appearance_changed.invoke_noret(appearance_change_type::visual_only);
			}
			/// Invokes \ref appearance_changed with \ref appearance_change_type::visual_only.
			void on_element_changed() {
				interp->appearance_changed.invoke_noret(appearance_change_type::visual_only);
			}

			interpretation *interp = nullptr; ///< The \ref interpretation.
		};
		using interpretation_decoration_provider_list = decoration_provider_list<interpretation_ref>;


		/// Constructor. Sets up event handlers to reinterpret the buffer when it's changed, and performs the initial
		/// full decoding.
		interpretation(std::shared_ptr<buffer>, const buffer_encoding&);
		/// No copy construction.
		interpretation(const interpretation&) = delete;
		/// No copy assignment.
		interpretation &operator=(const interpretation&) = delete;
		/// Clears all tags and unregisters from \ref buffer events.
		~interpretation() {
			_tags.clear();
			_buf->begin_modify -= _begin_modify_tok;
			_buf->end_modify -= _end_modify_tok;
			_buf->end_edit -= _end_edit_tok;
		}

		/// Returns a \ref codepoint_iterator pointing at the beginning of this document.
		[[nodiscard]] codepoint_iterator codepoint_begin() const {
			return codepoint_iterator(get_buffer().begin(), *this);
		}
		/// Returns a \ref codepoint_iterator pointing at the specified codepoint.
		[[nodiscard]] codepoint_iterator codepoint_at(std::size_t pos) const {
			_codepoint_pos_converter finder;
			_chunks.find(finder, pos);
			codepoint_iterator res(get_buffer().at(finder.total_bytes), *this);
			for (std::size_t i = 0; i < pos; ++i) {
				res.next();
			}
			return res;
		}
		/// Returns a \ref character_iterator pointing at the specified character.
		[[nodiscard]] character_iterator character_at(std::size_t pos) const {
			auto [colinfo, cp] = _linebreaks.get_line_and_column_and_codepoint_of_char(pos);
			return character_iterator(codepoint_at(cp), colinfo.line_iterator, colinfo.position_in_line);
		}

		/// Returns the total number of codepoints in this \ref interpretation.
		std::size_t num_codepoints() const {
			return _chunks.root() == nullptr ? 0 : _chunks.root()->synth_data.total_codepoints;
		}
		/// Returns the result of \ref linebreak_registry::num_linebreaks() plus one.
		std::size_t num_lines() const {
			return _linebreaks.num_linebreaks() + 1;
		}

		/// Returns the \ref buffer that this object interprets.
		[[nodiscard]] buffer &get_buffer() const {
			return *_buf;
		}
		/// Returns the \ref buffer_encoding used by this object.
		[[nodiscard]] const buffer_encoding *get_encoding() const {
			return _encoding;
		}
		/// Returns the linebreaks in this \ref interpretation.
		[[nodiscard]] const linebreak_registry &get_linebreaks() const {
			return _linebreaks;
		}
		/// Returns the default line ending for this \ref interpretation.
		[[nodiscard]] ui::line_ending get_default_line_ending() const {
			return _line_ending;
		}
		/// Sets the default line ending for this \ref interpretation. Note that this does not affect existing text.
		void set_default_line_ending(ui::line_ending end) {
			_line_ending = end;
		}


		/// Called when the user presses `backspace' to modify the underlying \ref buffer. No modification is
		/// recorded if this operation does not affect the contents of the buffer.
		void on_backspace(caret_set &carets, ui::element *src) {
			std::vector<_precomp_mod_positions> pos = _precomp_mod_backspace(carets);
			if (pos.size() > 1 || pos[0].length > 0) {
				buffer::scoped_normal_modifier mod(*_buf, src);
				for (const _precomp_mod_positions &modpos : pos) {
					mod.get_modifier().modify(modpos.begin, modpos.length, byte_string());
				}
			}
		}
		/// Called when the user presses `delete' to modify the underlying \ref buffer. No modification is recorded
		/// if this operation does not affect the contents of the buffer.
		void on_delete(caret_set &carets, ui::element *src) {
			std::vector<_precomp_mod_positions> pos = _precomp_mod_delete(carets);
			if (pos.size() > 1 || pos[0].length > 0) {
				buffer::scoped_normal_modifier mod(*_buf, src);
				for (const _precomp_mod_positions &modpos : pos) {
					mod.get_modifier().modify(modpos.begin, modpos.length, byte_string());
				}
			}
		}
		/// Called when the user enters a short clip of text to modify the underlying \ref buffer.
		void on_insert(caret_set &carets, const byte_string &contents, ui::element *src) {
			std::vector<_precomp_mod_positions> pos = _precomp_mod_insert(carets);
			buffer::scoped_normal_modifier mod(*_buf, src);
			for (const _precomp_mod_positions &modpos : pos) {
				mod.get_modifier().modify(modpos.begin, modpos.length, contents);
			}
		}


		/// Returns the associated \ref document_theme_provider_registry. All providers will be updated via
		/// \ref document_theme_provider_registry::on_modification() automatically when the buffer is modified.
		[[nodiscard]] document_theme_provider_registry &get_theme_providers() {
			return _theme_providers;
		}
		/// \overload
		[[nodiscard]] const document_theme_provider_registry &get_theme_providers() const {
			return _theme_providers;
		}

		/// Returns \ref _decorations.
		[[nodiscard]] interpretation_decoration_provider_list &get_decoration_providers() {
			return _decorations;
		}
		/// Returns \ref _decorations.
		[[nodiscard]] const interpretation_decoration_provider_list &get_decoration_providers() const {
			return _decorations;
		}

		/// Adds a tooltip provider to this document.
		[[nodiscard]] tooltip_provider_token add_tooltip_provider(std::unique_ptr<tooltip_provider> ptr) {
			return tooltip_provider_token(_tooltip_providers.insert(_tooltip_providers.end(), std::move(ptr)));
		}
		/// Returns \ref _tooltip_providers.
		[[nodiscard]] const std::list<std::unique_ptr<tooltip_provider>> &get_tooltip_providers() const {
			return _tooltip_providers;
		}
		/// Removes the given \ref tooltip_provider.
		void remove_tooltip_provider(tooltip_provider_token tok) {
			_tooltip_providers.erase(tok._iter);
			// TODO notify open tooltips
		}


		/// Checks the integrity of this \ref interpretation by re-interpreting the underlying \ref buffer, and the
		/// underlying buffer by invoking \ref buffer::check_integrity(). Used for testing and debugging.
		///
		/// \return Indicates whether the data in \ref _linebreaks and \ref _chunks are valid and correct. Note that
		///         the underlying trees assert if they're corrupted instead of returning \p false.
		bool check_integrity() const;


		/// This event is invoked when \ref buffer::end_modify is invoked. Specifically, this is invoked after the
		/// new contents have been decoded, but before \ref _linebreaks and \ref _chunks are updated. **This means
		/// that they'll be out-of-sync with the underlying buffer and lag behind**. This can be useful to obtain
		/// information about the text that has been removed.
		info_event<modification_decoded_info> modification_decoded;
		/// This event is invoked after \ref modification_decoded after \ref _linebreaks and \ref _chunks have been
		/// updated. This can be useful for obtaining information about content added by the modification.
		info_event<end_modification_info> end_modification;
		/// Wraps around \ref buffer::end_edit to provide additional information.
		info_event<end_edit_info> end_edit;
		/// Invoked when the appearance of this document has changed. In many instances this would need to be invoked
		/// manually. This may be invoked repeatedly for a single change (due to the change causing multiple function
		/// calls), so it's preferable to process this event lazily.
		info_event<appearance_changed_info> appearance_changed;
	protected:
		/// Used to find the number of bytes before a specified codepoint.
		struct _codepoint_pos_converter {
			/// The underlying \ref sum_synthesizer::index_finder.
			using finder = sum_synthesizer::index_finder<node_data::num_codepoints_property>;
			/// Interface for \ref red_black_tree::tree::find_custom().
			int select_find(const node_type &n, std::size_t &c) {
				return finder::template select_find<node_data::num_bytes_property>(n, c, total_bytes);
			}
			std::size_t total_bytes = 0; ///< Records the total number of bytes before the resulting chunk.
		};
		/// Used to find the chunk in which the i-th byte is located.
		template <template <typename T> typename Cmp> using _byte_finder =
			sum_synthesizer::index_finder<node_data::num_bytes_property, false, Cmp<std::size_t>>;
		/// Used to find the chunk in which the i-th byte is located, and the number of codepoints before that chunk.
		template <template <typename T> typename Cmp = std::less> struct _byte_pos_converter {
			/// The underlying \ref sum_synthesizer::index_finder.
			using finder = _byte_finder<Cmp>;
			/// Interface for \ref red_black_tree::tree::find_custom().
			int select_find(const node_type &n, std::size_t &c) {
				return finder::template select_find<node_data::num_codepoints_property>(n, c, total_codepoints);
			}
			std::size_t total_codepoints = 0; ///< Records the total number of codepoints before the resulting chunk.
		};

		/// Stores information generated before a modification and used after a modification.
		struct _modification_cache {
			// starting position
			std::size_t
				/// The codepoint index that corresponds to the first entry in \ref start_boundaries.
				start_decoding_codepoint = 0,
				/// The codepoint position of the first codepoint of \ref start_decoding_chunk.
				chunk_codepoint_offset = 0,
				chunk_byte_offset = 0; ///< The byte position of the first byte of \ref start_decoding_chunk.
			tree_type::const_iterator start_decoding_chunk; ///< The chunk that \ref start_decoding_codepoint is in.
			/// Byte indices of a few consecutive codepoints before the start of the modified range.
			std::vector<std::size_t> start_boundaries;
			// end position
			/// Cached codepoint boundaries (byte positions) after the erased region in the old document. Elements in
			/// this array represent consecutive codepoints.
			std::vector<std::size_t> post_erase_boundaries;
			/// The codepoint index of the first element of \ref post_erase_boundaries in the old document.
			std::size_t post_erase_codepoint_index = 0;

			/// All character modification position for the current edit. Unlike \ref buffer::end_edit_info, all
			/// positions in these structs are caret positions, and the ranges may overlap - they should be treated
			/// as a series of consecutive operations.
			std::vector<buffer::modification_position> modification_chars;
		};

		/// Used to store precomputed byte positions of a modification.
		struct _precomp_mod_positions {
			/// Default constructor.
			_precomp_mod_positions() = default;
			/// Initializes all fields of this struct.
			_precomp_mod_positions(std::size_t beg, std::size_t len) : begin(beg), length(len) {
			}

			std::size_t
				begin = 0, ///< Position of the first removed byte.
				length = 0; ///< The number of consecutive removed bytes.
		};


		tree_type _chunks; ///< Chunks used to speed up navigation.
		linebreak_registry _linebreaks; ///< Records all linebreaks.

		document_theme_provider_registry _theme_providers; ///< Theme providers.
		interpretation_decoration_provider_list _decorations{ interpretation_ref(*this) }; ///< The list of decoration providers.
		std::list<std::unique_ptr<tooltip_provider>> _tooltip_providers; ///< The list of tooltip providers.

		std::deque<std::any> _tags; ///< Tags associated with this interpretation.

		const std::shared_ptr<buffer> _buf; ///< The underlying \ref buffer.
		ui::line_ending _line_ending = ui::line_ending::n; ///< The default line ending for this \ref interpretation.
		const buffer_encoding *const _encoding = nullptr; ///< The encoding used to interpret the \ref buffer.

		// event tokens
		/// Used to listen to \ref buffer::begin_modify;
		info_event<buffer::begin_modification_info>::token _begin_modify_tok;
		/// Used to listen to \ref buffer::end_modify;
		info_event<buffer::end_modification_info>::token _end_modify_tok;
		info_event<buffer::end_edit_info>::token _end_edit_tok; ///< Used to listen to \ref buffer::end_edit.

		/// Used when the underlying \ref buffer is changed to update \ref _chunks and \ref _linebreaks.
		_modification_cache _mod_cache;


		/// Computes byte positions of the removed contents of an edit for a whole \ref caret_set, when the user
		/// inputs a short clip of text. This function assumes that \ref caret_data::bytepos_first and
		/// \ref caret_data::bytepos_second have already been computed.
		std::vector<_precomp_mod_positions> _precomp_mod_insert(const caret_set &carets) {
			std::vector<_precomp_mod_positions> res;
			character_position_converter conv(*this);
			for (auto it = carets.begin(); it.get_iterator() != carets.carets.end(); it.move_next()) {
				auto [start, end] = it.get_caret_selection().get_range();
				std::size_t first = conv.character_to_byte(start), second = conv.character_to_byte(end);
				res.emplace_back(first, second - first);
			}
			return res;
		}
		/// Similar to \ref _precomp_mod_insert(), but for when the user presses the `backspace' key.
		std::vector<_precomp_mod_positions> _precomp_mod_backspace(const caret_set &carets) {
			std::vector<_precomp_mod_positions> res;
			character_position_converter conv(*this);
			for (auto it = carets.begin(); it.get_iterator() != carets.carets.end(); it.move_next()) {
				auto caret_sel = it.get_caret_selection();
				std::size_t first, second;
				if (caret_sel.has_selection()) {
					auto [start, end] = caret_sel.get_range();
					first = conv.character_to_byte(start);
					second = conv.character_to_byte(end);
				} else {
					std::size_t caret = caret_sel.get_caret_position();
					if (caret > 0) {
						first = conv.character_to_byte(caret - 1);
						second = conv.character_to_byte(caret);
					} else {
						first = second = conv.character_to_byte(caret);
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
			for (auto it = carets.begin(); it.get_iterator() != carets.carets.end(); it.move_next()) {
				auto caret_sel = it.get_caret_selection();
				std::size_t first, second;
				if (caret_sel.has_selection()) {
					auto [start, end] = caret_sel.get_range();
					first = conv.character_to_byte(start);
					second = conv.character_to_byte(end);
				} else {
					std::size_t caret = caret_sel.get_caret_position();
					first = conv.character_to_byte(caret);
					if (caret < get_linebreaks().num_chars()) {
						second = conv.character_to_byte(caret + 1);
					} else {
						second = first;
					}
				}
				res.emplace_back(first, second - first);
			}
			return res;
		}


		/// Attempts to merge the given node with its neighboring chunks, if they're too small. This operation won't
		/// invalidate the given iterator.
		///
		/// \param it Iterator to the target node.
		/// \param merged If the nodes are merged, this will be filled with data of the deleted node.
		/// \return Whether the nodes have been merged.
		/// \todo Find a better merging strategy.
		bool _try_merge_small_chunks(tree_type::const_iterator it, chunk_data &merged) {
			if (it != _chunks.end() && it != _chunks.begin()) {
				auto prev = it;
				--prev;
				if (it->num_codepoints + prev->num_codepoints < maximum_codepoints_per_chunk) { // merge!
					merged = *prev;
					_chunks.erase(prev);
					{
						auto mod = _chunks.get_modifier_for(it.get_node());
						mod->num_bytes += merged.num_bytes;
						mod->num_codepoints += merged.num_codepoints;
					}
					return true;
				}
			}
			return false;
		}

		/// Called when a modification is about to be made. This function gathers line-related information about the
		/// removed text and saves codepoint boundaries around the erased region to speed up decoding of new content.
		void _on_begin_modify(buffer::begin_modification_info&);
		/// Called when a modification has been made. This function decodes the inserted content and updates
		/// \ref _chunks and \ref _linebreaks.
		void _on_end_modify(buffer::end_modification_info&);
		/// Called when an edit has been made to \ref _buf to invoke \ref theme_changed.
		void _on_end_edit(buffer::end_edit_info &info) {
			buffer::edit_positions pos;
			std::swap(pos, _mod_cache.modification_chars);
			end_edit.invoke_noret(std::move(pos), info);

			appearance_changed.invoke_noret(appearance_change_type::layout_and_visual);
		}
	};
}
