// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/pango_harfbuzz_text_context.h"

/// \file
/// Implementation of text layout using Pango, Harfbuzz, Freetype, and Fontconfig.

#include <harfbuzz/hb-ft.h>

#include "codepad/ui/misc.h"

namespace codepad::ui::pango_harfbuzz {
	namespace _details {
		/// Converts a component of a color to a \p guint16.
		[[nodiscard]] guint16 cast_color_component(double c) {
			return static_cast<guint16>(std::round(c * std::numeric_limits<guint16>::max()));
		}

		/// Converts a \ref horizontal_text_alignment to a \p PangoAlignment.
		[[nodiscard]] PangoAlignment cast_horizontal_alignment(horizontal_text_alignment align) {
			switch (align) {
				// FIXME front & rear does not have exactly the same semantics as left & right
			case horizontal_text_alignment::front:
				return PANGO_ALIGN_LEFT;
			case horizontal_text_alignment::rear:
				return PANGO_ALIGN_RIGHT;
			case horizontal_text_alignment::center:
				return PANGO_ALIGN_CENTER;
			}
			return PANGO_ALIGN_LEFT;
		}

		/// Converts a \ref font_style to a \p PangoStyle.
		[[nodiscard]] PangoStyle cast_font_style(font_style style) {
			switch (style) {
			case font_style::normal:
				return PANGO_STYLE_NORMAL;
			case font_style::italic:
				return PANGO_STYLE_ITALIC;
			case font_style::oblique:
				return PANGO_STYLE_OBLIQUE;
			}
			return PANGO_STYLE_NORMAL;
		}

		/// Converts a \ref font_style to a Fontconfig font slant.
		[[nodiscard]] int cast_font_style_fontconfig(font_style style) {
			switch (style) {
			case font_style::normal:
				return FC_SLANT_ROMAN;
			case font_style::italic:
				return FC_SLANT_ITALIC;
			case font_style::oblique:
				return FC_SLANT_OBLIQUE;
			}
			return FC_SLANT_ROMAN;
		}

		/// Converts a \ref font_weight to a \p PangoWeight.
		[[nodiscard]] PangoWeight cast_font_weight(font_weight weight) {
			// TODO
			return PANGO_WEIGHT_NORMAL;
		}

		/// Converts a \ref font_weight to a Fontconfig weight.
		[[nodiscard]] int cast_font_weight_fontconfig(font_weight weight) {
			// TODO
			return FC_WEIGHT_NORMAL;
		}

		/// Converts a \ref font_stretch to a \p PangoStretch.
		[[nodiscard]] PangoStretch cast_font_stretch(font_stretch stretch) {
			// TODO
			return PANGO_STRETCH_NORMAL;
		}

		/// Converts a \ref font_stretch to a Fontconfig width.
		[[nodiscard]] int cast_font_stretch_fontconfig(font_stretch stretch) {
			// TODO
			return FC_WIDTH_NORMAL;
		}

		/// Casts a \p PangoRectangle **in pango units** to a \ref rectd.
		[[nodiscard]] rectd cast_rect_back(const PangoRectangle &rect) {
			return rectd::from_xywh(
				pango_units_to_double(rect.x),
				pango_units_to_double(rect.y),
				pango_units_to_double(rect.width),
				pango_units_to_double(rect.height)
			);
		}


		/// Casts a \ref ui::font to a \ref font.
		[[nodiscard]] font &cast_font(ui::font &t) {
			auto *rt = dynamic_cast<font*>(&t);
			assert_true_usage(rt, "invalid formatted text type");
			return *rt;
		}
	}


	void fontconfig_usage::maybe_initialize() {
		static fontconfig_usage _usage;
	}


	rectd formatted_text::get_layout() const {
		PangoRectangle layout;
		pango_layout_get_extents(_layout.get(), nullptr, &layout);
		return _details::cast_rect_back(layout).translated(_get_offset());
	}

