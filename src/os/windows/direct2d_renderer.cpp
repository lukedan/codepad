// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/os/windows/direct2d_renderer.h"

/// \file
/// Implementation of the Direct2D renderer.

#include "codepad/ui/misc.h"

namespace codepad::os::direct2d {
	namespace _details { // cast objects to types specific to the direct2d renderer
		using namespace os::_details; // import stuff from common win32

		/// Converts a \ref matd3x3 to a \p D2D1_MATRIX_3X2_F.
		[[nodiscard]] D2D1_MATRIX_3X2_F cast_matrix(matd3x3 m) {
			// the resulting matrix is transposed
			D2D1_MATRIX_3X2_F mat;
			mat.m11 = static_cast<FLOAT>(m[0][0]);
			mat.m12 = static_cast<FLOAT>(m[1][0]);
			mat.m21 = static_cast<FLOAT>(m[0][1]);
			mat.m22 = static_cast<FLOAT>(m[1][1]);
			mat.dx = static_cast<FLOAT>(m[0][2]);
			mat.dy = static_cast<FLOAT>(m[1][2]);
			return mat;
		}
		/// Converts a \ref matd3x3 to a \p DWRITE_MATRIX.
		[[nodiscard]] DWRITE_MATRIX cast_dwrite_matrix(matd3x3 m) {
			// the resulting matrix is transposed
			DWRITE_MATRIX mat;
			mat.m11 = static_cast<FLOAT>(m[0][0]);
			mat.m12 = static_cast<FLOAT>(m[1][0]);
			mat.m21 = static_cast<FLOAT>(m[0][1]);
			mat.m22 = static_cast<FLOAT>(m[1][1]);
			mat.dx = static_cast<FLOAT>(m[0][2]);
			mat.dy = static_cast<FLOAT>(m[1][2]);
			return mat;
		}
		/// Converts a \ref colord to a \p D2D1_COLOR_F.
		[[nodiscard]] D2D1_COLOR_F cast_color(colord c) {
			return D2D1::ColorF(
				static_cast<FLOAT>(c.r),
				static_cast<FLOAT>(c.g),
				static_cast<FLOAT>(c.b),
				static_cast<FLOAT>(c.a)
			);
		}
		/// Converts a \ref rectd to a \p D2D1_RECT_F.
		[[nodiscard]] D2D1_RECT_F cast_rect(rectd r) {
			return D2D1::RectF(
				static_cast<FLOAT>(r.xmin),
				static_cast<FLOAT>(r.ymin),
				static_cast<FLOAT>(r.xmax),
				static_cast<FLOAT>(r.ymax)
			);
		}
		/// Converts a \ref vec2d to a \p D2D1_POINT_2F.
		[[nodiscard]] D2D1_POINT_2F cast_point(vec2d pt) {
			return D2D1::Point2F(static_cast<FLOAT>(pt.x), static_cast<FLOAT>(pt.y));
		}

		/// Casts a \ref ui::font_style to a \p DWRITE_FONT_STYLE.
		[[nodiscard]] DWRITE_FONT_STYLE cast_font_style(ui::font_style style) {
			switch (style) {
			case ui::font_style::normal:
				return DWRITE_FONT_STYLE_NORMAL;
			case ui::font_style::italic:
				return DWRITE_FONT_STYLE_ITALIC;
			case ui::font_style::oblique:
				return DWRITE_FONT_STYLE_OBLIQUE;
			}
			return DWRITE_FONT_STYLE_NORMAL; // should not be here
		}
		/// Casts a \ref ui::font_weight to a \p DWRITE_FONT_WEIGHT.
		[[nodiscard]] DWRITE_FONT_WEIGHT cast_font_weight(ui::font_weight weight) {
			// TODO
			return DWRITE_FONT_WEIGHT_REGULAR;
		}
		/// Casts a \ref ui::font_stretch to a \p DWRITE_FONT_STRETCH.
		[[nodiscard]] DWRITE_FONT_STRETCH cast_font_stretch(ui::font_stretch stretch) {
			switch (stretch) {
			case ui::font_stretch::ultra_condensed:
				return DWRITE_FONT_STRETCH_ULTRA_CONDENSED;
			case ui::font_stretch::extra_condensed:
				return DWRITE_FONT_STRETCH_EXTRA_CONDENSED;
			case ui::font_stretch::condensed:
				return DWRITE_FONT_STRETCH_CONDENSED;
			case ui::font_stretch::semi_condensed:
				return DWRITE_FONT_STRETCH_SEMI_CONDENSED;
			case ui::font_stretch::normal:
				return DWRITE_FONT_STRETCH_NORMAL;
			case ui::font_stretch::semi_expanded:
				return DWRITE_FONT_STRETCH_SEMI_EXPANDED;
			case ui::font_stretch::expanded:
				return DWRITE_FONT_STRETCH_EXPANDED;
			case ui::font_stretch::extra_expanded:
				return DWRITE_FONT_STRETCH_EXTRA_EXPANDED;
			case ui::font_stretch::ultra_expanded:
				return DWRITE_FONT_STRETCH_ULTRA_EXPANDED;
			}
			return DWRITE_FONT_STRETCH_NORMAL; // should not be here
		}

		/// Casts a \ref ui::horizontal_text_alignment to a \p DWRITE_TEXT_ALIGNMENT.
		[[nodiscard]] DWRITE_TEXT_ALIGNMENT cast_horizontal_text_alignment(ui::horizontal_text_alignment align) {
			switch (align) {
			case ui::horizontal_text_alignment::center:
				return DWRITE_TEXT_ALIGNMENT_CENTER;
			case ui::horizontal_text_alignment::front:
				return DWRITE_TEXT_ALIGNMENT_LEADING;
			case ui::horizontal_text_alignment::rear:
				return DWRITE_TEXT_ALIGNMENT_TRAILING;
			}
			return DWRITE_TEXT_ALIGNMENT_LEADING; // should not be here
		}
		/// Casts a \p DWRITE_TEXT_ALIGNMENT back to a \ref ui::horizontal_text_alignment.
		[[nodiscard]] ui::horizontal_text_alignment cast_horizontal_text_alignment_back(
			DWRITE_TEXT_ALIGNMENT align
		) {
			switch (align) {
			case DWRITE_TEXT_ALIGNMENT_CENTER:
				return ui::horizontal_text_alignment::center;
			case DWRITE_TEXT_ALIGNMENT_LEADING:
				return ui::horizontal_text_alignment::front;
			case DWRITE_TEXT_ALIGNMENT_TRAILING:
				return ui::horizontal_text_alignment::rear;
			}
			return ui::horizontal_text_alignment::front; // should not be here
		}

		/// Casts a \ref ui::vertical_text_alignment to a \p DWRITE_PARAGRAPH_ALIGNMENT.
		[[nodiscard]] DWRITE_PARAGRAPH_ALIGNMENT cast_vertical_text_alignment(ui::vertical_text_alignment align) {
			switch (align) {
			case ui::vertical_text_alignment::top:
				return DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
			case ui::vertical_text_alignment::center:
				return DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
			case ui::vertical_text_alignment::bottom:
				return DWRITE_PARAGRAPH_ALIGNMENT_FAR;
			}
			return DWRITE_PARAGRAPH_ALIGNMENT_NEAR; // should not be here
		}
		/// Casts a \p DWRITE_PARAGRAPH_ALIGNMENT back to a \ref ui::vertical_text_alignment.
		[[nodiscard]] ui::vertical_text_alignment cast_vertical_text_alignment_back(DWRITE_PARAGRAPH_ALIGNMENT align) {
			switch (align) {
			case DWRITE_PARAGRAPH_ALIGNMENT_NEAR:
				return ui::vertical_text_alignment::top;
			case DWRITE_PARAGRAPH_ALIGNMENT_CENTER:
				return ui::vertical_text_alignment::center;
			case DWRITE_PARAGRAPH_ALIGNMENT_FAR:
				return ui::vertical_text_alignment::bottom;
			}
			return ui::vertical_text_alignment::top; // should not be here
		}

