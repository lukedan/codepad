// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/backends/cairo_renderer_base.h"

/// \file
/// Implementation of the cairo renderer.

#include <cairo/cairo-ft.h>

namespace codepad::ui::cairo {
	namespace _details {
		using namespace ui::_details;

		/// Converts a \ref matd3x3 to a \p cairo_matrix_t.
		[[nodiscard]] cairo_matrix_t cast_matrix(matd3x3 m) {
			cairo_matrix_t result;
			result.xx = m[0][0];
			result.xy = m[0][1];
			result.x0 = m[0][2];
			result.yx = m[1][0];
			result.yy = m[1][1];
			result.y0 = m[1][2];
			return result;
		}


		/// Casts a \ref ui::bitmap to a \ref bitmap.
		[[nodiscard]] bitmap &cast_bitmap(ui::bitmap &b) {
			auto *bmp = dynamic_cast<bitmap*>(&b);
			assert_true_usage(bmp, "invalid bitmap type");
			return *bmp;
		}

		/// Casts a \ref ui::render_target to a \ref render_target.
		[[nodiscard]] render_target &cast_render_target(ui::render_target &r) {
			auto *rt = dynamic_cast<render_target*>(&r);
			assert_true_usage(rt, "invalid render target type");
			return *rt;
		}
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
		auto resrt = std::make_shared<render_target>();
		auto resbmp = std::make_shared<bitmap>();

		// create surface
		resbmp->_size = size;
		resbmp->_surface = _create_offscreen_surface(
			static_cast<int>(std::ceil(size.x * scaling_factor.x)),
			static_cast<int>(std::ceil(size.y * scaling_factor.y)),
			scaling_factor
		);

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
		auto &text = pango_harfbuzz::_details::cast_formatted_text(ft);
		cairo_t *context = _render_stack.top().context;

		pos += text.get_alignment_offset();

		pango_cairo_update_context(context, _text_context.get_pango_context());
		pango_layout_context_changed(text.get_pango_layout());
		cairo_move_to(context, pos.x, pos.y);
		pango_cairo_show_layout(context, text.get_pango_layout());
		cairo_new_path(context);
	}

	void renderer_base::draw_plain_text(const ui::plain_text &generic_text, vec2d pos, colord c) {
		auto &text = pango_harfbuzz::_details::cast_plain_text(generic_text);

		cairo_t *context = _render_stack.top().context;

		// TODO this prevents the text from flickering, but is this the correct way to do it?
		double sx, sy;
		cairo_surface_get_device_scale(cairo_get_target(context), &sx, &sy);
		_details::ft_check(FT_Set_Char_Size(
			text.get_font(), 0, static_cast<FT_F26Dot6>(std::round(64.0 * text.get_font_size())), 96 * sx, 96 * sy
		));

		auto cairo_fnt = _details::make_gtk_object_ref_give(
			cairo_ft_font_face_create_for_ft_face(text.get_font(), 0)
		);
		cairo_set_font_face(context, cairo_fnt.get());
		cairo_set_font_size(context, text.get_font_size() * (96.0 / 72.0));

		// gather glyphs
		unsigned int num_glyphs = 0;
		// FIXME check if the two num_glyphs are the same
		hb_glyph_position_t *glyph_positions = hb_buffer_get_glyph_positions(text.get_buffer(), &num_glyphs);
		hb_glyph_info_t *glyph_infos = hb_buffer_get_glyph_infos(text.get_buffer(), &num_glyphs);
		std::vector<cairo_glyph_t> glyphs;
		glyphs.reserve(static_cast<std::size_t>(num_glyphs));
		vec2d pen_pos = pos;
		pen_pos.y += text.get_ascender();
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
		const ui::generic_brush &brush,
		const ui::generic_pen &pen
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
		const brushes::solid_color &brush
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
		const brushes::linear_gradient &brush
	) {
		if (brush.gradients) {
			auto patt = _details::make_gtk_object_ref_give(cairo_pattern_create_linear(
				brush.from.x, brush.from.y, brush.to.x, brush.to.y
			));
			_add_gradient_stops(patt.get(), *brush.gradients);
			return patt;
		}
		return _details::gtk_object_ref<cairo_pattern_t>();
	}

	_details::gtk_object_ref<cairo_pattern_t> renderer_base::_create_pattern(
		const brushes::radial_gradient &brush
	) {
		if (brush.gradients) {
			auto patt = _details::make_gtk_object_ref_give(cairo_pattern_create_radial(
				brush.center.x, brush.center.y, 0.0, brush.center.x, brush.center.y, brush.radius
			));
			_add_gradient_stops(patt.get(), *brush.gradients);
			return patt;
		}
		return _details::gtk_object_ref<cairo_pattern_t>();
	}

	_details::gtk_object_ref<cairo_pattern_t> renderer_base::_create_pattern(
		const brushes::bitmap_pattern &brush
	) {
		if (brush.image) {
			return _details::make_gtk_object_ref_give(cairo_pattern_create_for_surface(
				_details::cast_bitmap(*brush.image)._surface.get()
			));
		}
		return _details::gtk_object_ref<cairo_pattern_t>();
	}

	_details::gtk_object_ref<cairo_pattern_t> renderer_base::_create_pattern(
		const brushes::none&
	) {
		return _details::gtk_object_ref<cairo_pattern_t>();
	}

	_details::gtk_object_ref<cairo_pattern_t> renderer_base::_create_pattern(
		const generic_brush &b
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

	_details::gtk_object_ref<cairo_surface_t> renderer_base::_create_similar_surface(
		window_base &wnd, int width, int height
	) {
		return _details::make_gtk_object_ref_give(cairo_surface_create_similar(
			_get_window_data_as<_window_data>(wnd).get_surface(), CAIRO_CONTENT_COLOR_ALPHA, width, height
		));
	}

	_details::gtk_object_ref<cairo_surface_t> renderer_base::_create_offscreen_surface(
		int width, int height, vec2d scale
	) {
		_details::gtk_object_ref<cairo_surface_t> result;
		if (_random_window) {
			result = _create_similar_surface(*_random_window, width, height);
		} else {
			logger::get().log_warning(CP_HERE) <<
				"no window has been created before creating this offscreen surface: " <<
				"an image surface has been created which could lead to poor performance" <<
				logger::stacktrace;
			result = _details::make_gtk_object_ref_give(cairo_image_surface_create(
				CAIRO_FORMAT_ARGB32, width, height
			));
		}
		assert_true_sys(
			cairo_surface_status(result.get()) == CAIRO_STATUS_SUCCESS,
			"failed to create cairo surface"
		);
		// set dpi scaling
		cairo_surface_set_device_scale(result.get(), scale.x, scale.y);
		return result;
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
		rgn = rgn.made_positive_average();

		cairo_set_matrix(context, &mat);
		cairo_arc(context, rgn.xmin, rgn.ymin, 1.0, -1.57079632, 0.0);
		cairo_arc(context, rgn.xmax, rgn.ymin, 1.0, 0.0, 1.57079632);
		cairo_arc(context, rgn.xmax, rgn.ymax, 1.0, 1.57079632, 3.14159265);
		cairo_arc(context, rgn.xmin, rgn.ymax, 1.0, 3.14159265, 4.71238898);
		cairo_close_path(context);

		stackframe.update_transform();
	}
}