	std::vector<line_metrics> formatted_text::get_line_metrics() const {
		PangoLayoutIter *iter = pango_layout_get_iter(_layout.get());
		std::vector<line_metrics> result;
		auto pos_it = _line_positions.begin();
		std::size_t prev_pos = 0;
		do {
			int ymin_pu = 0, ymax_pu = 0;
			pango_layout_iter_get_line_yrange(iter, &ymin_pu, &ymax_pu);
			double height = pango_units_to_double(ymax_pu - ymin_pu);
			double baseline = pango_units_to_double(pango_layout_iter_get_baseline(iter) - ymin_pu);
			// gather results
			auto &line_info = result.emplace_back();
			line_info.height = height;
			line_info.baseline = baseline;

			assert_true_logical(pos_it != _line_positions.end(), "not enough entries in line lengths");
			line_info.non_linebreak_characters = pos_it->end_pos_before_break - prev_pos;
			line_info.linebreak_characters = pos_it->end_pos_after_break - pos_it->end_pos_before_break;

			prev_pos = pos_it->end_pos_after_break;
			++pos_it;
		} while (pango_layout_iter_next_line(iter));
		pango_layout_iter_free(iter);
		return result;
	}

	caret_hit_test_result formatted_text::hit_test(vec2d pos) const {
		vec2d offset = _get_offset();
		pos -= offset;
		int index = 0, trailing = 0;
		PangoRectangle rect;
		pango_layout_xy_to_index(
			_layout.get(),
			pango_units_from_double(pos.x), pango_units_from_double(pos.y),
			&index, &trailing
		);
		pango_layout_index_to_pos(_layout.get(), index, &rect);

		return caret_hit_test_result(
			_byte_to_char(static_cast<std::size_t>(index)),
			_details::cast_rect_back(rect).translated(offset),
			rect.width < 0,
			trailing != 0
		);
	}

	caret_hit_test_result formatted_text::hit_test_at_line(std::size_t line, double x) const {
		vec2d offset = _get_offset();
		x -= offset.x;
		caret_hit_test_result res;
		PangoLayoutLine *pango_line = pango_layout_get_line_readonly(_layout.get(), static_cast<int>(line));
		int byte_index = 0;
		if (pango_line == nullptr) {
			byte_index = static_cast<int>(_bytepos.back());
			res.character = _bytepos.size() - 1;
			res.rear = true;
		} else {
			int trailing = 0;
			// TODO the x offset is from the left edge of the line
			pango_layout_line_x_to_index(pango_line, pango_units_from_double(x), &byte_index, &trailing);
			res.character = _byte_to_char(static_cast<std::size_t>(byte_index));
			res.rear = trailing != 0;
		}
		PangoRectangle rect;
		pango_layout_index_to_pos(_layout.get(), byte_index, &rect);
		res.character_layout = _details::cast_rect_back(rect).translated(offset);
		res.right_to_left = rect.width < 0;
		return res;
	}

	rectd formatted_text::get_character_placement(std::size_t pos) const {
		PangoRectangle rect;
		pango_layout_index_to_pos(
			_layout.get(), static_cast<int>(_bytepos[std::min(pos, _bytepos.size() - 1)]), &rect
		);
		return _details::cast_rect_back(rect).translated(_get_offset());
	}