		/// Casts a \ref ui::wrapping_mode to a \p DWRITE_WORD_WRAPPING.
		[[nodiscard]] DWRITE_WORD_WRAPPING cast_wrapping_mode(ui::wrapping_mode wrap) {
			switch (wrap) {
			case ui::wrapping_mode::none:
				return DWRITE_WORD_WRAPPING_NO_WRAP;
			case ui::wrapping_mode::wrap:
				return DWRITE_WORD_WRAPPING_WRAP;
			}
			return DWRITE_WORD_WRAPPING_NO_WRAP; // should not be here
		}
		/// Casts a \p DWRITE_WORD_WRAPPING back to a \ref ui::wrapping_mode.
		[[nodiscard]] ui::wrapping_mode cast_wrapping_mode_back(DWRITE_WORD_WRAPPING wrap) {
			switch (wrap) {
			case DWRITE_WORD_WRAPPING_NO_WRAP:
				return ui::wrapping_mode::none;
			case DWRITE_WORD_WRAPPING_WRAP:
				return ui::wrapping_mode::wrap;
			}
			return ui::wrapping_mode::none; // should not be here
		}

		/// Underlying implementation of various <tt>cast_*</tt> functions.
		template <typename To, typename From> [[nodiscard]] To &cast_object(From &f) {
#ifdef CP_CHECK_LOGICAL_ERRORS
			To *res = dynamic_cast<To*>(&f);
			assert_true_logical(res, "invalid object type");
			return *res;
#else
			return *static_cast<To*>(&f);
#endif
		}

		/// Casts a \ref ui::render_target to a \ref render_target.
		[[nodiscard]] render_target &cast_render_target(ui::render_target &t) {
			return cast_object<render_target>(t);
		}
		/// Casts a \ref ui::bitmap to a \ref bitmap.
		[[nodiscard]] bitmap &cast_bitmap(ui::bitmap &b) {
			return cast_object<bitmap>(b);
		}
		/// Casts a \ref ui::formatted_text to a \ref formatted_text.
		[[nodiscard]] const formatted_text &cast_formatted_text(const ui::formatted_text &t) {
			return cast_object<const formatted_text>(t);
		}
		/// Casts a \ref ui::font to a \ref font.
		[[nodiscard]] font &cast_font(ui::font &f) {
			return cast_object<font>(f);
		}
		/// Casts a \ref ui::font_family to a \ref font_family.
		[[nodiscard]] font_family &cast_font_family(ui::font_family &f) {
			return cast_object<font_family>(f);
		}
		/// Casts a \ref ui::plain_text to a \ref plain_text.
		[[nodiscard]] const plain_text &cast_plain_text(const ui::plain_text &f) {
			return cast_object<const plain_text>(f);
		}
	}


	rectd formatted_text::get_layout() const {
		DWRITE_TEXT_METRICS metrics;
		_details::com_check(_text->GetMetrics(&metrics));
		return rectd::from_xywh(
			metrics.left, metrics.top, metrics.widthIncludingTrailingWhitespace, metrics.height
		);
	}

	std::vector<ui::line_metrics> formatted_text::get_line_metrics() const {
		constexpr std::size_t small_buffer_size = 5;

		DWRITE_LINE_METRICS small_buffer[small_buffer_size], *bufptr = small_buffer;
		std::vector<DWRITE_LINE_METRICS> large_buffer;
		UINT32 linecount;
		HRESULT res = _text->GetLineMetrics(small_buffer, small_buffer_size, &linecount);
		auto ressize = static_cast<std::size_t>(linecount);
		if (res == E_NOT_SUFFICIENT_BUFFER) {
			large_buffer.resize(ressize);
			bufptr = large_buffer.data();
			_details::com_check(_text->GetLineMetrics(bufptr, linecount, &linecount));
		} else {
			_details::com_check(res);
		}

		std::vector<ui::line_metrics> resvec;
		resvec.reserve(ressize);
		std::size_t position = 0, word_position = 0;
		for (std::size_t i = 0; i < ressize; ++i) {
			ui::line_metrics &line = resvec.emplace_back();
			line.height = bufptr[i].height;
			line.baseline = bufptr[i].baseline;

			word_position += bufptr[i].length - bufptr[i].newlineLength;
			std::size_t end = _word_index_to_char_index(word_position);
			word_position += bufptr[i].newlineLength;
			std::size_t linebreak_end = _word_index_to_char_index(word_position);

			line.non_linebreak_characters = end - position;
			line.linebreak_characters = linebreak_end - end;

			position = linebreak_end;
		}
		return resvec;
	}

	ui::caret_hit_test_result formatted_text::hit_test(vec2d pos) const {
		return _hit_test_impl(static_cast<FLOAT>(pos.x), static_cast<FLOAT>(pos.y));
	}

	ui::caret_hit_test_result formatted_text::hit_test_at_line(std::size_t line, double x) const {
		DWRITE_TEXT_METRICS metrics;
		_details::com_check(_text->GetMetrics(&metrics));
		// if `line` is larger than the number of lines, do this hit test on the last of the line
		line = std::min(line, static_cast<std::size_t>(metrics.lineCount - 1));
		std::vector<DWRITE_LINE_METRICS> line_metrics(static_cast<std::size_t>(metrics.lineCount));
		UINT32 num_lines = 0;
		HRESULT res = _text->GetLineMetrics(line_metrics.data(), metrics.lineCount, &num_lines);
		_details::com_check(res);
		// find the y position for this hit test
		FLOAT y = metrics.top;
		for (std::size_t i = 0; i < line; ++i) {
			y += line_metrics[i].height;
		}
		y += 0.5f * line_metrics[line].height;
		// here we clamp the x position to the text area because the caller may use +-inf/max as the horizontal
		// position which may in turn lead to strange results from hit detection
		FLOAT clamped_x = std::clamp(
			static_cast<FLOAT>(x),
			metrics.left,
			metrics.left + metrics.widthIncludingTrailingWhitespace
		);
		return _hit_test_impl(clamped_x, y);
	}

	rectd formatted_text::get_character_placement(std::size_t pos) const {
		FLOAT px, py;
		DWRITE_HIT_TEST_METRICS metrics;
		_details::com_check(_text->HitTestTextPosition(
			static_cast<UINT32>(_char_index_to_word_index(pos)), false, &px, &py, &metrics
		));
		rectd result = rectd::from_xywh(metrics.left, metrics.top, metrics.width, metrics.height);
		if (metrics.bidiLevel % 2 != 0) {
			std::swap(result.xmin, result.xmax);
		}
		return result;
	}

	std::vector<rectd> formatted_text::get_character_range_placement(std::size_t beg, std::size_t len) const {
		constexpr UINT32 static_buffer_size = 20;
		static DWRITE_HIT_TEST_METRICS static_buffer[static_buffer_size];

		DWRITE_TEXT_RANGE range = _make_text_range(beg, len);
		UINT32 count = 0;
		HRESULT hres = _text->HitTestTextRange(
			range.startPosition, range.length, 0.0f, 0.0f, static_buffer, static_buffer_size, &count
		);
		const DWRITE_HIT_TEST_METRICS *result_ptr = static_buffer;
		std::vector<DWRITE_HIT_TEST_METRICS> buffer;
		if (hres == E_NOT_SUFFICIENT_BUFFER) {
			buffer.resize(count);
			_details::com_check(_text->HitTestTextRange(
				range.startPosition, range.length, 0.0f, 0.0f, buffer.data(), count, &count
			));
			result_ptr = buffer.data();
		} else {
			_details::com_check(hres);
		}

		// move into result vector
		std::vector<rectd> result;
		result.reserve(count);
		for (UINT32 i = 0; i < count; ++i) {
			result.emplace_back(rectd::from_xywh(
				result_ptr[i].left, result_ptr[i].top, result_ptr[i].width, result_ptr[i].height
			));
		}
		return result;
	}

	vec2d formatted_text::get_layout_size() const {
		return vec2d(_text->GetMaxWidth(), _text->GetMaxHeight());
	}

	void formatted_text::set_layout_size(vec2d sz) {
		_details::com_check(_text->SetMaxWidth(static_cast<FLOAT>(std::max(sz.x, 0.0))));
		_details::com_check(_text->SetMaxHeight(static_cast<FLOAT>(std::max(sz.y, 0.0))));
	}

	ui::horizontal_text_alignment formatted_text::get_horizontal_alignment() const {
		return _details::cast_horizontal_text_alignment_back(_text->GetTextAlignment());
	}

