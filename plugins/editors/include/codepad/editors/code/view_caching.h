// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Structs and classes used to cache layout information of text fragments, to speed up rendering and layout
/// operations.

#include <queue>

#include "codepad/editors/buffer_manager.h"
#include "interpretation.h"
#include "fragment_generation.h"

namespace codepad::editors::code {
	/*
	/// For long lines, the text is split into small chunks and the length of each chunk is calculated and stored
	/// separately to accelerate certain operations.
	class line_part_length_cache {
	public:
		constexpr static size_t maximum_chunk_size = 80; ///< The maimum size of a chunk.

		/// Information of a chunk of text.
		struct chunk_info {
			/// Default constructor.
			chunk_info() = default;
			/// Initializes all fields of the struct.
			chunk_info(double sz, size_t nchars) : text_size(sz), num_chars(nchars) {
			}

			double text_size = 0.0; ///< The space that the text takes up, in pixels.
			size_t num_chars = 0; ///< The number of characters in this chunk.
		};
		/// Stores additional data of a node in the tree.
		struct chunk_synth_data {
			/// The type of a node in the tree.
			using node_type = binary_tree_node<chunk_info, chunk_synth_data>;

			/// The total space that the subtree takes up, in pixels.
			///
			/// \remark This field is not calculated by simply summing up the same property of the two children
			///         because of kerning.
			double total_size = 0.0;
			size_t total_chars = 0; ///< The total number of characters.

			using total_chars_property = sum_synthesizer::compact_property<
				synthesization_helper::field_value_property<&chunk_info::num_chars>,
				&chunk_synth_data::total_chars
			>; ///< Property used to calculate the total number of characters in a subtree.

			/// Updates the value of this struct. \ref total_size is not updated here because updating it requires
			/// the position of this node in the line. The custom synthesizer records this node and performs updates
			/// later.
			inline static void synthesize(node_type &n) {
				sum_synthesizer::synthesize<total_chars_property>(n);
			}
		};
		/// The type of a node in the tree.
		using node_type = binary_tree_node<chunk_info, chunk_synth_data>;
		/// The type of a tree.
		using tree_type = binary_tree<chunk_info, chunk_synth_data, lacks_synthesizer>;
		/// The custom synthesizer.
		struct synthesizer {
			/// Calls \ref chunk_synth_data::synthesize and adds the given node to \ref invalid_nodes to update
			/// later.
			void operator()(node_type &n) {
				chunk_synth_data::synthesize(n);
				invalid_nodes.emplace(&n);
			}

			/// The nodes whose \ref chunk_synth_data::total_size are invalid, stored in a queue so that only these
			/// nodes need updating.
			std::queue<node_type*> invalid_nodes;
		};
		using iterator = tree_type::iterator; ///< Iterators.

		/// Returns an iterator to the node that the character at the given position is in.
		iterator find_node_of_character(size_t pos) {
			return _t.find_custom(_find_node_of_char(), pos);
		}

		/// Recalculates \ref chunk_synth_data::total_size of the given node.
		///
		/// \param n The node.
		/// \param pos The position of the first character of this line in the document.
		/// \param doc The document.
		void recalc_size_of(node_type *n, size_t pos, interpretation &doc) {
			n->synth_data.total_size = n->value.text_size;
			if (n->left || n->right) {
				size_t posinline = _first_char_of_node(n);
				if (n->left) {
					n->synth_data.total_size +=
						n->left->synth_data.total_size +
						_get_kerning_at(doc, pos + posinline - 1);
				}
				if (n->right) {
					n->synth_data.total_size +=
						n->right->synth_data.total_size +
						_get_kerning_at(doc, pos + posinline + n->value.num_chars - 1);
				}
			}
		}

		/// Resets this struct with the given text.
		void set(interpretation &doc, size_t beg, size_t len) {
			_t.clear();
			size_t nchunks = (len + maximum_chunk_size - 1) / maximum_chunk_size;
			std::vector<chunk_info> chunks;
			size_t pos = beg;
			for (size_t i = 1; i < nchunks; ++i) {
				size_t chunk_size = maximum_chunk_size;
				chunks.emplace_back(_get_chunk_size(doc, pos, pos + chunk_size), chunk_size);
				pos += chunk_size;
				len -= chunk_size;
			}
			chunks.emplace_back(_get_chunk_size(doc, pos, pos + len), len);
			synthesizer synth;
			_t.insert_range_before_move(_t.end(), chunks.begin(), chunks.end(), synth);
			while (!synth.invalid_nodes.empty()) {
				node_type *n = synth.invalid_nodes.front();
				synth.invalid_nodes.pop();
				recalc_size_of(n, beg, doc);
			}
		}
	protected:
		/// Used to find the node that the character at a given position is in.
		using _find_node_of_char = sum_synthesizer::index_finder<chunk_synth_data::total_chars_property>;

		/// Calculates and returns the horizontal size of the given range of characters.
		double _get_chunk_size(document &doc, size_t start, size_t end) {
			character_rendering_iterator iter(doc, 0.0, start, end); // line height doesn't matter
			for (iter.begin(); !iter.ended(); iter.next_char()) {
			}
			return iter.character_info().prev_char_right();
		}
		/// Returns the kerning between the character at \p pos and the one at <tt>pos + 1</tt>. Note that \p pos is
		/// relative to the beginning of the document, instead of that of the line.
		double _get_kerning_at(document &doc, size_t pos) {
			auto tit = doc.get_text_theme().style.get_iter_at(pos), ptit = tit;
			++ptit;
			if (ptit == doc.get_text_theme().style.end() || ptit->first == pos + 1) {
				return 0.0;
			}
			auto cit = doc.at_char(pos);
			codepoint c = cit.current_character();
			++cit;
			return contents_region::get_font().get_by_style(tit->second)->get_kerning(c, cit.current_character()).x;
		}
		/// Returns the position of the first character of the given node in the line.
		size_t _first_char_of_node(node_type *n) const {
			size_t v = 0;
			sum_synthesizer::sum_before<chunk_synth_data::total_chars_property>(_t.get_const_iterator_for(n), v);
			return v;
		}

		tree_type _t; ///< The underlying tree.
	};

	/// Caches \ref line_length_data of long lines of a \ref document to speed up rendering and layout operations.
	class document_formatting_cache {
	public:
		/// The minimum line length for this struct to be in effect.
		constexpr static size_t minimum_line_length = 120;

		/// Initializes \ref _doc and listens to \ref document::visual_changed and \ref document::modified.
		explicit document_formatting_cache(document &doc) : _doc(&doc) {
			_vis_tok = (_doc->visual_changed += [this]() {
				_on_document_visual_changed();
				});
			_vis_changed_tok = (_doc->modified += [this](modification_info &info) {
				_on_document_modified(info);
				});
			_recalculate_cache();
		}
		/// No move constructor.
		///
		/// \todo Wait for std::any to change.
		document_formatting_cache(const document_formatting_cache&) {
			assert_true_logical(false, "document_formatting_cache should not be copy constructed");
		}
		/// Move constructor. Sets \ref _doc of the moved-from object to \p nullptr.
		document_formatting_cache(document_formatting_cache &&cache) :
			_lines(std::move(cache._lines)), _doc(cache._doc),
			_vis_tok(std::move(cache._vis_tok)), _vis_changed_tok(std::move(cache._vis_changed_tok)) {
			cache._doc = nullptr;
		}
		/// No copy assignment.
		document_formatting_cache &operator=(const document_formatting_cache&) = delete;
		/// Unregisters event handlers.
		~document_formatting_cache() {
			if (_doc) {
				_doc->visual_changed -= _vis_tok;
				_doc->modified -= _vis_changed_tok;
			}
		}

		/// Enables caching for all documents.
		inline static void enable() {
			assert_true_logical(!in_effect(), "invalid enable() call");
			size_t id = buffer_manager::get().allocate_tag();
			buffer_manager::get().for_each_buffer([id](const std::shared_ptr<document> &doc) {
				doc->get_tag(id).emplace<document_formatting_cache>(*doc);
				});
			_in_effect_params &params = _eff.emplace();
			params.tag_id = id;
			params.event_token = (buffer_manager::get().buffer_created += [id](buffer_info &info) {
				info.doc.get_tag(id).emplace<document_formatting_cache>(info.doc);
				});
		}
		/// Returns whether this class is currently in effect.
		inline static bool in_effect() {
			return _eff.has_value();
		}
		/// Disables caching for all documents.
		inline static void disable() {
			assert_true_logical(in_effect(), "invalid disable() call");
			buffer_manager::get().deallocate_tag(_eff->tag_id);
			buffer_manager::get().buffer_created -= _eff->event_token;
			_eff.reset();
		}
	protected:
		/// Stores lengths of text fragments for certain lines.
		incremental_positional_registry<size_t, line_length_data> _lines;
		document *_doc; ///< The associated \ref document.
		info_event<>::token _vis_tok; ///< Event token for \ref document::visual_changed.
		info_event<modification_info>::token _vis_changed_tok; ///< Event token for \ref document::modified.

		/// Stores information used when this class is in effect.
		struct _in_effect_params {
			size_t tag_id; ///< The index of the tag.
			/// The token for listening to \ref buffer_manager::buffer_created.
			info_event<buffer_info>::token event_token;
		};

		/// Information about \ref buffer_manager when this class is in effect.
		static std::optional<_in_effect_params> _eff;

		/// Clears \ref _lines and recalculates all lengths.
		void _recalculate_cache() {
			_lines.clear();
			size_t loffset = 0, pos = 0;
			for (
				auto i = _doc->get_linebreak_registry().begin();
				i != _doc->get_linebreak_registry().end();
				++i, ++loffset
				) {
				if (i->nonbreak_chars > minimum_line_length) {
					auto it = _lines.insert_at(_lines.end(), loffset, line_length_data());
					it->set(*_doc, pos, i->nonbreak_chars);
					loffset = 0;
				}
				pos += i->nonbreak_chars + 1;
			}
		}

		/// Called when the visuals of \ref _doc has changed, but its content has not been modified. Calls
		/// \ref _recalculate_cache.
		///
		/// \todo Optimize theme specification so that it's not needed to recalculate the entire layout.
		void _on_document_visual_changed() {
			_recalculate_cache();
		}
		/// Called when \ref _doc has been modified. Updates \ref _lines incrementally.
		///
		/// \todo Partially reuse previous results.
		void _on_document_modified(modification_info &info) {
			for (size_t i = 0; i < info.caret_fixup.mods.size(); ++i) {
				const linebreak_registry::text_clip_info &deleted = info.removed_clips_info[i];
				const modification_position &mod = info.caret_fixup.mods[i];
				auto start = _doc->get_linebreak_registry().get_line_and_column_of_char(mod.position);
				// deleted segment
				if (deleted.lines.size() > 1) {
					size_t endline = start.line + deleted.lines.size() - 1;
					auto firstinfo = _lines.find_at_or_after(start.line + 1);
					// entries of all following lines are erased
					while (firstinfo.iterator != _lines.end() && firstinfo.entry_position() <= endline) {
						firstinfo.iterator = _lines.erase(firstinfo.iterator);
					}
					if (firstinfo.iterator != _lines.end()) {
						firstinfo.iterator.get_raw().get_modifier()->length -= deleted.lines.size() - 1;
					}
				}
				// added segment
				auto firstinfo = _lines.find_at_or_after(start.line);
				if (start.column + mod.added_range > start.line_iterator->nonbreak_chars) {
					auto addend = _doc->get_linebreak_registry().get_line_and_column_of_char(
						mod.position + mod.added_range
					);
					auto it = firstinfo.iterator;
					if (firstinfo.iterator != _lines.end() && firstinfo.entry_position() == start.line) {
						++it;
					}
					if (it != _lines.end()) {
						it.get_raw().get_modifier()->length += addend.line - start.line;
					}
					auto curet = _doc->get_linebreak_registry().get_line_info(start.line + 1);
					for (size_t v = start.line + 1; v < addend.line; ++v) {
						if (curet.entry->nonbreak_chars > minimum_line_length) {
							auto inserted = _lines.insert_at(v, line_length_data());
							inserted->set(*_doc, curet.first_char, curet.entry->nonbreak_chars);
						}
						curet.first_char += curet.entry->nonbreak_chars + 1;
						++curet.entry;
					}
				}
				if (start.line_iterator->nonbreak_chars > minimum_line_length) {
					if (firstinfo.iterator == _lines.end()) {
						firstinfo.iterator = _lines.insert_at(start.line, line_length_data());
					}
					firstinfo.iterator->set(
						*_doc,
						_doc->get_linebreak_registry().get_beginning_char_of(start.line_iterator),
						start.line_iterator->nonbreak_chars
					);
				} else {
					if (firstinfo.iterator != _lines.end() && firstinfo.entry_position() == start.line) {
						_lines.erase(firstinfo.iterator);
					}
				}
			}
		}
	};
	*/
}