	std::vector<rectd> formatted_text::get_character_range_placement(std::size_t beg, std::size_t len) const {
		std::size_t end = beg + len;
		std::size_t first_line = std::lower_bound(
			_line_positions.begin(), _line_positions.end(), beg,
			[](const _line_position &line, std::size_t pos) {
				return line.end_pos_after_break < pos;
			}
		) - _line_positions.begin();

		PangoLayoutIter *pango_iter = pango_layout_get_iter(_layout.get());
		for (std::size_t i = 0; i < first_line; ++i) {
			assert_true_logical(pango_layout_iter_next_line(pango_iter), "not enough pango lines");
		}
		std::vector<_line_position>::const_iterator line_iter = _line_positions.begin() + first_line;

		std::size_t line_begin = first_line == 0 ? 0 : _line_positions[first_line - 1].end_pos_after_break;
		std::vector<rectd> result;
		while (true) {
			PangoLayoutLine *pango_line = pango_layout_iter_get_line_readonly(pango_iter);
			int *ranges = nullptr;
			int num_ranges = 0;
			pango_layout_line_get_x_ranges(
				pango_line,
				static_cast<int>(_bytepos[std::max(beg, line_begin)]),
				// FIXME this will not add the extra space after the line representing the line break
				//       however pango does not seem to add it even if this is after the break
				//       so maybe we need to add it manually
				static_cast<int>(_bytepos[std::min(end, line_iter->end_pos_before_break)]),
				&ranges, &num_ranges
			);
			if (ranges) {
				int ymin_pu = 0, ymax_pu = 0;
				pango_layout_iter_get_line_yrange(pango_iter, &ymin_pu, &ymax_pu);
				double ymin = pango_units_to_double(ymin_pu), ymax = pango_units_to_double(ymax_pu);

				int *iter = ranges;
				for (int i = 0; i < num_ranges; ++i, iter += 2) {
					int line_beg = iter[0], line_end = iter[1];
					result.emplace_back(
						pango_units_to_double(line_beg), pango_units_to_double(line_end), ymin, ymax
					);
				}
				g_free(ranges);
			}

			if (end < line_iter->end_pos_after_break) {
				break;
			}
			line_begin = line_iter->end_pos_after_break;
			++line_iter;
			if (line_iter == _line_positions.end()) {
				break;
			}
			assert_true_logical(pango_layout_iter_next_line(pango_iter), "not enough pango lines");
		}

		pango_layout_iter_free(pango_iter);

		// offset all regions
		vec2d offset = _get_offset();
		for (rectd &r : result) {
			r = r.translated(offset);
		}
		return result;
	}

	void formatted_text::set_text_color(colord c, std::size_t beg, std::size_t len) {
		PangoAttribute
			*attr_rgb = pango_attr_foreground_new(
				_details::cast_color_component(c.r),
				_details::cast_color_component(c.g),
				_details::cast_color_component(c.b)
			),
			*attr_a = pango_attr_foreground_alpha_new(_details::cast_color_component(c.a));
		auto [byte_beg, byte_end] = _char_to_byte(beg, len);
		attr_rgb->start_index = attr_a->start_index = byte_beg;
		attr_rgb->end_index = attr_a->end_index = byte_end;
		PangoAttrList *list = pango_layout_get_attributes(_layout.get());
		pango_attr_list_change(list, attr_rgb);
		pango_attr_list_change(list, attr_a);
	}

	void formatted_text::set_font_family(const std::u8string &family, std::size_t beg, std::size_t len) {
		PangoAttribute *attr = pango_attr_family_new(reinterpret_cast<const char*>(family.c_str()));
		auto [byte_beg, byte_end] = _char_to_byte(beg, len);
		attr->start_index = byte_beg;
		attr->end_index = byte_end;
		pango_attr_list_change(pango_layout_get_attributes(_layout.get()), attr);
	}

	void formatted_text::set_font_size(double size, std::size_t beg, std::size_t len) {
		PangoAttribute *attr = pango_attr_size_new(pango_units_from_double(size));
		auto [byte_beg, byte_end] = _char_to_byte(beg, len);
		attr->start_index = byte_beg;
		attr->end_index = byte_end;
		pango_attr_list_change(pango_layout_get_attributes(_layout.get()), attr);
	}

	void formatted_text::set_font_style(font_style style, std::size_t beg, std::size_t len) {
		PangoAttribute *attr = pango_attr_style_new(_details::cast_font_style(style));
		auto [byte_beg, byte_end] = _char_to_byte(beg, len);
		attr->start_index = byte_beg;
		attr->end_index = byte_end;
		pango_attr_list_change(pango_layout_get_attributes(_layout.get()), attr);
	}

	void formatted_text::set_font_weight(font_weight weight, std::size_t beg, std::size_t len) {
		PangoAttribute *attr = pango_attr_weight_new(_details::cast_font_weight(weight));
		auto [byte_beg, byte_end] = _char_to_byte(beg, len);
		attr->start_index = byte_beg;
		attr->end_index = byte_end;
		pango_attr_list_change(pango_layout_get_attributes(_layout.get()), attr);
	}