	void formatted_text::set_horizontal_alignment(ui::horizontal_text_alignment halign) {
		_details::com_check(_text->SetTextAlignment(_details::cast_horizontal_text_alignment(halign)));
	}

	ui::vertical_text_alignment formatted_text::get_vertical_alignment() const {
		return _details::cast_vertical_text_alignment_back(_text->GetParagraphAlignment());
	}

	void formatted_text::set_vertical_alignment(ui::vertical_text_alignment valign) {
		_details::com_check(_text->SetParagraphAlignment(_details::cast_vertical_text_alignment(valign)));
	}

	ui::wrapping_mode formatted_text::get_wrapping_mode() const {
		return _details::cast_wrapping_mode_back(_text->GetWordWrapping());
	}

	void formatted_text::set_wrapping_mode(ui::wrapping_mode wrap) {
		_details::com_check(_text->SetWordWrapping(_details::cast_wrapping_mode(wrap)));
	}

	void formatted_text::set_text_color(colord c, std::size_t beg, std::size_t len) {
		_details::com_wrapper<ID2D1SolidColorBrush> brush;
		_rend._d2d_device_context->CreateSolidColorBrush(_details::cast_color(c), brush.get_ref());
		_details::com_check(_text->SetDrawingEffect(brush.get(), _make_text_range(beg, len)));
	}

	void formatted_text::set_font_family(const std::u8string &family, std::size_t beg, std::size_t len) {
		_details::com_check(_text->SetFontFamilyName(
			_details::utf8_to_wstring(family).c_str(), _make_text_range(beg, len)
		));
	}

	void formatted_text::set_font_size(double size, std::size_t beg, std::size_t len) {
		_details::com_check(_text->SetFontSize(static_cast<FLOAT>(size), _make_text_range(beg, len)));
	}

	void formatted_text::set_font_style(ui::font_style style, std::size_t beg, std::size_t len) {
		_details::com_check(_text->SetFontStyle(
			_details::cast_font_style(style), _make_text_range(beg, len)
		));
	}

	void formatted_text::set_font_weight(ui::font_weight weight, std::size_t beg, std::size_t len) {
		_details::com_check(_text->SetFontWeight(
			_details::cast_font_weight(weight), _make_text_range(beg, len)
		));
	}

	void formatted_text::set_font_stretch(ui::font_stretch stretch, std::size_t beg, std::size_t len) {
		_details::com_check(_text->SetFontStretch(
			_details::cast_font_stretch(stretch), _make_text_range(beg, len)
		));
	}

	std::size_t formatted_text::_char_index_to_word_index(std::size_t i) const {
		auto iter = std::lower_bound(_surrogate_chars.begin(), _surrogate_chars.end(), i);
		return std::max(i, i + (iter - _surrogate_chars.begin()));
	}

	std::size_t formatted_text::_word_index_to_char_index(std::size_t i) const {
		if (_surrogate_chars.empty() || i <= _surrogate_chars[0]) {
			return i;
		}

		auto beg = _surrogate_chars.begin(), end = _surrogate_chars.end();
		std::size_t prev_surrogate_word_index = _surrogate_chars[0];
		while (end != beg + 1) {
			auto mid = beg + (end - beg) / 2;
			prev_surrogate_word_index = *mid + (mid - beg);
			if (prev_surrogate_word_index < i) {
				beg = mid;
			} else {
				end = mid;
			}
		}

		std::size_t offset = i - (prev_surrogate_word_index + 1);
		if (offset == 0) {
			logger::get().log_warning(CP_HERE) << "word index is at second word of a surrogate pair: " << i;
		}
		return *beg + offset;
	}

	DWRITE_TEXT_RANGE formatted_text::_make_text_range(std::size_t beg, std::size_t len) const {
		std::size_t word_beg = _char_index_to_word_index(beg), word_end = beg + len;
		if (std::numeric_limits<std::size_t>::max() - len < beg) {
			word_end = std::numeric_limits<std::size_t>::max();
		}
		word_end = _char_index_to_word_index(word_end);

		DWRITE_TEXT_RANGE result;
		result.startPosition = static_cast<UINT32>(word_beg);
		// this works for size_t::max() as well since both types are unsigned
		result.length = static_cast<UINT32>(word_end - word_beg);
		return result;
	}

	ui::caret_hit_test_result formatted_text::_hit_test_impl(FLOAT x, FLOAT y) const {
		BOOL trailing, inside;
		DWRITE_HIT_TEST_METRICS metrics;
		_details::com_check(_text->HitTestPoint(x, y, &trailing, &inside, &metrics));
		rectd placement = rectd::from_xywh(metrics.left, metrics.top, metrics.width, metrics.height);
		if (metrics.bidiLevel % 2 == 1) {
			std::swap(placement.xmin, placement.xmax);
		}
		return ui::caret_hit_test_result(
			_word_index_to_char_index(static_cast<std::size_t>(metrics.textPosition)),
			placement,
			metrics.bidiLevel % 2 == 1, trailing != 0
		);
	}


	std::shared_ptr<ui::font> font_family::get_matching_font(
		ui::font_style style, ui::font_weight weight, ui::font_stretch stretch
	) const {
		auto [it, inserted] = _cache.try_emplace(std::make_tuple(style, weight, stretch));
		if (inserted) {
			auto result = std::make_shared<font>();
			_details::com_check(_family->GetFirstMatchingFont(
				_details::cast_font_weight(weight),
				_details::cast_font_stretch(stretch),
				_details::cast_font_style(style),
				result->_font.get_ref()
			));
			_details::com_check(result->_font->CreateFontFace(result->_font_face.get_ref()));
			result->_font_face->GetMetrics(&result->_metrics);

			it->second = result;
		}
		return it->second;
	}


	// - one-to-one mapping
	//   - glyph to char: 1 2 3
	//   - char to glyph: 1 2 3
	// - one char to many glyphos
	//   - glyph to char: 1 2 2 2 3
	//   - char to glyph: 1 2     5
	// - many chars to one glyph
	//   - glyph to char: 1 2     5
	//   - char to glyph: 1 2 2 2 3
	// - many chars to many glhphs
	//   - glyph to char: 1 2 2 2 4
	//   - char to glyph: 1 2 2   5
	ui::caret_hit_test_result plain_text::hit_test(double xpos) const {
		_maybe_calculate_glyph_positions();
		_maybe_calculate_glyph_backmapping();

		ui::caret_hit_test_result result;

		// find glyph index
		auto glyphit = std::upper_bound(_cached_glyph_positions.begin(), _cached_glyph_positions.end(), xpos);
		if (glyphit != _cached_glyph_positions.begin()) {
			--glyphit;
		}
		std::size_t glyphid = glyphit - _cached_glyph_positions.begin();

		if (glyphid < _glyph_count) {
			_cluster_analysis cluster = _analyze_at_glyph(glyphid);
			// divide the cluster evenly among the characters
			double tot_len =
				_cached_glyph_positions[cluster.past_last_glyph()] - _cached_glyph_positions[cluster.first_glyph];
			double ratio = (xpos - _cached_glyph_positions[cluster.first_glyph]) / tot_len;
			ratio *= cluster.num_chars;
			std::size_t offset = std::min(static_cast<std::size_t>(ratio), cluster.num_chars - 1);

			result.character = cluster.first_char + offset;
			result.rear = ratio - offset > 0.5;
			result.character_layout = _get_character_placement_impl(cluster, offset);
		} else { // past the end
			result.character = _char_count;
			result.rear = false;
			result.character_layout = _get_character_placement_impl(_analyze_at_glyph(_glyph_count), 0);
		}

		return result;
	}

	rectd plain_text::get_character_placement(std::size_t pos) const {
		_maybe_calculate_glyph_positions();
		_maybe_calculate_glyph_backmapping();

		std::size_t glyphid = _glyph_count, offset = 0;
		if (pos < _char_count) {
			glyphid = _cluster_map[pos];
			offset = pos - _cached_glyph_to_char_mapping[glyphid];
		}
		return _get_character_placement_impl(_analyze_at_glyph(glyphid), offset);
	}

