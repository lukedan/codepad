// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/cairo_renderer_base.h"

/// \file
/// Implementation of the cairo renderer.

#ifdef CP_USE_CAIRO

#	include <cairo/cairo-ft.h>

#	include <harfbuzz/hb-ft.h>

namespace codepad::ui::cairo {
	namespace _details {
		/// Converts a \ref matd3x3 to a \p cairo_matrix_t.
		cairo_matrix_t cast_matrix(matd3x3 m) {
			cairo_matrix_t result;
			result.xx = m[0][0];
			result.xy = m[0][1];
			result.x0 = m[0][2];
			result.yx = m[1][0];
			result.yy = m[1][1];
			result.y0 = m[1][2];
			return result;
		}

		/// Converts a component of a color to a \p guint16.
		guint16 cast_color_component(double c) {
			return static_cast<guint16>(std::round(c * std::numeric_limits<guint16>::max()));
		}

		/// Converts a \ref horizontal_text_alignment to a \p PangoAlignment.
		PangoAlignment cast_horizontal_alignment(horizontal_text_alignment align) {
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
		PangoStyle cast_font_style(font_style style) {
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
		int cast_font_style_fontconfig(font_style style) {
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
		PangoWeight cast_font_weight(font_weight weight) {
			// TODO
			return PANGO_WEIGHT_NORMAL;
		}

		/// Converts a \ref font_weight to a Fontconfig weight.
		int cast_font_weight_fontconfig(font_weight weight) {
			// TODO
			return FC_WEIGHT_NORMAL;
		}

		/// Converts a \ref font_stretch to a \p PangoStretch.
		PangoStretch cast_font_stretch(font_stretch stretch) {
			// TODO
			return PANGO_STRETCH_NORMAL;
		}

		/// Converts a \ref font_stretch to a Fontconfig width.
		int cast_font_stretch_fontconfig(font_stretch stretch) {
			// TODO
			return FC_WIDTH_NORMAL;
		}

		/// Casts a \ref ui::bitmap to a \ref bitmap.
		bitmap &cast_bitmap(ui::bitmap &b) {
			auto *bmp = dynamic_cast<bitmap*>(&b);
			assert_true_usage(bmp, "invalid bitmap type");
			return *bmp;
		}

		/// Casts a \ref ui::render_target to a \ref render_target.
		render_target &cast_render_target(ui::render_target &r) {
			auto *rt = dynamic_cast<render_target*>(&r);
			assert_true_usage(rt, "invalid render target type");
			return *rt;
		}

		/// Casts a \ref ui::formatted_text to a \ref formatted_text.
		const formatted_text &cast_formatted_text(const ui::formatted_text &t) {
			auto *rt = dynamic_cast<const formatted_text*>(&t);
			assert_true_usage(rt, "invalid formatted text type");
			return *rt;
		}

		/// Casts a \ref ui::font to a \ref font.
		font &cast_font(ui::font &t) {
			auto *rt = dynamic_cast<font*>(&t);
			assert_true_usage(rt, "invalid formatted text type");
			return *rt;
		}

		/// Casts a \ref ui::plain_text to a \ref plain_text.
		const plain_text &cast_plain_text(const ui::plain_text &t) {
			auto *rt = dynamic_cast<const plain_text*>(&t);
			assert_true_usage(rt, "invalid formatted text type");
			return *rt;
		}

		/// Casts a \p PangoRectangle **in pango units** to a \ref rectd.
		rectd cast_rect_back(const PangoRectangle &rect) {
			return rectd::from_xywh(
				pango_units_to_double(rect.x),
				pango_units_to_double(rect.y),
				pango_units_to_double(rect.width),
				pango_units_to_double(rect.height)
			);
		}
	}


	rectd formatted_text::get_layout() const {
		PangoRectangle layout;
		pango_layout_get_extents(_layout.get(), nullptr, &layout);
		return _details::cast_rect_back(layout);
	}

	std::vector<line_metrics> formatted_text::get_line_metrics() const {
		PangoLayoutIter *iter = pango_layout_get_iter(_layout.get());
		std::vector<line_metrics> result;
		do {
			int ymin_pu = 0, ymax_pu = 0;
			pango_layout_iter_get_line_yrange(iter, &ymin_pu, &ymax_pu);
			double height = pango_units_to_double(ymax_pu - ymin_pu);
			int baseline = pango_layout_iter_get_baseline(iter);
			double baseline1 = pango_units_to_double(baseline - ymin_pu);
		} while (pango_layout_iter_next_line(iter));
		pango_layout_iter_free(iter);
		return result;
	}

	caret_hit_test_result formatted_text::hit_test(vec2d pos) const {
		int index = 0, trailing = 0;
		PangoRectangle rect;
		pango_layout_xy_to_index(
			_layout.get(),
			pango_units_from_double(pos.x), pango_units_from_double(pos.y),
			&index, &trailing
		);
		pango_layout_index_to_pos(_layout.get(), index, &rect);

		caret_hit_test_result result;
		result.character = std::lower_bound(_bytepos.begin(), _bytepos.end(), index) - _bytepos.begin();
		result.character_layout = _details::cast_rect_back(rect);
		result.rear = (trailing != 0);
		return result;
	}

	rectd formatted_text::get_character_placement(std::size_t pos) const {
		PangoRectangle rect;
		pango_layout_index_to_pos(_layout.get(), _bytepos[std::min(pos, _bytepos.size() - 1)], &rect);
		if (rect.width < 0) {
			rect.x += rect.width;
			rect.width = -rect.width;
		}
		return _details::cast_rect_back(rect);
	}

	std::vector<rectd> formatted_text::get_character_range_placement(std::size_t beg, std::size_t len) const {
		PangoLayoutIter *iter = pango_layout_get_iter(_layout.get());
		int beg_index = static_cast<int>(_bytepos[beg]), end_index = static_cast<int>(_bytepos[beg + len]);
		std::vector<rectd> result;
		do {
			PangoLayoutLine *line = pango_layout_iter_get_line_readonly(iter);
			int *ranges = nullptr;
			int num_ranges = 0;
			pango_layout_line_get_x_ranges(line, beg_index, end_index, &ranges, &num_ranges);
			if (ranges) {
				int ymin_pu = 0, ymax_pu = 0;
				pango_layout_iter_get_line_yrange(iter, &ymin_pu, &ymax_pu);
				double ymin = pango_units_to_double(ymin_pu), ymax = pango_units_to_double(ymax_pu);

				int *iter = ranges;
				for (int i = 0; i < num_ranges; ++i, iter += 2) {
					int beg = iter[0], end = iter[1];
					result.emplace_back(pango_units_to_double(beg), pango_units_to_double(end), ymin, ymax);
				}
				g_free(ranges);
			}
		} while (pango_layout_iter_next_line(iter));
		pango_layout_iter_free(iter);
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

	std::pair<guint, guint> formatted_text::_char_to_byte(std::size_t beg, std::size_t len) const {
		beg = std::min(beg, _bytepos.size() - 1);
		std::size_t end = std::min(beg + len, _bytepos.size() - 1);
		return { static_cast<guint>(_bytepos[beg]), static_cast<guint>(_bytepos[end]) };
	}


	std::unique_ptr<ui::font> font_family::get_matching_font(
		font_style style, font_weight weight, font_stretch stretch
	) const {
		auto patt = _details::make_gtk_object_ref_give(FcPatternDuplicate(_pattern.get()));

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
		auto res_patt = _details::make_gtk_object_ref_give(FcFontMatch(nullptr, patt.get(), &result));
		assert_true_sys(result != FcResultOutOfMemory, "Fontconfig out of memory");

		FcChar8 *file_name = nullptr;
		int font_index = 0;
		assert_true_sys(
			FcPatternGetString(res_patt.get(), FC_FILE, 0, &file_name) == FcResultMatch,
			"failed to obtain font file name from Fontconfig"
		);
		FcPatternGetInteger(res_patt.get(), FC_INDEX, 0, &font_index);

		FT_Face face;
		_details::ft_check(FT_New_Face(
			_renderer._freetype, reinterpret_cast<const char*>(file_name), font_index, &face
		));

		return std::unique_ptr<font>(new font(_details::make_freetype_face_ref_give(face)));
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


	void path_geometry_builder::add_arc(
		vec2d to, vec2d radius, double rotation, ui::sweep_direction dir, ui::arc_type type
	) {
		// if any radius component is zero, it will cause divide by zeroes in later calculations
		if (approximately_equals(radius.x, 0.0) || approximately_equals(radius.y, 0.0)) {
			// TODO this is a very inaccurate approximation
			cairo_line_to(_context, to.x, to.y);
			return;
		}

		cairo_matrix_t old_matrix;
		cairo_get_matrix(_context, &old_matrix);

		matd3x3
			scale = matd3x3::scale(vec2d(), radius),
			rotate = matd3x3::rotate_clockwise(vec2d(), rotation);
		matd3x3 inverse_transform =
			matd3x3::scale(vec2d(), vec2d(1.0 / radius.x, 1.0 / radius.y)) * rotate.transpose();
		cairo_matrix_t trans = _details::cast_matrix(rotate * scale), full_trans;
		cairo_matrix_multiply(&full_trans, &trans, &old_matrix);
		cairo_set_matrix(_context, &full_trans);

		vec2d current_point;
		cairo_get_current_point(_context, &current_point.x, &current_point.y);

		vec2d trans_to = inverse_transform.transform_position(to);

		// find the center
		vec2d offset = trans_to - current_point;
		vec2d center = current_point + 0.5 * offset;
		// x is right and y is down, so we're rotating the vector like this
		if ((dir == ui::sweep_direction::clockwise) == (type == ui::arc_type::major)) {
			offset = vec2d(offset.y, -offset.x);
		} else {
			offset = vec2d(-offset.y, offset.x);
		}
		double sqrlen = offset.length_sqr();
		offset *= std::sqrt((1.0 - 0.25 * sqrlen) / sqrlen);
		center += offset;

		vec2d off1 = current_point - center, off2 = trans_to - center;
		double angle1 = std::atan2(off1.y, off1.x), angle2 = std::atan2(off2.y, off2.x);
		if (dir == ui::sweep_direction::clockwise) {
			cairo_arc(_context, center.x, center.y, 1.0, angle1, angle2);
		} else {
			cairo_arc_negative(_context, center.x, center.y, 1.0, angle1, angle2);
		}

		cairo_set_matrix(_context, &old_matrix); // restore transformation
	}


	void renderer_base::_render_target_stackframe::update_transform() {
		cairo_matrix_t mat = _details::cast_matrix(matrices.top());
		cairo_set_matrix(context, &mat);
	}


	render_target_data renderer_base::create_render_target(vec2d size, vec2d scaling_factor, colord clear) {
		auto resrt = std::make_unique<render_target>();
		auto resbmp = std::make_unique<bitmap>();

		// create surface
		resbmp->_size = size;
		resbmp->_surface = _details::make_gtk_object_ref_give(
			cairo_image_surface_create(
				CAIRO_FORMAT_ARGB32,
				static_cast<int>(std::ceil(size.x * scaling_factor.x)),
				static_cast<int>(std::ceil(size.y * scaling_factor.y))
			));
		assert_true_sys(
			cairo_surface_status(resbmp->_surface.get()) == CAIRO_STATUS_SUCCESS,
			"failed to create cairo surface"
		);
		// set dpi scaling
		cairo_surface_set_device_scale(resbmp->_surface.get(), scaling_factor.x, scaling_factor.y);

		// create context
		resrt->_context = _details::make_gtk_object_ref_give(cairo_create(resbmp->_surface.get()));
		assert_true_sys(
			cairo_status(resrt->_context.get()) == CAIRO_STATUS_SUCCESS,
			"failed to create cairo context"
		);
		// clear it
		cairo_set_source_rgba(resrt->_context.get(), clear.r, clear.g, clear.b, clear.a);
		cairo_paint(resrt->_context.get());

		return render_target_data(std::move(resrt), std::move(resbmp));
	}

	std::unique_ptr<ui::font_family> renderer_base::find_font_family(const std::u8string &family) {
		auto pattern = _details::make_gtk_object_ref_give(FcPatternCreate());
		FcPatternAddString(pattern.get(), FC_FAMILY, reinterpret_cast<const FcChar8*>(family.c_str()));
		return std::unique_ptr<font_family>(new font_family(*this, std::move(pattern)));
	}

	void renderer_base::begin_drawing(ui::window_base &w) {
		auto &data = _window_data::get(w);
		_render_stack.emplace(data.context.get(), &w);
	}

	void renderer_base::begin_drawing(ui::render_target &generic_rt) {
		auto &rt = _details::cast_render_target(generic_rt);
		_render_stack.emplace(rt._context.get());
	}

	void renderer_base::end_drawing() {
		assert_true_usage(!_render_stack.empty(), "begin_drawing/end_drawing calls mismatch");
		assert_true_usage(_render_stack.top().matrices.size() == 1, "push_matrix/pop_matrix calls mismatch");
		_finish_drawing_to_target();
		_render_stack.pop();
	}

	void renderer_base::clear(colord color) {
		cairo_t *context = _render_stack.top().context;
		cairo_save(context);
		{
			// reset state
			cairo_reset_clip(context);
			cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
			// clear
			cairo_set_source_rgba(context, color.r, color.g, color.b, color.a);
			cairo_paint(context);
		}
		cairo_restore(context);
	}

	void renderer_base::draw_formatted_text(const ui::formatted_text &ft, vec2d pos) {
		auto text = _details::cast_formatted_text(ft);
		cairo_t *context = _render_stack.top().context;

		pango_cairo_update_context(context, _pango_context.get());
		pango_layout_context_changed(text._layout.get());
		cairo_move_to(context, pos.x, pos.y);
		pango_cairo_show_layout(context, text._layout.get());
		cairo_new_path(context);
	}

	std::unique_ptr<ui::plain_text> renderer_base::create_plain_text(
		std::u8string_view text, ui::font &generic_fnt, double font_size
	) {
		auto buf = _details::make_gtk_object_ref_give(hb_buffer_create());
		codepoint replacement = hb_buffer_get_replacement_codepoint(buf.get());
		std::size_t index = 0;
		for (auto it = text.begin(); it != text.end(); ) {
			codepoint cp;
			if (!encodings::utf8::next_codepoint(it, text.end(), cp)) {
				cp = replacement;
			}
			hb_buffer_add(buf.get(), cp, static_cast<unsigned int>(index));
		}
		return _create_plain_text_impl(std::move(buf), generic_fnt, font_size);
	}

	std::unique_ptr<ui::plain_text> renderer_base::create_plain_text(
		std::basic_string_view<codepoint> text, ui::font &generic_fnt, double font_size
	) {
		auto buf = _details::make_gtk_object_ref_give(hb_buffer_create());
		for (std::size_t i = 0; i < text.size(); ++i) {
			hb_buffer_add(buf.get(), text[i], static_cast<unsigned int>(i));
		}
		return _create_plain_text_impl(std::move(buf), generic_fnt, font_size);
	}

	void renderer_base::draw_plain_text(const ui::plain_text &generic_text, vec2d pos, colord c) {
		const plain_text &text = _details::cast_plain_text(generic_text);

		cairo_t *context = _render_stack.top().context;

		// TODO this prevents the text from flickering, but is this the correct way to do it?
		double sx, sy;
		cairo_surface_get_device_scale(cairo_get_target(context), &sx, &sy);
		_details::ft_check(FT_Set_Char_Size(
			text._font.get(), 0, static_cast<FT_F26Dot6>(std::round(64.0 * text._font_size)), 96 * sx, 96 * sy
		));

		auto cairo_fnt = _details::make_gtk_object_ref_give(
			cairo_ft_font_face_create_for_ft_face(text._font.get(), 0)
		);
		cairo_set_font_face(context, cairo_fnt.get());
		cairo_set_font_size(context, text._font_size * (96.0 / 72.0));

		// gather glyphs
		unsigned int num_glyphs = 0;
		// FIXME check if the two num_glyphs are the same
		hb_glyph_position_t *glyph_positions = hb_buffer_get_glyph_positions(text._buffer.get(), &num_glyphs);
		hb_glyph_info_t *glyph_infos = hb_buffer_get_glyph_infos(text._buffer.get(), &num_glyphs);
		std::vector<cairo_glyph_t> glyphs;
		glyphs.reserve(static_cast<std::size_t>(num_glyphs));
		vec2d pen_pos = pos;
		pen_pos.y += text._ascender;
		for (unsigned int i = 0; i < num_glyphs; ++i) {
			cairo_glyph_t &glyph = glyphs.emplace_back();
			glyph.index = glyph_infos[i].codepoint;
			glyph.x = pen_pos.x + glyph_positions[i].x_offset / 64.0;
			glyph.y = pen_pos.y + glyph_positions[i].y_offset / 64.0;
			pen_pos.x += glyph_positions[i].x_advance / 64.0;
		}

		cairo_set_source_rgba(context, c.r, c.g, c.b, c.a);
		// TODO the text flickers, seems that wrong font sizes are used for certain glyphs
		cairo_show_glyphs(context, glyphs.data(), static_cast<int>(glyphs.size()));
	}

	void renderer_base::_draw_path(
		cairo_t *context,
		const ui::generic_brush_parameters &brush,
		const ui::generic_pen_parameters &pen
	) {
		if (_details::gtk_object_ref<cairo_pattern_t> brush_patt = _create_pattern(brush)) {
			cairo_set_source(context, brush_patt.get());
			cairo_fill_preserve(context);
		}
		if (_details::gtk_object_ref<cairo_pattern_t> pen_patt = _create_pattern(pen.brush)) {
			cairo_set_source(context, pen_patt.get());
			cairo_set_line_width(context, pen.thickness);
			cairo_stroke_preserve(context);
		}
		cairo_new_path(context); // clear current path
		// release the source pattern
		cairo_set_source_rgb(context, 1.0, 0.4, 0.7);
	}

	_details::gtk_object_ref<cairo_pattern_t> renderer_base::_create_pattern(
		const ui::brush_parameters::solid_color &brush
	) {
		return _details::make_gtk_object_ref_give(
			cairo_pattern_create_rgba(
				brush.color.r, brush.color.g, brush.color.b, brush.color.a
			));
	}

	void renderer_base::_add_gradient_stops(cairo_pattern_t *patt, const gradient_stop_collection &gradients) {
		for (const gradient_stop &stop : gradients) {
			cairo_pattern_add_color_stop_rgba(
				patt, stop.position, stop.color.r, stop.color.g, stop.color.b, stop.color.a
			);
		}
	}

	_details::gtk_object_ref<cairo_pattern_t> renderer_base::_create_pattern(
		const ui::brush_parameters::linear_gradient &brush
	) {
		if (brush.gradients) {
			auto patt = _details::make_gtk_object_ref_give(
				cairo_pattern_create_linear(
					brush.from.x, brush.from.y, brush.to.x, brush.to.y
				));
			_add_gradient_stops(patt.get(), *brush.gradients);
			return patt;
		}
		return _details::gtk_object_ref<cairo_pattern_t>();
	}

	_details::gtk_object_ref<cairo_pattern_t> renderer_base::_create_pattern(
		const ui::brush_parameters::radial_gradient &brush
	) {
		if (brush.gradients) {
			auto patt = _details::make_gtk_object_ref_give(
				cairo_pattern_create_radial(
					brush.center.x, brush.center.y, 0.0, brush.center.x, brush.center.y, brush.radius
				));
			_add_gradient_stops(patt.get(), *brush.gradients);
			return patt;
		}
		return _details::gtk_object_ref<cairo_pattern_t>();
	}

	_details::gtk_object_ref<cairo_pattern_t> renderer_base::_create_pattern(
		const ui::brush_parameters::bitmap_pattern &brush
	) {
		if (brush.image) {
			return _details::make_gtk_object_ref_give(
				cairo_pattern_create_for_surface(
					_details::cast_bitmap(*brush.image)._surface.get()
				));
		}
		return _details::gtk_object_ref<cairo_pattern_t>();
	}

	_details::gtk_object_ref<cairo_pattern_t> renderer_base::_create_pattern(
		const ui::brush_parameters::none&
	) {
		return _details::gtk_object_ref<cairo_pattern_t>();
	}

	_details::gtk_object_ref<cairo_pattern_t> renderer_base::_create_pattern(
		const generic_brush_parameters &b
	) {
		auto pattern = std::visit([](auto &&brush) {
			return _create_pattern(brush);
			}, b.value);
		if (pattern) {
			cairo_matrix_t mat = _details::cast_matrix(b.transform.inverse());
			cairo_pattern_set_matrix(pattern.get(), &mat);
		}
		return pattern;
	}

	void renderer_base::_make_ellipse_geometry(vec2d center, double rx, double ry) {
		_render_target_stackframe &stackframe = _render_stack.top();
		cairo_t *context = stackframe.context;

		cairo_matrix_t mat = _details::cast_matrix(
			stackframe.matrices.top() * matd3x3::scale(center, vec2d(rx, ry))
		); // apply this scale transform before all else (in local space)
		cairo_set_matrix(context, &mat);
		cairo_arc(context, center.x, center.y, 1.0, 0.0, 6.2831853); // 2 pi, but slightly less
		cairo_close_path(context);

		stackframe.update_transform(); // restore transform
	}

	void renderer_base::_make_rounded_rectangle_geometry(rectd rgn, double rx, double ry) {
		_render_target_stackframe &stackframe = _render_stack.top();
		cairo_t *context = stackframe.context;

		if (rx < 1e-4 || ry < 1e-4) { // radius too small or negative, append rectangle directly
			cairo_rectangle(context, rgn.xmin, rgn.ymin, rgn.width(), rgn.height());
			return;
		}

		cairo_matrix_t mat = _details::cast_matrix(
			stackframe.matrices.top() * matd3x3::scale(vec2d(), vec2d(rx, ry))
		); // apply scale transform locally
		// adjust region
		rgn.xmin = rgn.xmin / rx + 1.0;
		rgn.xmax = rgn.xmax / rx - 1.0;
		rgn.ymin = rgn.ymin / ry + 1.0;
		rgn.ymax = rgn.ymax / ry - 1.0;
		rgn.make_valid_average();

		cairo_set_matrix(context, &mat);
		cairo_arc(context, rgn.xmin, rgn.ymin, 1.0, -1.57079632, 0.0);
		cairo_arc(context, rgn.xmax, rgn.ymin, 1.0, 0.0, 1.57079632);
		cairo_arc(context, rgn.xmax, rgn.ymax, 1.0, 1.57079632, 3.14159265);
		cairo_arc(context, rgn.xmin, rgn.ymax, 1.0, 3.14159265, 4.71238898);
		cairo_close_path(context);

		stackframe.update_transform();
	}

	std::unique_ptr<formatted_text> renderer_base::_create_formatted_text_impl(
		std::u8string_view text, const font_parameters &font, colord c, vec2d size, wrapping_mode wrap,
		horizontal_text_alignment halign, vertical_text_alignment
	) {
		auto result = std::make_unique<formatted_text>();
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
		/*pango_layout_set_height(result->_layout.get(), pango_units_from_double(size.y));*/

		{ // set color
			auto attr_list = _details::make_gtk_object_ref_give(pango_attr_list_new());
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

		// compute _bytepos
		result->_bytepos.emplace_back(0); // first character
		for (auto it = text.begin(); it != text.end(); ) {
			encodings::utf8::next_codepoint(it, text.end());
			result->_bytepos.emplace_back(it - text.begin());
		}

		return result;
	}

	std::unique_ptr<ui::plain_text> renderer_base::_create_plain_text_impl(
		_details::gtk_object_ref<hb_buffer_t> buf, ui::font &generic_fnt, double font_size
	) {
		auto fnt = _details::cast_font(generic_fnt);

		unsigned int num_chars = hb_buffer_get_length(buf.get());
		hb_buffer_set_content_type(buf.get(), HB_BUFFER_CONTENT_TYPE_UNICODE);

		// FIXME see if these settings are good enough for code
		hb_buffer_set_direction(buf.get(), HB_DIRECTION_LTR);
		hb_buffer_set_script(buf.get(), HB_SCRIPT_LATIN);
		hb_buffer_set_language(buf.get(), hb_language_from_string("en", -1));

		_details::ft_check(FT_Set_Char_Size(
			fnt._face.get(), 0, static_cast<FT_F26Dot6>(std::round(64.0 * font_size)), 96, 96
		));
		hb_font_t *hb_font = hb_ft_font_create(fnt._face.get(), nullptr);

		hb_shape(hb_font, buf.get(), nullptr, 0); // TODO features?

		hb_font_destroy(hb_font);
		return std::unique_ptr<plain_text>(new plain_text(
			std::move(buf), fnt, fnt._face->size->metrics, num_chars, font_size
		));
	}

	void renderer_base::_new_window(window_base &wnd) {
		std::any &data = _get_window_data(wnd);
		_window_data actual_data;

		// set data
		actual_data.context = _create_context_for_window(wnd, wnd.get_scaling_factor());
		data.emplace<_window_data>(actual_data);
		// resize buffer when the window size has changed
		wnd.size_changed += [this, pwnd = &wnd](ui::window_base::size_changed_info&) {
			auto &data = _window_data::get(*pwnd);
			data.context.reset();
			data.context = _create_context_for_window(*pwnd, pwnd->get_scaling_factor());
			pwnd->invalidate_visual();
		};
		// reallocate buffer when the window scaling has changed
		wnd.scaling_factor_changed += [this, pwnd = &wnd](window_base::scaling_factor_changed_info &p) {
			auto &data = _window_data::get(*pwnd);
			data.context.reset();
			data.context = _create_context_for_window(*pwnd, p.new_value);
			pwnd->invalidate_visual();
		};
	}
}

#endif