	void formatted_text::set_font_stretch(font_stretch stretch, std::size_t beg, std::size_t len) {
		PangoAttribute *attr = pango_attr_stretch_new(_details::cast_font_stretch(stretch));
		auto [byte_beg, byte_end] = _char_to_byte(beg, len);
		attr->start_index = byte_beg;
		attr->end_index = byte_end;
		pango_attr_list_change(pango_layout_get_attributes(_layout.get()), attr);
	}

	vec2d formatted_text::_get_offset() const {
		PangoAlignment halign = pango_layout_get_alignment(_layout.get());
		if (_valign == vertical_text_alignment::top && halign == PANGO_ALIGN_LEFT) {
			return vec2d();
		}
		// get layout extents
		PangoRectangle layout_pango;
		pango_layout_get_extents(_layout.get(), nullptr, &layout_pango);
		rectd layout = _details::cast_rect_back(layout_pango);
		// compute offset
		vec2d offset = _layout_size - layout.xmax_ymax();
		// x
		if (halign == PANGO_ALIGN_LEFT) {
			offset.x = 0.0;
		} else if (halign == PANGO_ALIGN_CENTER) {
			offset.x *= 0.5;
		}
		// y
		if (_valign == vertical_text_alignment::top) {
			offset.y = 0.0;
		} else if (_valign == vertical_text_alignment::center) {
			offset.y *= 0.5;
		}
		return offset;
	}

	std::pair<guint, guint> formatted_text::_char_to_byte(std::size_t beg, std::size_t len) const {
		beg = std::min(beg, _bytepos.size() - 1);
		std::size_t end = std::min(beg + len, _bytepos.size() - 1);
		return { static_cast<guint>(_bytepos[beg]), static_cast<guint>(_bytepos[end]) };
	}

	std::size_t formatted_text::_byte_to_char(std::size_t c) const {
		return std::lower_bound(_bytepos.begin(), _bytepos.end(), c) - _bytepos.begin();
	}


	std::shared_ptr<ui::font> font_family::get_matching_font(
		font_style style, font_weight weight, font_stretch stretch
	) const {
		auto patt = ui::_details::make_gtk_object_ref_give(FcPatternDuplicate(_pattern.get()));

		// FIXME what are these return values?
		FcPatternAddInteger(patt.get(), FC_SLANT, _details::cast_font_style_fontconfig(style));
		FcPatternAddInteger(patt.get(), FC_WEIGHT, _details::cast_font_weight_fontconfig(weight));
		FcPatternAddInteger(patt.get(), FC_WIDTH, _details::cast_font_stretch_fontconfig(stretch));

		// FIXME fontconfig? hello? what should i do here?
		//       FcFontMatch() calls FcFontRenderPrepare() which calls FcConfigSubstituteWithPat() with
		//       FcMatchFont, so i assume that we don't need to do that here
		assert_true_sys(FcConfigSubstitute(nullptr, patt.get(), FcMatchPattern), "Fontconfig operation failed");
		FcDefaultSubstitute(patt.get());

		FcResult result;
		auto res_patt = ui::_details::make_gtk_object_ref_give(FcFontMatch(nullptr, patt.get(), &result));
		assert_true_sys(result != FcResultOutOfMemory, "Fontconfig out of memory");

		FcChar8 *file_name = nullptr;
		int font_index = 0;
		assert_true_sys(
			FcPatternGetString(res_patt.get(), FC_FILE, 0, &file_name) == FcResultMatch,
			"failed to obtain font file name from Fontconfig"
		);
		FcPatternGetInteger(res_patt.get(), FC_INDEX, 0, &font_index);

		FT_Face face;
		ui::_details::ft_check(FT_New_Face(
			_ctx._freetype, reinterpret_cast<const char*>(file_name), font_index, &face
		));

		return std::make_shared<font>(ui::_details::make_freetype_face_ref_give(face));
	}