	void plain_text::_maybe_calculate_glyph_positions() const {
		if (_cached_glyph_positions.empty()) {
			_cached_glyph_positions.reserve(_glyph_count + 1);
			double pos = 0.0;
			for (std::size_t i = 0; i < _glyph_count; ++i) {
				_cached_glyph_positions.emplace_back(pos);
				pos += static_cast<double>(_glyph_advances[i]);
			}
			_cached_glyph_positions.emplace_back(pos);
		}
	}

	void plain_text::_maybe_calculate_glyph_backmapping() const {
		if (_cached_glyph_to_char_mapping.empty()) {
			// compute inverse map
			_cached_glyph_to_char_mapping.resize(_glyph_count, 0);
			// fill in 'keys' in reverse so the smaller values override the larger ones
			for (std::size_t i = _char_count; i > 0; ) {
				--i;
				_cached_glyph_to_char_mapping[_cluster_map[i]] = i;
			}
			for (std::size_t i = 1; i < _glyph_count; ++i) { // extrapolate into the gaps
				_cached_glyph_to_char_mapping[i] = std::max(
					_cached_glyph_to_char_mapping[i - 1], _cached_glyph_to_char_mapping[i]
				);
			}
			_cached_glyph_to_char_mapping.emplace_back(_char_count);
		}
	}

	plain_text::_cluster_analysis plain_text::_analyze_at_glyph(std::size_t glyphid) const {
		_cluster_analysis result;
		if (glyphid < _glyph_count) {
			_maybe_calculate_glyph_backmapping();
			result.first_char = _cached_glyph_to_char_mapping[glyphid];
			result.first_glyph = _cluster_map[result.first_char];
			for (
				std::size_t c = result.first_char;
				c < _char_count && _cluster_map[c] == result.first_glyph;
				++c, ++result.num_chars
			) {
			}
			for (
				std::size_t g = result.first_glyph;
				g < _glyph_count && _cluster_map[g] == result.first_glyph;
				++g, ++result.num_glyphs
			) {
			}
		} else {
			result.first_char = _char_count;
			result.first_glyph = _glyph_count;
		}
		return result;
	}

	rectd plain_text::_get_character_placement_impl(_cluster_analysis anal, std::size_t charoffset) const {
		// horizontal
		double left = _cached_glyph_positions[anal.first_glyph], width = 0.0;
		if (anal.first_glyph < _glyph_count) {
			double total_width = _cached_glyph_positions[anal.past_last_glyph()] - left;
			width = total_width / anal.num_chars;
			left += width * charoffset;
		}

		// vertical
		DWRITE_FONT_METRICS metrics;
		_font_face->GetMetrics(&metrics);

		return rectd::from_xywh(
			left, 0.0,
			width, _font_size * (metrics.ascent + metrics.descent) / static_cast<double>(metrics.designUnitsPerEm)
		);
	}


	void path_geometry_builder::close() {
		if (_stroking) {
			_sink->EndFigure(D2D1_FIGURE_END_CLOSED);
			_stroking = false;
		}
	}

	void path_geometry_builder::move_to(vec2d pos) {
		if (_stroking) { // end current subpath first
			_sink->EndFigure(D2D1_FIGURE_END_OPEN);
		}
		_last_point = _details::cast_point(pos);
		_sink->BeginFigure(_last_point, D2D1_FIGURE_BEGIN_FILLED); // FIXME no additional information
		_stroking = true;
	}

	void path_geometry_builder::add_segment(vec2d to) {
		_on_stroke();
		_last_point = _details::cast_point(to);
		_sink->AddLine(_last_point);
	}

	void path_geometry_builder::add_cubic_bezier(vec2d to, vec2d control1, vec2d control2) {
		_on_stroke();
		_last_point = _details::cast_point(to);
		_sink->AddBezier(D2D1::BezierSegment(
			_details::cast_point(control1),
			_details::cast_point(control2),
			_last_point
		));
	}

	void path_geometry_builder::add_arc(
		vec2d to, vec2d radius, double rotation, ui::sweep_direction dir, ui::arc_type type
	) {
		_on_stroke();
		_last_point = _details::cast_point(to);
		_sink->AddArc(D2D1::ArcSegment(
			_last_point,
			D2D1::SizeF(static_cast<FLOAT>(radius.x), static_cast<FLOAT>(radius.y)),
			static_cast<FLOAT>(rotation) * (180.0f / 3.14159f), // in degrees
			dir == ui::sweep_direction::clockwise ?
			D2D1_SWEEP_DIRECTION_CLOCKWISE :
			D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
			type == ui::arc_type::minor ?
			D2D1_ARC_SIZE_SMALL :
			D2D1_ARC_SIZE_LARGE
		));
	}

	void path_geometry_builder::_start(ID2D1Factory *factory) {
		_details::com_check(factory->CreatePathGeometry(_geom.get_ref()));
		_details::com_check(_geom->Open(_sink.get_ref()));
		_stroking = false;
	}

	_details::com_wrapper<ID2D1PathGeometry> path_geometry_builder::_end() {
		if (_stroking) {
			_sink->EndFigure(D2D1_FIGURE_END_OPEN);
		}
		_details::com_check(_sink->Close());
		_sink.reset();
		_details::com_wrapper<ID2D1PathGeometry> result;
		std::swap(result, _geom);
		return result;
	}

	void path_geometry_builder::_on_stroke() {
		if (!_stroking) {
			_sink->BeginFigure(_last_point, D2D1_FIGURE_BEGIN_FILLED); // FIXME no additional information
			_stroking = true;
		}
	}


	renderer::_text_analysis renderer::_text_analysis::analyze(std::u8string_view text) {
		_text_analysis result;
		result.text.reserve(text.size() * 2);

		std::size_t i = 0;
		for (auto it = text.begin(); it != text.end(); ++i) {
			codepoint cp;
			if (!encodings::utf8::next_codepoint(it, text.end(), cp)) {
				cp = encodings::replacement_character;
			}
			auto encoded = encodings::utf16<>::encode_codepoint(cp);
			if (encoded.size() == 4) {
				result.surrogate.emplace_back(i);
			}
			result.text += encoded;
		}
		result.num_chars = i;

		return result;
	}

	renderer::_text_analysis renderer::_text_analysis::analyze(std::basic_string_view<codepoint> text) {
		_text_analysis result;
		result.num_chars = text.size();
		result.text.reserve(text.size() * 2);

		for (std::size_t i = 0; i < text.size(); ++i) {
			auto encoded = encodings::utf16<>::encode_codepoint(text[i]);
			if (encoded.size() == 4) {
				result.surrogate.emplace_back(i);
			}
			result.text += encoded;
		}

		return result;
	}


	renderer::renderer() {
		D3D_FEATURE_LEVEL supported_feature_levels[]{
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
			D3D_FEATURE_LEVEL_9_3,
			D3D_FEATURE_LEVEL_9_2,
			D3D_FEATURE_LEVEL_9_1
		};
		D3D_FEATURE_LEVEL created_feature_level;
		UINT create_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifndef NDEBUG
		create_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
		_details::com_check(D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			create_flags,
			supported_feature_levels,
			ARRAYSIZE(supported_feature_levels),
			D3D11_SDK_VERSION,
			_d3d_device.get_ref(),
			&created_feature_level,
			nullptr
		));
		logger::get().log_debug(CP_HERE) << "D3D feature level: " << created_feature_level;
		_details::com_check(_d3d_device->QueryInterface(_dxgi_device.get_ref()));

