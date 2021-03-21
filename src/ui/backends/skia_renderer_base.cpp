// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/backends/skia_renderer_base.h"

/// \file
/// Implementation of the Skia renderer.

#include <skia/effects/SkGradientShader.h>
#include <skia/core/SkTextBlob.h>

namespace codepad::ui::skia {
	render_target_data renderer_base::create_render_target(vec2d size, vec2d scaling_factor, colord clear) {
		vec2d pixel_size(size.x * scaling_factor.x, size.y * scaling_factor.y);
		SkImageInfo image_info = SkImageInfo::MakeN32Premul(
			SkISize::Make(static_cast<int>(std::ceil(pixel_size.x)), static_cast<int>(std::ceil(pixel_size.y))),
			_color_space
		);
		auto target = std::make_shared<render_target>();
		target->_scale = scaling_factor;
		target->_surface = SkSurface::MakeRenderTarget(_skia_context.get(), SkBudgeted::kNo, image_info);

		auto target_bmp = std::make_shared<bitmap>();
		target_bmp->_image_or_surface.emplace<sk_sp<SkSurface>>(target->_surface);

		// TODO clear color

		render_target_data result;
		result.target = std::move(target);
		result.target_bitmap = std::move(target_bmp);
		return result;
	}

	void renderer_base::draw_plain_text(const ui::plain_text &generic_text, vec2d topleft, colord color) {
		auto &text = _details::cast_plain_text(generic_text);

		SkCanvas *canvas = _render_stack.top().canvas;

		if (!text._cached_text) {
			// gather glyphs
			unsigned int num_glyphs = 0;
			// FIXME check if the two num_glyphs are the same
			hb_glyph_position_t *glyph_positions = hb_buffer_get_glyph_positions(text._data.get_buffer(), &num_glyphs);
			hb_glyph_info_t *glyph_infos = hb_buffer_get_glyph_infos(text._data.get_buffer(), &num_glyphs);
			SkTextBlobBuilder builder;
			auto &run = builder.allocRunPos(text._font, num_glyphs);

			double x = 0.0, y_offset = text._data.get_ascender();
			for (unsigned int i = 0; i < num_glyphs; ++i) {
				run.glyphs[i] = glyph_infos[i].codepoint;
				run.pos[i * 2] = x + glyph_positions[i].x_offset / 64.0;
				run.pos[i * 2 + 1] = y_offset + glyph_positions[i].y_offset / 64.0;
				x += glyph_positions[i].x_advance / 64.0;
			}

			text._cached_text = builder.make();
		}

		SkPaint paint;
		paint.setColor4f(_details::cast_colorf(color), _color_space.get());
		canvas->drawTextBlob(text._cached_text, topleft.x, topleft.y, paint);
	}

	std::optional<SkPaint> renderer_base::_create_paint(const brushes::none&, const matd3x3&) {
		return std::nullopt;
	}

	std::optional<SkPaint> renderer_base::_create_paint(const brushes::solid_color &brush, const matd3x3&) {
		SkPaint paint;
		paint.setColor4f(_details::cast_colorf(brush.color), _color_space.get());
		return paint;
	}

	std::optional<SkPaint> renderer_base::_create_paint(const brushes::linear_gradient &brush, const matd3x3 &mat) {
		SkPaint paint;

		SkPoint endpoints[2]{ _details::cast_point(brush.from), _details::cast_point(brush.to) };
		std::vector<SkColor4f> colors(brush.gradients->size());
		std::vector<SkScalar> positions(brush.gradients->size());
		for (std::size_t i = 0; i < brush.gradients->size(); ++i) {
			const auto &stop = (*brush.gradients)[i];
			colors[i] = _details::cast_colorf(stop.color);
			positions[i] = static_cast<SkScalar>(stop.position);
		}
		SkMatrix skmat = _details::cast_matrix(mat);
		paint.setShader(SkGradientShader::MakeLinear(
			endpoints, colors.data(), _color_space, positions.data(), static_cast<int>(brush.gradients->size()),
			SkTileMode::kClamp, 0, &skmat
		));
		return paint;
	}

	std::optional<SkPaint> renderer_base::_create_paint(const brushes::radial_gradient &brush, const matd3x3 &mat) {
		SkPaint paint;

		std::vector<SkColor4f> colors(brush.gradients->size());
		std::vector<SkScalar> positions(brush.gradients->size());
		for (std::size_t i = 0; i < brush.gradients->size(); ++i) {
			const auto &stop = (*brush.gradients)[i];
			colors[i] = _details::cast_colorf(stop.color);
			positions[i] = static_cast<SkScalar>(stop.position);
		}
		SkMatrix skmat = _details::cast_matrix(mat);
		paint.setShader(SkGradientShader::MakeRadial(
			_details::cast_point(brush.center), static_cast<SkScalar>(brush.radius),
			colors.data(), _color_space, positions.data(), static_cast<int>(brush.gradients->size()),
			SkTileMode::kClamp, 0, &skmat
		));
		return paint;
	}

	std::optional<SkPaint> renderer_base::_create_paint(const brushes::bitmap_pattern &brush, const matd3x3 &mat) {
		SkPaint paint;

		if (brush.image == nullptr) {
			return std::nullopt;
		}
		const auto &bmp = _details::cast_bitmap(*brush.image);
		SkImage *img = nullptr;
		sk_sp<SkImage> img_handle;
		if (std::holds_alternative<sk_sp<SkImage>>(bmp._image_or_surface)) {
			img = std::get<sk_sp<SkImage>>(bmp._image_or_surface).get();
		} else {
			img_handle = std::get<sk_sp<SkSurface>>(bmp._image_or_surface)->makeImageSnapshot();
			img = img_handle.get();
		}
		if (img == nullptr) {
			return std::nullopt;
		}
		paint.setShader(img->makeShader(_details::cast_matrix(mat)));
		return paint;
	}

	std::optional<SkPaint> renderer_base::_create_paint(const generic_brush &brush) {
		auto result = std::visit([this, &brush](const auto &b) {
			return _create_paint(b, brush.transform);
		}, brush.value);
		if (result) {
			result->setAntiAlias(true);
		}
		return result;
	}

	std::optional<SkPaint> renderer_base::_create_paint(const generic_pen &pen) {
		std::optional<SkPaint> paint = _create_paint(pen.brush);
		if (paint) {
			paint->setStyle(SkPaint::Style::kStroke_Style);
			paint->setStrokeWidth(static_cast<SkScalar>(pen.thickness));
		}
		return paint;
	}
}