	caret_hit_test_result plain_text::hit_test(double x) const {
		_maybe_calculate_block_map();

		x = std::max(x, 0.0);
		auto block_iter = std::lower_bound(
			_cached_block_positions.begin(), _cached_block_positions.end(), x
		);
		if (block_iter != _cached_block_positions.begin()) {
			--block_iter;
		}
		std::size_t block_id = block_iter - _cached_block_positions.begin();

		caret_hit_test_result res;
		if (block_id + 1 < _cached_block_positions.size()) {
			double part_width = _get_part_width(block_id);
			double offset = (x - _cached_block_positions[block_id]) / part_width;
			std::size_t block_offset = static_cast<std::size_t>(offset);
			double left = _cached_block_positions[block_id] + block_offset * part_width;

			res.character = _cached_first_char_of_block[block_id] + block_offset;
			res.rear = (offset - block_offset) > 0.5;
			res.character_layout = rectd::from_xywh(left, 0.0, part_width, _height);
		} else { // end of the clip
			res.character = _num_characters;
			res.rear = false;
			res.character_layout = rectd::from_xywh(_cached_block_positions.back(), 0.0, 0.0, _height);
		}
		return res;
	}

	rectd plain_text::get_character_placement(std::size_t i) const {
		_maybe_calculate_block_map();

		if (i >= _num_characters) {
			return rectd::from_xywh(_cached_block_positions.back(), 0.0, 0.0, _height);
		}

		auto block_it = std::upper_bound(_cached_first_char_of_block.begin(), _cached_first_char_of_block.end(), i);
		if (block_it == _cached_first_char_of_block.begin()) {
			// this means that the first few characters have no corresponding glyphs
			return rectd(0.0, 0.0, 0.0, _height);
		}
		--block_it;
		std::size_t block_id = block_it - _cached_first_char_of_block.begin();
		double part_width = _get_part_width(block_id);
		double left = _cached_block_positions[block_id] + part_width * (i - _cached_first_char_of_block[block_id]);
		return rectd::from_xywh(left, 0.0, part_width, _height);
	}

	void plain_text::_maybe_calculate_block_map() const {
		if (_cached_block_positions.empty()) {
			unsigned int num_glyphs_u = 0;
			hb_glyph_info_t *glyphs = hb_buffer_get_glyph_infos(_buffer.get(), &num_glyphs_u);
			hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(_buffer.get(), &num_glyphs_u);
			auto num_glyphs = static_cast<std::size_t>(num_glyphs_u);

			_cached_first_char_of_block.reserve(num_glyphs);
			_cached_block_positions.reserve(num_glyphs + 1);

			double pos = 0.0;
			std::size_t last_char = std::numeric_limits<std::size_t>::max();
			for (std::size_t i = 0; i < num_glyphs; ++i) {
				auto cur_char = static_cast<std::size_t>(glyphs[i].cluster);
				if (cur_char != last_char) {
					_cached_first_char_of_block.emplace_back(cur_char);
					_cached_block_positions.emplace_back(pos);
					last_char = cur_char;
				}
				pos += positions[i].x_advance / 64.0;
			}

			// final element
			_cached_first_char_of_block.emplace_back(_num_characters);
			_cached_block_positions.emplace_back(pos);
		}
	}

	double plain_text::_get_part_width(std::size_t block) const {
		return
			(_cached_block_positions[block + 1] - _cached_block_positions[block]) /
			static_cast<double>(_cached_first_char_of_block[block + 1] - _cached_first_char_of_block[block]);
	}