		_details::com_check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, _d2d_factory.get_ref()));
		_details::com_check(_d2d_factory->CreateDevice(_dxgi_device.get(), _d2d_device.get_ref()));
		_details::com_check(_d2d_device->CreateDeviceContext(
			D2D1_DEVICE_CONTEXT_OPTIONS_NONE, _d2d_device_context.get_ref()
		));
		_d2d_device_context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
		// create brush for text
		_details::com_check(_d2d_device_context->CreateSolidColorBrush(
			_details::cast_color(colord()), _text_brush.get_ref()
		));

		_details::com_check(DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory4),
			reinterpret_cast<IUnknown**>(_dwrite_factory.get_ref())
		));
		_details::com_check(_dwrite_factory->CreateTextAnalyzer(_dwrite_text_analyzer.get_ref()));
	}

	ui::render_target_data renderer::create_render_target(vec2d size, vec2d scaling_factor, colord c) {
		auto resrt = std::make_shared<render_target>();
		auto resbmp = std::make_shared<bitmap>();
		_details::com_wrapper<IDXGISurface> surface;

		D3D11_TEXTURE2D_DESC texture_desc;
		texture_desc.Width = static_cast<UINT>(std::max(std::ceil(size.x * scaling_factor.x), 1.0));
		texture_desc.Height = static_cast<UINT>(std::max(std::ceil(size.y * scaling_factor.y), 1.0));
		texture_desc.MipLevels = 1;
		texture_desc.ArraySize = 1;
		texture_desc.Format = pixel_format;
		texture_desc.SampleDesc.Count = 1;
		texture_desc.SampleDesc.Quality = 0;
		texture_desc.Usage = D3D11_USAGE_DEFAULT;
		texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		texture_desc.CPUAccessFlags = 0;
		texture_desc.MiscFlags = 0;
		_details::com_check(_d3d_device->CreateTexture2D(&texture_desc, nullptr, resrt->_texture.get_ref()));
		_details::com_check(resrt->_texture->QueryInterface(surface.get_ref()));
		_details::com_check(_d2d_device_context->CreateBitmapFromDxgiSurface(
			surface.get(),
			D2D1::BitmapProperties1(
				D2D1_BITMAP_OPTIONS_TARGET,
				D2D1::PixelFormat(pixel_format, D2D1_ALPHA_MODE_PREMULTIPLIED),
				static_cast<FLOAT>(USER_DEFAULT_SCREEN_DPI * scaling_factor.x),
				static_cast<FLOAT>(USER_DEFAULT_SCREEN_DPI * scaling_factor.y)
			),
			resrt->_bitmap.get_ref()
		));
		resbmp->_bitmap = resrt->_bitmap;
		{ // manually clear the bitmap
			begin_drawing(*resrt);
			clear(c);
			end_drawing();
		}
		return ui::render_target_data(std::move(resrt), std::move(resbmp));
	}

	std::shared_ptr<ui::bitmap> renderer::load_bitmap(const std::filesystem::path &bmp, vec2d scaling_factor) {
		auto res = std::make_shared<bitmap>();
		_details::com_wrapper<IWICBitmapSource> converted;
		_details::com_wrapper<IWICBitmapSource> img = _details::wic_image_loader::get().load_image(bmp);
		_details::com_check(WICConvertBitmapSource(GUID_WICPixelFormat32bppPBGRA, img.get(), converted.get_ref()));
		_details::com_check(_d2d_device_context->CreateBitmapFromWicBitmap(
			converted.get(),
			D2D1::BitmapProperties1(
				D2D1_BITMAP_OPTIONS_NONE,
				D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_UNKNOWN),
				static_cast<FLOAT>(scaling_factor.x * USER_DEFAULT_SCREEN_DPI),
				static_cast<FLOAT>(scaling_factor.y * USER_DEFAULT_SCREEN_DPI),
				nullptr
			),
			res->_bitmap.get_ref())
		);
		return res;
	}

	std::shared_ptr<ui::font_family> renderer::find_font_family(const std::u8string &family) {
		_details::com_wrapper<IDWriteFontCollection> fonts;
		_details::com_check(_dwrite_factory->GetSystemFontCollection(fonts.get_ref(), false)); // no need to hurry

		UINT32 index = 0;
		BOOL exist = false;
		_details::com_check(fonts->FindFamilyName(os::_details::utf8_to_wstring(family).c_str(), &index, &exist));
		if (!exist) {
			return nullptr;
		}

		auto res = std::make_shared<font_family>();
		_details::com_check(fonts->GetFontFamily(index, res->_family.get_ref()));
		return res;
	}

	void renderer::begin_drawing(ui::window &w) {
		auto &data = _get_window_data_as<_window_data>(w);
		_begin_draw_impl(data.target.get(), w.get_scaling_factor() * USER_DEFAULT_SCREEN_DPI);
		_present_chains.emplace(data.swap_chain.get());
	}

	void renderer::begin_drawing(ui::render_target &r) {
		_details::com_wrapper<ID2D1Bitmap1> &bitmap = _details::cast_render_target(r)._bitmap;
		FLOAT dpix, dpiy;
		bitmap->GetDpi(&dpix, &dpiy);
		_begin_draw_impl(bitmap.get(), vec2d(dpix, dpiy));
	}

	void renderer::end_drawing() {
		assert_true_usage(!_render_stack.empty(), "begin_drawing/end_drawing calls mismatch");
		assert_true_usage(_render_stack.top().matrices.size() == 1, "push_matrix/pop_matrix calls mismatch");
		_render_stack.pop();
		if (_render_stack.empty()) {
			_d2d_device_context->SetTarget(nullptr); // so that windows can be resized normally
			_details::com_check(_d2d_device_context->EndDraw());
			for (IDXGISwapChain *chain : _present_chains) {
				_details::com_check(chain->Present(0, 0));
			}
			_present_chains.clear();
		} else {
			_d2d_device_context->SetTarget(_render_stack.top().target);
			_update_transform();
		}
	}

	void renderer::clear(colord color) {
		_d2d_device_context->Clear(_details::cast_color(color));
	}

	void renderer::draw_ellipse(
		vec2d center, double radiusx, double radiusy,
		const ui::generic_brush &brush, const ui::generic_pen &pen
	) {
		_details::com_wrapper<ID2D1EllipseGeometry> geom;
		_details::com_check(_d2d_factory->CreateEllipseGeometry(
			D2D1::Ellipse(
				_details::cast_point(center), static_cast<FLOAT>(radiusx), static_cast<FLOAT>(radiusy)
			),
			geom.get_ref()
		));
		_draw_geometry(std::move(geom), brush, pen);
	}

	void renderer::draw_rectangle(
		rectd rect, const ui::generic_brush &brush, const ui::generic_pen &pen
	) {
		_details::com_wrapper<ID2D1RectangleGeometry> geom;
		_details::com_check(_d2d_factory->CreateRectangleGeometry(_details::cast_rect(rect), geom.get_ref()));
		_draw_geometry(std::move(geom), brush, pen);
	}

	void renderer::draw_rounded_rectangle(
		rectd region, double radiusx, double radiusy,
		const ui::generic_brush &brush, const ui::generic_pen &pen
	) {
		_details::com_wrapper<ID2D1RoundedRectangleGeometry> geom;
		_details::com_check(_d2d_factory->CreateRoundedRectangleGeometry(
			D2D1::RoundedRect(
				_details::cast_rect(region), static_cast<FLOAT>(radiusx), static_cast<FLOAT>(radiusy)
			),
			geom.get_ref()
		));
		_draw_geometry(std::move(geom), brush, pen);
	}

	void renderer::end_and_draw_path(
		const ui::generic_brush &brush, const ui::generic_pen &pen
	) {
		_draw_geometry(_path_builder._end(), brush, pen);
	}

	void renderer::push_ellipse_clip(vec2d center, double radiusx, double radiusy) {
		_details::com_wrapper<ID2D1EllipseGeometry> geom;
		_details::com_check(_d2d_factory->CreateEllipseGeometry(D2D1::Ellipse(
			_details::cast_point(center), static_cast<FLOAT>(radiusx), static_cast<FLOAT>(radiusy)
		), geom.get_ref()));
		_push_layer(std::move(geom));
	}

	void renderer::push_rectangle_clip(rectd rect) {
		_details::com_wrapper<ID2D1RectangleGeometry> geom;
		_details::com_check(_d2d_factory->CreateRectangleGeometry(_details::cast_rect(rect), geom.get_ref()));
		_push_layer(std::move(geom));
	}

	void renderer::push_rounded_rectangle_clip(rectd rect, double radiusx, double radiusy) {
		_details::com_wrapper<ID2D1RoundedRectangleGeometry> geom;
		_details::com_check(_d2d_factory->CreateRoundedRectangleGeometry(D2D1::RoundedRect(
			_details::cast_rect(rect), static_cast<FLOAT>(radiusx), static_cast<FLOAT>(radiusy)
		), geom.get_ref()));
		_push_layer(std::move(geom));
	}

	void renderer::end_and_push_path_clip() {
		_push_layer(_path_builder._end());
	}

	void renderer::pop_clip() {
		_d2d_device_context->PopLayer();
	}

	std::shared_ptr<ui::formatted_text> renderer::create_formatted_text(
		std::u8string_view text, const ui::font_parameters &params, colord c, vec2d maxsize, ui::wrapping_mode wrap,
		ui::horizontal_text_alignment halign, ui::vertical_text_alignment valign
	) {
		return _create_formatted_text_impl(
			_text_analysis::analyze(text), params, c, maxsize, wrap, halign, valign
		);
	}

	std::shared_ptr<ui::formatted_text> renderer::create_formatted_text(
		std::basic_string_view<codepoint> text, const ui::font_parameters &params, colord c,
		vec2d maxsize, ui::wrapping_mode wrap,
		ui::horizontal_text_alignment halign, ui::vertical_text_alignment valign
	) {
		return _create_formatted_text_impl(
			_text_analysis::analyze(text), params, c, maxsize, wrap, halign, valign
		);
	}

	void renderer::draw_formatted_text(const ui::formatted_text &text, vec2d topleft) {
		auto ctext = _details::cast_formatted_text(text);
		// don't really need this since _create_formatted_text_impl already covers all text, but just in case
		_text_brush->SetColor(_details::cast_color(colord(0.0, 0.0, 0.0, 1.0)));
		_d2d_device_context->DrawTextLayout(
			_details::cast_point(topleft),
			ctext._text.get(),
			_text_brush.get(),
			D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT
		);
	}

	std::shared_ptr<ui::plain_text> renderer::create_plain_text(
		std::u8string_view text, ui::font &font, double size
	) {
		auto utf16 = _details::utf8_to_wstring(text);
		return _create_plain_text_impl(utf16, font, size);
	}

	std::shared_ptr<ui::plain_text> renderer::create_plain_text(
		std::basic_string_view<codepoint> text, ui::font &font, double size
	) {
		std::basic_string<std::byte> bytestr;
		for (codepoint cp : text) {
			bytestr += encodings::utf16<>::encode_codepoint(cp);
		}
		return _create_plain_text_impl(
			std::basic_string_view<WCHAR>(reinterpret_cast<const WCHAR*>(bytestr.c_str()), bytestr.size() / 2),
			font, size
		);
	}

	std::shared_ptr<ui::plain_text> renderer::create_plain_text_fast(
		std::basic_string_view<codepoint> text, ui::font &font, double size
	) {
		return _create_plain_text_fast_impl(text, font, size);
	}

	void renderer::draw_plain_text(const ui::plain_text &t, vec2d pos, colord color) {
		auto &text = _details::cast_plain_text(t);

		// gather glyph run information
		DWRITE_GLYPH_RUN run;
		run.fontFace = text._font_face.get();
		run.fontEmSize = static_cast<FLOAT>(text._font_size);
		run.glyphCount = static_cast<UINT32>(text._glyph_count);
		run.glyphIndices = text._glyphs.get();
		run.glyphAdvances = text._glyph_advances.get();
		run.glyphOffsets = text._glyph_offsets.get();
		run.isSideways = false;
		run.bidiLevel = 0;

		// calculate baseline position
		DWRITE_FONT_METRICS metrics;
		text._font_face->GetMetrics(&metrics);
		pos.y += text._font_size * (metrics.ascent / static_cast<double>(metrics.designUnitsPerEm));

		// get scaling
		FLOAT dpix, dpiy;
		_render_stack.top().target->GetDpi(&dpix, &dpiy);
		vec2d scale = vec2d(dpix, dpiy) / USER_DEFAULT_SCREEN_DPI;

		matd3x3 trans = _render_stack.top().matrices.top();

		// check for colored glyphs
		// https://github.com/microsoft/Windows-universal-samples/blob/master/Samples/DWriteColorGlyph/cpp/CustomTextRenderer.cpp
		_details::com_wrapper<IDWriteColorGlyphRunEnumerator1> color_glyphs;
		DWRITE_MATRIX matrix = _details::cast_dwrite_matrix(matd3x3::scale(vec2d(), scale) * trans);
		HRESULT has_color = _dwrite_factory->TranslateColorGlyphRun(
			_details::cast_point(pos), &run, nullptr,
			DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE |
			DWRITE_GLYPH_IMAGE_FORMATS_CFF |
			DWRITE_GLYPH_IMAGE_FORMATS_COLR |
			DWRITE_GLYPH_IMAGE_FORMATS_SVG |
			DWRITE_GLYPH_IMAGE_FORMATS_PNG |
			DWRITE_GLYPH_IMAGE_FORMATS_JPEG |
			DWRITE_GLYPH_IMAGE_FORMATS_TIFF |
			DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8,
			DWRITE_MEASURING_MODE_NATURAL, &matrix, 0,
			color_glyphs.get_ref()
		);

		if (has_color == DWRITE_E_NOCOLOR) { // no color information, draw original glyph run
			_text_brush->SetColor(_details::cast_color(color)); // text color
			_d2d_device_context->DrawGlyphRun(_details::cast_point(pos), &run, _text_brush.get());
		} else { // draw color glyphs
			_details::com_check(has_color);
			while (true) {
				BOOL have_more = false;
				_details::com_check(color_glyphs->MoveNext(&have_more));
				if (!have_more) {
					break;
				}

				const DWRITE_COLOR_GLYPH_RUN1 *colored_run;
				_details::com_check(color_glyphs->GetCurrentRun(&colored_run));
				D2D1_POINT_2F baseline_origin = D2D1::Point2F(
					colored_run->baselineOriginX, colored_run->baselineOriginY
				);
				switch (colored_run->glyphImageFormat) {
				case DWRITE_GLYPH_IMAGE_FORMATS_PNG:
					[[fallthrough]];
				case DWRITE_GLYPH_IMAGE_FORMATS_JPEG:
					[[fallthrough]];
				case DWRITE_GLYPH_IMAGE_FORMATS_TIFF:
					[[fallthrough]];
				case DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8:
					_d2d_device_context->DrawColorBitmapGlyphRun(
						colored_run->glyphImageFormat, baseline_origin, &colored_run->glyphRun
					);
					break;

				case DWRITE_GLYPH_IMAGE_FORMATS_SVG:
					{
						auto brush = _create_brush(ui::brushes::solid_color(color));
						_d2d_device_context->DrawSvgGlyphRun(
							baseline_origin, &colored_run->glyphRun, brush.get()
						);
					}
					break;

				case DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE:
					[[fallthrough]];
				case DWRITE_GLYPH_IMAGE_FORMATS_CFF:
					[[fallthrough]];
				case DWRITE_GLYPH_IMAGE_FORMATS_COLR:
					[[fallthrough]];
				default: // draw as solid color glyph run by default, as in the example code
					{
						colord run_color = color;
						if (colored_run->paletteIndex != 0xFFFF) {
							run_color = colord(
								colored_run->runColor.r,
								colored_run->runColor.g,
								colored_run->runColor.b,
								colored_run->runColor.a
							);
						}
						auto brush = _create_brush(ui::brushes::solid_color(run_color));
						_d2d_device_context->DrawGlyphRun(baseline_origin, &colored_run->glyphRun, brush.get());
					}
					break;
				}
			}
		}
	}

	void renderer::_begin_draw_impl(ID2D1Bitmap1 *target, vec2d dpi) {
		_d2d_device_context->SetTarget(target);
		_d2d_device_context->SetDpi(static_cast<FLOAT>(dpi.x), static_cast<FLOAT>(dpi.y));
		if (_render_stack.empty()) {
			_d2d_device_context->BeginDraw();
		}
		_render_stack.emplace(target);
		_update_transform();
	}

	void renderer::_update_transform() {
		_d2d_device_context->SetTransform(_details::cast_matrix(_render_stack.top().matrices.top()));
	}

	void renderer::_draw_geometry(
		_details::com_wrapper<ID2D1Geometry> geom,
		const ui::generic_brush &brush_def,
		const ui::generic_pen &pen_def
	) {
		if (_details::com_wrapper<ID2D1Brush> brush = _create_brush(brush_def)) {
			_d2d_device_context->FillGeometry(geom.get(), brush.get());
		}
		if (_details::com_wrapper<ID2D1Brush> pen = _create_brush(pen_def.brush)) {
			_d2d_device_context->DrawGeometry(geom.get(), pen.get(), static_cast<FLOAT>(pen_def.thickness));
		}
	}

	void renderer::_push_layer(_details::com_wrapper<ID2D1Geometry> clip) {
		// since the layer is initialized from the background, we temporarily change the blending mode to copy so
		// that everything blends correctly and subpixel antialiasing is properly enabled
		// also the mode is set here instead of during PopLayer() as per the documentation at:
		// https://docs.microsoft.com/en-us/windows/win32/direct2d/direct2d-layers-overview#blend-modes
		_d2d_device_context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
		// the overload that accepts a D2D1::LayerParameters has an option
		// D2D1_LAYER_OPTIONS_INITIALIZE_FOR_CLEARTYPE which has been deprecated
		_d2d_device_context->PushLayer(D2D1::LayerParameters1(
			D2D1::InfiniteRect(), clip.get(), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
			D2D1::IdentityMatrix(), 1.0f, nullptr, D2D1_LAYER_OPTIONS1_INITIALIZE_FROM_BACKGROUND
		), nullptr);
		_d2d_device_context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
	}

	_details::com_wrapper<ID2D1SolidColorBrush> renderer::_create_brush(
		const ui::brushes::solid_color &brush_def
	) {
		_details::com_wrapper<ID2D1SolidColorBrush> brush;
		_details::com_check(_d2d_device_context->CreateSolidColorBrush(
			_details::cast_color(brush_def.color), brush.get_ref()
		));
		return brush;
	}

	_details::com_wrapper<ID2D1GradientStopCollection> renderer::_create_gradient_stop_collection(
		const std::vector<ui::gradient_stop> &stops_def
	) {
		_details::com_wrapper<ID2D1GradientStopCollection> gradients;
		std::vector<D2D1_GRADIENT_STOP> stops;
		for (const auto &s : stops_def) {
			stops.emplace_back(D2D1::GradientStop(static_cast<FLOAT>(s.position), _details::cast_color(s.color)));
		}
		_details::com_check(_d2d_device_context->CreateGradientStopCollection(
			stops.data(), static_cast<UINT32>(stops.size()), D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP,
			gradients.get_ref()
		)); // TODO clamp mode
		return gradients;
	}

	_details::com_wrapper<ID2D1LinearGradientBrush> renderer::_create_brush(
		const ui::brushes::linear_gradient &brush_def
	) {
		_details::com_wrapper<ID2D1LinearGradientBrush> brush;
		if (brush_def.gradients) {
			_details::com_check(_d2d_device_context->CreateLinearGradientBrush(
				D2D1::LinearGradientBrushProperties(
					_details::cast_point(brush_def.from), _details::cast_point(brush_def.to)
				),
				_create_gradient_stop_collection(*brush_def.gradients).get(),
				brush.get_ref()
			));
		}
		return brush;
	}

	_details::com_wrapper<ID2D1RadialGradientBrush> renderer::_create_brush(
		const ui::brushes::radial_gradient &brush_def
	) {
		_details::com_wrapper<ID2D1RadialGradientBrush> brush;
		if (brush_def.gradients) {
			_details::com_check(_d2d_device_context->CreateRadialGradientBrush(
				D2D1::RadialGradientBrushProperties(
					_details::cast_point(brush_def.center), D2D1::Point2F(),
					static_cast<FLOAT>(brush_def.radius), static_cast<FLOAT>(brush_def.radius)
				),
				_create_gradient_stop_collection(*brush_def.gradients).get(),
				brush.get_ref()
			));
		}
		return brush;
	}

	_details::com_wrapper<ID2D1BitmapBrush> renderer::_create_brush(
		const ui::brushes::bitmap_pattern &brush_def
	) {
		_details::com_wrapper<ID2D1BitmapBrush> brush;
		if (brush_def.image) {
			_details::com_check(_d2d_device_context->CreateBitmapBrush(
				_details::cast_bitmap(*brush_def.image)._bitmap.get(),
				D2D1::BitmapBrushProperties(), // TODO extend modes
				brush.get_ref()
			));
		}
		return brush;
	}

	_details::com_wrapper<ID2D1Brush> renderer::_create_brush(const ui::brushes::none&) {
		return _details::com_wrapper<ID2D1Brush>();
	}

	_details::com_wrapper<ID2D1Brush> renderer::_create_brush(const ui::generic_brush &b) {
		auto brush = std::visit([this](auto &&brush) {
			return _details::com_wrapper<ID2D1Brush>(_create_brush(brush));
			}, b.value);
		if (brush) {
			brush->SetTransform(_details::cast_matrix(b.transform));
		}
		return brush;
	}


	std::shared_ptr<formatted_text> renderer::_create_formatted_text_impl(
		_text_analysis analysis, const ui::font_parameters &fmt, colord c,
		vec2d maxsize, ui::wrapping_mode wrap,
		ui::horizontal_text_alignment halign, ui::vertical_text_alignment valign
	) {
		// use new to access protedted constructor
		auto res = std::make_shared<formatted_text>(*this, std::move(analysis.surrogate), analysis.num_chars);

		_details::com_wrapper<IDWriteTextFormat> format;
		_details::com_check(_dwrite_factory->CreateTextFormat(
			_details::utf8_to_wstring(fmt.family).c_str(),
			nullptr,
			_details::cast_font_weight(fmt.weight),
			_details::cast_font_style(fmt.style),
			_details::cast_font_stretch(fmt.stretch),
			static_cast<FLOAT>(fmt.size),
			L"", // FIXME is this good practice?
			format.get_ref()
		));
		_details::com_check(_dwrite_factory->CreateTextLayout(
			reinterpret_cast<const WCHAR*>(analysis.text.data()), static_cast<UINT32>(analysis.text.size() / 2),
			format.get(), static_cast<FLOAT>(std::max(maxsize.x, 0.0)), static_cast<FLOAT>(std::max(maxsize.y, 0.0)),
			res->_text.get_ref()
		));
		res->set_wrapping_mode(wrap);
		res->set_horizontal_alignment(halign);
		res->set_vertical_alignment(valign);
		res->set_text_color(c, 0, std::numeric_limits<std::size_t>::max());
		return res;
	}

	std::shared_ptr<plain_text> renderer::_create_plain_text_impl(
		std::basic_string_view<WCHAR> text, ui::font &f, double size
	) {
		auto result = std::make_shared<plain_text>();
		result->_font_face = _details::cast_font(f)._font_face;
		result->_font_size = size;

		// script & font features
		DWRITE_SCRIPT_ANALYSIS script;
		script.script = 215; // 215 for latin script
		script.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT;

		// TODO more features & customizable?
		DWRITE_FONT_FEATURE features[] = {
			{ DWRITE_FONT_FEATURE_TAG_KERNING, 1 },
			{ DWRITE_FONT_FEATURE_TAG_REQUIRED_LIGATURES, 1 }, // rlig
			{ DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 1 }, // liga
			{ DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_LIGATURES, 1 }, // clig
			{ DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_ALTERNATES, 1 } // calt
		};

		DWRITE_TYPOGRAPHIC_FEATURES feature_list;
		feature_list.features = features;
		feature_list.featureCount = static_cast<UINT32>(std::size(features));
		const DWRITE_TYPOGRAPHIC_FEATURES *pfeature_list = &feature_list;
		UINT32 feature_range_length = static_cast<UINT32>(text.size());

		// get glyphs
		std::unique_ptr<DWRITE_SHAPING_TEXT_PROPERTIES[]> text_props;
		std::unique_ptr<DWRITE_SHAPING_GLYPH_PROPERTIES[]> glyph_props;
		UINT32 glyph_count = 0;

		result->_cluster_map = std::make_unique<UINT16[]>(text.size());
		text_props = std::make_unique<DWRITE_SHAPING_TEXT_PROPERTIES[]>(text.size());

		for (std::size_t length = text.size(); ; length *= 2) {
			// resize buffers
			result->_glyphs = std::make_unique<UINT16[]>(length);
			glyph_props = std::make_unique<DWRITE_SHAPING_GLYPH_PROPERTIES[]>(length);

			HRESULT res = _dwrite_text_analyzer->GetGlyphs(
				text.data(), static_cast<UINT32>(text.size()), result->_font_face.get(), false, false,
				&script, nullptr, nullptr, &pfeature_list, &feature_range_length, 1,
				static_cast<UINT32>(length),
				result->_cluster_map.get(), text_props.get(), result->_glyphs.get(), glyph_props.get(),
				&glyph_count
			);
			if (res == S_OK) {
				break;
			}
			if (res != E_NOT_SUFFICIENT_BUFFER) {
				_details::com_check(res); // doomed to fail
			}
		}
		result->_glyph_count = static_cast<std::size_t>(glyph_count);

		// get glyph placements
		result->_glyph_advances = std::make_unique<FLOAT[]>(result->_glyph_count);
		result->_glyph_offsets = std::make_unique<DWRITE_GLYPH_OFFSET[]>(result->_glyph_count);

		_details::com_check(_dwrite_text_analyzer->GetGlyphPlacements(
			text.data(), result->_cluster_map.get(), text_props.get(), static_cast<UINT32>(text.size()),
			result->_glyphs.get(), glyph_props.get(), glyph_count,
			result->_font_face.get(), static_cast<FLOAT>(size), false, false,
			&script, nullptr, &pfeature_list, &feature_range_length, 1,
			result->_glyph_advances.get(), result->_glyph_offsets.get()
		));

		// since directwrite treats a surrogate pair (one codepoint) as two characters, here we find all
		// surrogate pairs, remove duplicate entries in the cluster map, and correct the number of characters
		// TODO this has not been thoroughly tested
		result->_char_count = text.size();
		for (std::size_t i = 0, reduced = 0; i < text.size(); ++i, ++reduced) {
			// if the last unit in the text starts a surrogate pair, then the codepoint is invalid; however this
			// means that there's no duplicate stuff so nothing needs to be done
			if (i + 1 < text.size()) {
				if ( // check that the current unit starts a surrogate pair
					(static_cast<std::uint16_t>(text[i]) & encodings::utf16<>::mask_pair) ==
					encodings::utf16<>::patt_pair
				) {
					// TODO do we check if the next unit is also part of the surrogate pair?
					assert_true_sys(
						result->_cluster_map[i] == result->_cluster_map[i + 1],
						"different glyphs for surrogate pair"
					);

					++i;
					--result->_char_count;
				}
			}
			result->_cluster_map[reduced] = result->_cluster_map[i];
		}

		// make the cluster map monotonic
		if (result->_char_count > 0) {
			for (std::size_t i = result->_char_count - 1; i > 0; ) {
				--i;
				if (result->_cluster_map[i + 1] < result->_cluster_map[i]) {
					logger::get().log_warning(CP_HERE) << "non-monotonic cluster map encountered at position " << i;
					result->_cluster_map[i] = result->_cluster_map[i + 1];
				}
			}
		}

		return result;
	}

	std::shared_ptr<plain_text> renderer::_create_plain_text_fast_impl(
		std::basic_string_view<codepoint> text, ui::font &fnt, double size
	) {
		auto &dwfnt = _details::cast_font(fnt);

		auto result = std::make_shared<plain_text>();
		result->_font_face = dwfnt._font_face;
		result->_font_size = size;
		result->_char_count = result->_glyph_count = text.size();

		result->_glyphs = std::make_unique<UINT16[]>(text.size());
		result->_cluster_map = std::make_unique<UINT16[]>(text.size());
		result->_glyph_advances = std::make_unique<FLOAT[]>(text.size());
		result->_glyph_offsets = std::make_unique<DWRITE_GLYPH_OFFSET[]>(text.size());
		for (std::size_t i = 0; i < text.size(); ++i) {
			codepoint cp = text[i];

			auto [it, inserted] = dwfnt._cached_glyph_info.try_emplace(cp);
			if (inserted) {
				_details::com_check(dwfnt._font_face->GetGlyphIndices(&cp, 1, &it->second.glyph_index));
				_details::com_check(
					dwfnt._font_face->GetDesignGlyphMetrics(&it->second.glyph_index, 1, &it->second.metrics)
				);
			}
			result->_cluster_map[i] = static_cast<UINT16>(i);
			result->_glyphs[i] = it->second.glyph_index;
			result->_glyph_advances[i] = static_cast<FLOAT>(
				size * it->second.metrics.advanceWidth / static_cast<double>(dwfnt._metrics.designUnitsPerEm)
			);
			result->_glyph_offsets[i] = DWRITE_GLYPH_OFFSET{ .advanceOffset = 0.0f, .ascenderOffset = 0.0f };
		}

		return result;
	}


	_details::com_wrapper<IDXGIFactory2> renderer::_get_dxgi_factory() {
		_details::com_wrapper<IDXGIAdapter> adapter;
		_details::com_check(_dxgi_device->GetAdapter(adapter.get_ref()));
		_details::com_wrapper<IDXGIFactory2> factory;
		_details::com_check(adapter->GetParent(IID_PPV_ARGS(factory.get_ref())));
		return factory;
	}

	_details::com_wrapper<ID2D1Bitmap1> renderer::_create_bitmap_from_swap_chain(
		IDXGISwapChain1 *chain, vec2d scaling_factor
	) {
		_details::com_wrapper<IDXGISurface> surface;
		_details::com_wrapper<ID2D1Bitmap1> bitmap;
		_details::com_check(chain->GetBuffer(0, IID_PPV_ARGS(surface.get_ref())));
		_details::com_check(_d2d_device_context->CreateBitmapFromDxgiSurface(
			surface.get(),
			D2D1::BitmapProperties1(
				D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
				D2D1::PixelFormat(pixel_format, D2D1_ALPHA_MODE_PREMULTIPLIED),
				static_cast<FLOAT>(scaling_factor.x * USER_DEFAULT_SCREEN_DPI),
				static_cast<FLOAT>(scaling_factor.y * USER_DEFAULT_SCREEN_DPI)
			),
			bitmap.get_ref()
		));
		return bitmap;
	}

	void renderer::_new_window(ui::window &wnd) {
		window_impl &wnd_impl = _details::cast_window_impl(wnd.get_impl());
		std::any &data = _get_window_data(wnd);
		_window_data actual_data;

		// create swap chain
		DXGI_SWAP_CHAIN_DESC1 swapchain_desc;
		swapchain_desc.Width = 0;
		swapchain_desc.Height = 0;
		swapchain_desc.Format = pixel_format;
		swapchain_desc.Stereo = false;
		swapchain_desc.SampleDesc.Count = 1;
		swapchain_desc.SampleDesc.Quality = 0;
		swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchain_desc.BufferCount = 2;
		swapchain_desc.Scaling = DXGI_SCALING_NONE;
		swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		swapchain_desc.Flags = 0;
		_details::com_check(_get_dxgi_factory()->CreateSwapChainForHwnd(
			_d3d_device.get(), wnd_impl.get_native_handle(), &swapchain_desc,
			nullptr, nullptr, actual_data.swap_chain.get_ref()
		));
		// set data
		actual_data.target = _create_bitmap_from_swap_chain(
			actual_data.swap_chain.get(), wnd.get_scaling_factor()
		);
		data.emplace<_window_data>(actual_data);
		// resize buffer when the window size has changed
		wnd.size_changed += [this, pwnd = &wnd](ui::window::size_changed_info&) {
			auto &data = _get_window_data_as<_window_data>(*pwnd);
			data.target.reset(); // must release bitmap before resizing
			_details::com_check(data.swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0));
			// recreate bitmap
			data.target = _create_bitmap_from_swap_chain(data.swap_chain.get(), pwnd->get_scaling_factor());
		};
		// reallocate buffer when the window scaling has changed
		wnd.scaling_factor_changed += [this, pwnd = &wnd](ui::window::scaling_factor_changed_info &p) {
			auto &data = _get_window_data_as<_window_data>(*pwnd);
			data.target.reset(); // must release bitmap before resizing
			_details::com_check(data.swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0));
			// recreate bitmap
			data.target = _create_bitmap_from_swap_chain(data.swap_chain.get(), p.new_value);
		};
	}

	void renderer::_delete_window(ui::window &w) {
		_get_window_data(w).reset();
	}
}
