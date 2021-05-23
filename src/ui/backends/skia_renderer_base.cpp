// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/backends/skia_renderer_base.h"

/// \file
/// Implementation of the Skia renderer.

#ifdef CP_PLATFORM_WINDOWS
#	include <Windows.h>
#endif
#include <GL/gl.h>

#include <skia/effects/SkGradientShader.h>
#include <skia/core/SkTextBlob.h>

namespace codepad::ui::skia {
	std::shared_ptr<font> font_family::_get_matching_font_impl(
		pango_harfbuzz::text_engine &engine, pango_harfbuzz::font_family_data family,
		font_style style, font_weight weight, font_stretch stretch
	) {
		auto &entry = family.get_cache_entry();
		auto [it, inserted] = entry.font_faces.try_emplace(pango_harfbuzz::font_params(style, weight, stretch));
		if (inserted) {
			auto found = entry.find_font(style, weight, stretch);
			auto result = std::make_shared<font>(
				engine, reinterpret_cast<const char*>(found.get_font_file_path()), found.get_font_index()
			);
			it->second = result;
			return result;
		}
		return std::dynamic_pointer_cast<font>(it->second);
	}


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
		target_bmp->_scaling = scaling_factor;

		target->_surface->getCanvas()->clear(_details::cast_color(clear));

		render_target_data result;
		result.target = std::move(target);
		result.target_bitmap = std::move(target_bmp);
		return result;
	}

	void renderer_base::draw_formatted_text(const ui::formatted_text &generic_text, vec2d pos) {
		auto &text = _details::cast_formatted_text(generic_text);

		if (!text._cached_text) {
			SkTextBlobBuilder builder;

			// iterate over all runs
			PangoLayoutIter *iter = pango_layout_get_iter(text._data.get_pango_layout());
			do {
				PangoLayoutRun *run = pango_layout_iter_get_run_readonly(iter);
				if (!run) { // continue to the next line
					continue;
				}

				// find the font used for this run
				// TODO font fallback doesn't work - is it handled by the renderer?
				SkFont font;
				{
					PangoFontDescription *font_description = pango_font_describe(run->item->analysis.font);

					auto family = _text_engine.find_font_family(
						reinterpret_cast<const char8_t*>(pango_font_description_get_family(font_description))
					);
					auto typeface = font_family::_get_matching_font_impl(
						_text_engine, family,
						pango_harfbuzz::_details::cast_font_style_back(
							pango_font_description_get_style(font_description)
						),
						pango_harfbuzz::_details::cast_font_weight_back(
							pango_font_description_get_weight(font_description)
						),
						pango_harfbuzz::_details::cast_font_stretch_back(
							pango_font_description_get_stretch(font_description)
						)
					)->_skia_font;
					// TODO pango_font_describe returns the font size in points. skia expects font size in points. yet
					//      the font sizes don't match
					auto font_size_pt = pango_units_to_double(pango_font_description_get_size(font_description)) * 1.33;
					font = SkFont(typeface, static_cast<SkScalar>(font_size_pt));
					font.setSubpixel(true);

					pango_font_description_free(font_description);
				}

				// retrieve text color
				for (GSList *attr_node = run->item->analysis.extra_attrs; attr_node; attr_node = attr_node->next) {
					auto *attrib = static_cast<PangoAttribute*>(attr_node->data);
					if (attrib->klass->type == PANGO_ATTR_FOREGROUND) {
						auto *color_attrib = reinterpret_cast<PangoAttrColor*>(attrib);
						color_attrib->color; // TODO do something with this
					} else if (attrib->klass->type == PANGO_ATTR_FOREGROUND_ALPHA) {
						auto *alpha_attrib = reinterpret_cast<PangoAttrInt*>(attrib);
						alpha_attrib->value; // TODO
					}
				}

				// collect glyphs
				PangoRectangle run_layout;
				pango_layout_iter_get_run_extents(iter, nullptr, &run_layout);
				int x = run_layout.x;
				int y = pango_layout_iter_get_baseline(iter);
				auto &buffer = builder.allocRunPos(font, run->glyphs->num_glyphs);
				for (gint i = 0; i < run->glyphs->num_glyphs; ++i) {
					buffer.glyphs[i] = static_cast<SkGlyphID>(run->glyphs->glyphs[i].glyph);
					buffer.pos[i * 2] = static_cast<SkScalar>(
						pango_units_to_double(x + run->glyphs->glyphs[i].geometry.x_offset)
					);
					buffer.pos[i * 2 + 1] = static_cast<SkScalar>(
						pango_units_to_double(y + run->glyphs->glyphs[i].geometry.y_offset)
					);
					x += run->glyphs->glyphs[i].geometry.width;
				}

			} while (pango_layout_iter_next_run(iter));
			pango_layout_iter_free(iter);

			text._cached_text = builder.make();
		}

		SkCanvas *canvas = _render_stack.top().canvas;
		SkPaint paint;
		paint.setColor4f(_details::cast_colorf(colord(1.0, 1.0, 1.0, 1.0)), _color_space.get()); // TODO color
		pos += text._data.get_alignment_offset();
		canvas->drawTextBlob(
			text._cached_text, static_cast<SkScalar>(pos.x), static_cast<SkScalar>(pos.y), paint
		);
	}

	void renderer_base::draw_plain_text(const ui::plain_text &generic_text, vec2d topleft, colord color) {
		auto &text = _details::cast_plain_text(generic_text);

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
				run.glyphs[i] = static_cast<SkGlyphID>(glyph_infos[i].codepoint);
				run.pos[i * 2] = static_cast<SkScalar>(x + glyph_positions[i].x_offset / 64.0);
				run.pos[i * 2 + 1] = static_cast<SkScalar>(y_offset + glyph_positions[i].y_offset / 64.0);
				x += glyph_positions[i].x_advance / 64.0;
			}

			text._cached_text = builder.make();
		}

		SkCanvas *canvas = _render_stack.top().canvas;
		SkPaint paint;
		paint.setColor4f(_details::cast_colorf(color), _color_space.get());
		canvas->drawTextBlob(
			text._cached_text, static_cast<SkScalar>(topleft.x), static_cast<SkScalar>(topleft.y), paint
		);
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
		// TODO also take into account the scaling factor of the bitmap
		paint.setShader(img->makeShader(
			SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kLinear),
			_details::cast_matrix(mat)
		));
		return paint;
	}

	sk_sp<SkSurface> renderer_base::_create_surface_for_window(ui::window &wnd, vec2d scaling) {
		vec2d size = wnd.get_client_size();
		size.x *= scaling.x;
		size.y *= scaling.y;
		GrGLFramebufferInfo fbinfo;
		fbinfo.fFBOID = 0; // render to the default framebuffer
		fbinfo.fFormat = GL_RGBA8;
		GrBackendRenderTarget target(static_cast<int>(size.x), static_cast<int>(size.y), 1, 0, fbinfo);
		SkSurfaceProps props;
		return SkSurface::MakeFromBackendRenderTarget(
			_skia_context.get(), target, kBottomLeft_GrSurfaceOrigin,
			kRGBA_8888_SkColorType, _color_space, &props
		);
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