	std::shared_ptr<formatted_text> text_context::_create_formatted_text_impl(
		std::u8string_view text, const font_parameters &font, colord c, vec2d size, wrapping_mode wrap,
		horizontal_text_alignment halign, vertical_text_alignment valign
	) {
		auto result = std::make_shared<formatted_text>(size, valign);
		result->_layout.set_give(pango_layout_new(_pango_context.get()));

		pango_layout_set_text(
			result->_layout.get(), reinterpret_cast<const char*>(text.data()), static_cast<int>(text.size())
		);

		{ // set font
			PangoFontDescription *desc = pango_font_description_new();
			pango_font_description_set_family(desc, reinterpret_cast<const char*>(font.family.c_str()));
			pango_font_description_set_style(desc, _details::cast_font_style(font.style));
			pango_font_description_set_weight(desc, _details::cast_font_weight(font.weight));
			pango_font_description_set_stretch(desc, _details::cast_font_stretch(font.stretch));
			pango_font_description_set_size(desc, pango_units_from_double(font.size));
			pango_layout_set_font_description(result->_layout.get(), desc);
			pango_font_description_free(desc);
		}

		pango_layout_set_ellipsize(result->_layout.get(), PANGO_ELLIPSIZE_NONE);
		pango_layout_set_single_paragraph_mode(result->_layout.get(), false);

		// horizontal wrapping
		if (wrap == wrapping_mode::none) {
			// FIXME alignment won't work for this case
			pango_layout_set_width(result->_layout.get(), -1); // disable wrapping
		} else {
			pango_layout_set_width(result->_layout.get(), pango_units_from_double(size.x));
			pango_layout_set_wrap(result->_layout.get(), PANGO_WRAP_WORD_CHAR);
		}
		pango_layout_set_alignment(result->_layout.get(), _details::cast_horizontal_alignment(halign));

		// TODO vertical alignment
		// "The behavior is undefined if a height other than -1 is set and ellipsization mode is set to
		// PANGO_ELLIPSIZE_NONE, and may change in the future."
		// https://developer.gnome.org/pango/stable/pango-Layout-Objects.html#pango-layout-set-height
		pango_layout_set_height(result->_layout.get(), -1);

		{ // set color
			auto attr_list = ui::_details::make_gtk_object_ref_give(pango_attr_list_new());
			pango_attr_list_insert(attr_list.get(), pango_attr_foreground_new(
				_details::cast_color_component(c.r),
				_details::cast_color_component(c.g),
				_details::cast_color_component(c.b)
			));
			pango_attr_list_insert(
				attr_list.get(), pango_attr_foreground_alpha_new(_details::cast_color_component(c.a))
			);
			pango_layout_set_attributes(result->_layout.get(), attr_list.get());
		}

		// compute _bytepos and _line_positions
		result->_bytepos.emplace_back(0); // first character
		linebreak_analyzer line_analyzer(
			[&result](std::size_t len, line_ending ending) {
				std::size_t begin_pos =
					result->_line_positions.empty() ? 0 : result->_line_positions.back().end_pos_after_break;
				formatted_text::_line_position &entry = result->_line_positions.emplace_back();
				entry.end_pos_before_break = begin_pos + len;
				entry.end_pos_after_break = entry.end_pos_before_break + get_line_ending_length(ending);
			}
		);
		for (auto it = text.begin(); it != text.end(); ) {
			codepoint cp;
			if (!encodings::utf8::next_codepoint(it, text.end(), cp)) {
				cp = 0;
			}
			line_analyzer.put(cp);
			result->_bytepos.emplace_back(it - text.begin());
		}
		line_analyzer.finish();
		assert_true_sys(
			result->_line_positions.back().end_pos_after_break ==
			pango_layout_get_character_count(result->_layout.get()),
			"character count inconsistent with pango"
		);
		assert_true_sys(
			result->_line_positions.size() == pango_layout_get_line_count(result->_layout.get()),
			"line count inconsistent with pango"
		);

		return result;
	}

	std::shared_ptr<plain_text> text_context::_create_plain_text_impl(
		ui::_details::gtk_object_ref<hb_buffer_t> buf, ui::font &generic_fnt, double font_size
	) {
		auto fnt = _details::cast_font(generic_fnt);

		unsigned int num_chars = hb_buffer_get_length(buf.get());
		hb_buffer_set_content_type(buf.get(), HB_BUFFER_CONTENT_TYPE_UNICODE);

		// FIXME see if these settings are good enough for code
		hb_buffer_set_direction(buf.get(), HB_DIRECTION_LTR);
		hb_buffer_set_script(buf.get(), HB_SCRIPT_LATIN);
		hb_buffer_set_language(buf.get(), hb_language_from_string("en", -1));

		ui::_details::ft_check(FT_Set_Char_Size(
			fnt._face.get(), 0, static_cast<FT_F26Dot6>(std::round(64.0 * font_size)), 96, 96
		));
		hb_font_t *hb_font = hb_ft_font_create(fnt._face.get(), nullptr);

		hb_shape(hb_font, buf.get(), nullptr, 0); // TODO features?

		hb_font_destroy(hb_font);
		return std::make_shared<plain_text>(std::move(buf), fnt, fnt._face->size->metrics, num_chars, font_size);
	}
}
