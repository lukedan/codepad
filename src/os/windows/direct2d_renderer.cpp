// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/os/windows/direct2d_renderer.h"

/// \file
/// Implementation of the Direct2D renderer.

namespace codepad::os::direct2d {
	namespace _details { // cast objects to types specific to the direct2d renderer
		using namespace os::_details; // import stuff from common win32

		/// Converts a \ref matd3x3 to a \p D2D1_MATRIX_3X2_F.
		D2D1_MATRIX_3X2_F cast_matrix(matd3x3 m) {
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
		DWRITE_MATRIX cast_dwrite_matrix(matd3x3 m) {
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
		D2D1_COLOR_F cast_color(colord c) {
			return D2D1::ColorF(
				static_cast<FLOAT>(c.r),
				static_cast<FLOAT>(c.g),
				static_cast<FLOAT>(c.b),
				static_cast<FLOAT>(c.a)
			);
		}
		/// Converts a \ref rectd to a \p D2D1_RECT_F.
		D2D1_RECT_F cast_rect(rectd r) {
			return D2D1::RectF(
				static_cast<FLOAT>(r.xmin),
				static_cast<FLOAT>(r.ymin),
				static_cast<FLOAT>(r.xmax),
				static_cast<FLOAT>(r.ymax)
			);
		}
		/// Converts a \ref vec2d to a \p D2D1_POINT_2F.
		D2D1_POINT_2F cast_point(vec2d pt) {
			return D2D1::Point2F(static_cast<FLOAT>(pt.x), static_cast<FLOAT>(pt.y));
		}

		/// Constructs a \p DWRITE_TEXT_RANGE.
		DWRITE_TEXT_RANGE make_text_range(std::size_t beg, std::size_t len) {
			DWRITE_TEXT_RANGE result;
			result.startPosition = static_cast<UINT32>(beg);
			// this works for size_t::max() as well since both types are unsigned
			result.length = static_cast<UINT32>(len);
			return result;
		}

		/// Casts a \ref ui::font_style to a \p DWRITE_FONT_STYLE.
		DWRITE_FONT_STYLE cast_font_style(ui::font_style style) {
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
		DWRITE_FONT_WEIGHT cast_font_weight(ui::font_weight weight) {
			// TODO
			return DWRITE_FONT_WEIGHT_REGULAR;
		}
		/// Casts a \ref ui::font_stretch to a \p DWRITE_FONT_STRETCH.
		DWRITE_FONT_STRETCH cast_font_stretch(ui::font_stretch stretch) {
			// TODO
			return DWRITE_FONT_STRETCH_NORMAL;
		}
		/// Casts a \ref ui::horizontal_text_alignment to a \p DWRITE_TEXT_ALIGNMENT.
		DWRITE_TEXT_ALIGNMENT cast_horizontal_text_alignment(ui::horizontal_text_alignment align) {
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
		/// Casts a \ref ui::vertical_text_alignment to a \p DWRITE_PARAGRAPH_ALIGNMENT.
		DWRITE_PARAGRAPH_ALIGNMENT cast_vertical_text_alignment(ui::vertical_text_alignment align) {
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
		/// Casts a \ref ui::wrapping_mode to a \p DWRITE_WORD_WRAPPING.
		DWRITE_WORD_WRAPPING cast_wrapping_mode(ui::wrapping_mode wrap) {
			switch (wrap) {
			case ui::wrapping_mode::none:
				return DWRITE_WORD_WRAPPING_NO_WRAP;
			case ui::wrapping_mode::wrap:
				return DWRITE_WORD_WRAPPING_WRAP;
			}
			return DWRITE_WORD_WRAPPING_NO_WRAP; // should not be here
		}

		/// Underlying implementation of various <tt>cast_*</tt> functions.
		template <typename To, typename From> To &cast_object(From &f) {
#ifdef CP_CHECK_LOGICAL_ERRORS
			To *res = dynamic_cast<To*>(&f);
			assert_true_logical(res, "invalid object type");
			return *res;
#else
			return *static_cast<To*>(&f);
#endif
		}

		/// Casts a \ref ui::render_target to a \ref render_target.
		render_target &cast_render_target(ui::render_target &t) {
			return cast_object<render_target>(t);
		}
		/// Casts a \ref ui::bitmap to a \ref bitmap.
		bitmap &cast_bitmap(ui::bitmap &b) {
			return cast_object<bitmap>(b);
		}
		/// Casts a \ref ui::formatted_text to a \ref formatted_text.
		const formatted_text &cast_formatted_text(const ui::formatted_text &t) {
			return cast_object<const formatted_text>(t);
		}
		/// Casts a \ref ui::font to a \ref font.
		font &cast_font(ui::font &f) {
			return cast_object<font>(f);
		}
		/// Casts a \ref ui::font_family to a \ref font_family.
		font_family &cast_font_family(ui::font_family &f) {
			return cast_object<font_family>(f);
		}
		/// Casts a \ref ui::plain_text to a \ref plain_text.
		const plain_text &cast_plain_text(const ui::plain_text &f) {
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
		for (std::size_t i = 0; i < ressize; ++i) {
			resvec.emplace_back(bufptr[i].height, bufptr[i].baseline);
		}
		return resvec;
	}

	ui::caret_hit_test_result formatted_text::hit_test(vec2d pos) const {
		BOOL trailing, inside;
		DWRITE_HIT_TEST_METRICS metrics;
		_details::com_check(_text->HitTestPoint(
			static_cast<FLOAT>(pos.x), static_cast<FLOAT>(pos.y), &trailing, &inside, &metrics
		));
		return ui::caret_hit_test_result(
			static_cast<std::size_t>(metrics.textPosition),
			rectd::from_xywh(metrics.left, metrics.top, metrics.width, metrics.height),
			trailing != 0
		);
	}

	rectd formatted_text::get_character_placement(std::size_t pos) const {
		FLOAT px, py;
		DWRITE_HIT_TEST_METRICS metrics;
		_details::com_check(_text->HitTestTextPosition(static_cast<UINT32>(pos), false, &px, &py, &metrics));
		return rectd::from_xywh(metrics.left, metrics.top, metrics.width, metrics.height);
	}

	std::vector<rectd> formatted_text::get_character_range_placement(std::size_t beg, std::size_t len) const {
		constexpr std::size_t static_buffer_size = 20;
		static DWRITE_HIT_TEST_METRICS static_buffer[static_buffer_size];

		UINT32 count = 0;
		HRESULT hres = _text->HitTestTextRange(beg, len, 0.0f, 0.0f, static_buffer, static_buffer_size, &count);
		const DWRITE_HIT_TEST_METRICS *result_ptr = static_buffer;
		std::vector<DWRITE_HIT_TEST_METRICS> buffer;
		if (hres == E_NOT_SUFFICIENT_BUFFER) {
			buffer.resize(count);
			_details::com_check(_text->HitTestTextRange(beg, len, 0.0f, 0.0f, buffer.data(), count, &count));
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

	void formatted_text::set_text_color(colord c, std::size_t beg, std::size_t len) {
		_details::com_wrapper<ID2D1SolidColorBrush> brush;
		_rend->_d2d_device_context->CreateSolidColorBrush(_details::cast_color(c), brush.get_ref());
		_details::com_check(_text->SetDrawingEffect(brush.get(), _details::make_text_range(beg, len)));
	}

	void formatted_text::set_font_family(const std::u8string &family, std::size_t beg, std::size_t len) {
		_details::com_check(_text->SetFontFamilyName(
			_details::utf8_to_wstring(family).c_str(), _details::make_text_range(beg, len)
		));
	}

	void formatted_text::set_font_size(double size, std::size_t beg, std::size_t len) {
		_details::com_check(_text->SetFontSize(static_cast<FLOAT>(size), _details::make_text_range(beg, len)));
	}

	void formatted_text::set_font_style(ui::font_style style, std::size_t beg, std::size_t len) {
		_details::com_check(_text->SetFontStyle(
			_details::cast_font_style(style), _details::make_text_range(beg, len)
		));
	}

	void formatted_text::set_font_weight(ui::font_weight weight, std::size_t beg, std::size_t len) {
		_details::com_check(_text->SetFontWeight(
			_details::cast_font_weight(weight), _details::make_text_range(beg, len)
		));
	}

	void formatted_text::set_font_stretch(ui::font_stretch stretch, std::size_t beg, std::size_t len) {
		_details::com_check(_text->SetFontStretch(
			_details::cast_font_stretch(stretch), _details::make_text_range(beg, len)
		));
	}


	std::unique_ptr<ui::font> font_family::get_matching_font(
		ui::font_style style, ui::font_weight weight, ui::font_stretch stretch
	) const {
		auto result = std::make_unique<font>();
		_details::com_check(_family->GetFirstMatchingFont(
			_details::cast_font_weight(weight),
			_details::cast_font_stretch(stretch),
			_details::cast_font_style(style),
			result->_font.get_ref()
		));
		_details::com_check(result->_font->CreateFontFace(result->_font_face.get_ref()));
		result->_font_face->GetMetrics(&result->_metrics);
		return result;
	}


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
			// find sub-glyph position
			double ratio = (xpos - _cached_glyph_positions[glyphid]) / _glyph_advances[glyphid];
			std::size_t firstchar = _cached_glyph_to_char_mapping_starting[glyphid];
			std::size_t nchars = _cached_glyph_to_char_mapping_starting[glyphid + 1] - firstchar;
			assert_true_logical(nchars > 0, "glyph without corresponding character");
			ratio *= nchars;
			std::size_t offset = std::min(static_cast<std::size_t>(ratio), nchars - 1);

			result.character = _cached_glyph_to_char_mapping[firstchar + offset];
			result.rear = ratio - offset > 0.5;
			result.character_layout = _get_character_placement_impl(glyphid, offset);
		} else { // past the end
			result.character = _char_count;
			result.rear = false;
			result.character_layout = _get_character_placement_impl(glyphid, 0);
		}

		return result;
	}

	rectd plain_text::get_character_placement(std::size_t pos) const {
		_maybe_calculate_glyph_positions();
		_maybe_calculate_glyph_backmapping();

		std::size_t glyphid = _glyph_count, offset = 0;
		if (pos < _char_count) {
			glyphid = _cluster_map[pos];
			std::size_t beg = _cached_glyph_to_char_mapping_starting[glyphid];
			for (; _cached_glyph_to_char_mapping[beg + offset] != pos; ++offset) {
			}
		}
		return _get_character_placement_impl(glyphid, offset);
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
		if (_cached_glyph_to_char_mapping_starting.empty()) {
			// compute inverse map
			_cached_glyph_to_char_mapping.resize(_char_count);
			for (std::size_t i = 0; i < _char_count; ++i) {
				_cached_glyph_to_char_mapping[i] = i;
			}
			std::sort(
				_cached_glyph_to_char_mapping.begin(), _cached_glyph_to_char_mapping.end(),
				[this](std::size_t lhs, std::size_t rhs) {
					UINT16 lglyph = _cluster_map[lhs], rglyph = _cluster_map[rhs];
					if (lglyph != rglyph) {
						return lglyph < rglyph;
					}
					return lhs < rhs;
				}
			);

			// compute starting indices
			_cached_glyph_to_char_mapping_starting.reserve(_glyph_count + 1);
			std::size_t pos = 0;
			for (std::size_t i = 0; i < _glyph_count; ++i) {
				_cached_glyph_to_char_mapping_starting.emplace_back(pos);
				while (pos < _char_count && _cluster_map[_cached_glyph_to_char_mapping[pos]] == i) {
					++pos;
				}
			}
			_cached_glyph_to_char_mapping_starting.emplace_back(pos);
		}
	}

	rectd plain_text::_get_character_placement_impl(std::size_t glyphid, std::size_t charoffset) const {
		// horizontal
		double left = _cached_glyph_positions[glyphid], width = 0.0;
		if (glyphid < _glyph_count) {
			std::size_t count =
				_cached_glyph_to_char_mapping_starting[glyphid + 1] -
				_cached_glyph_to_char_mapping_starting[glyphid];
			width = _glyph_advances[glyphid] / count;
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
		_details::com_check(D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
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
		auto resrt = std::make_unique<render_target>();
		auto resbmp = std::make_unique<bitmap>();
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

	std::unique_ptr<ui::bitmap> renderer::load_bitmap(const std::filesystem::path &bmp, vec2d scaling_factor) {
		auto res = std::make_unique<bitmap>();
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

	std::unique_ptr<ui::font_family> renderer::find_font_family(const std::u8string &family) {
		_details::com_wrapper<IDWriteFontCollection> fonts;
		_details::com_check(_dwrite_factory->GetSystemFontCollection(fonts.get_ref(), false)); // no need to hurry

		UINT32 index = 0;
		BOOL exist = false;
		_details::com_check(fonts->FindFamilyName(os::_details::utf8_to_wstring(family).c_str(), &index, &exist));
		if (!exist) {
			return nullptr;
		}

		auto res = std::make_unique<font_family>();
		_details::com_check(fonts->GetFontFamily(index, res->_family.get_ref()));
		return res;
	}

	void renderer::begin_drawing(ui::window_base &w) {
		auto &data = _window_data::get(w);
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
			_details::com_check(_d2d_device_context->EndDraw());
			_d2d_device_context->SetTarget(nullptr); // so that windows can be resized normally
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
		const ui::generic_brush_parameters &brush, const ui::generic_pen_parameters &pen
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
		rectd rect, const ui::generic_brush_parameters &brush, const ui::generic_pen_parameters &pen
	) {
		_details::com_wrapper<ID2D1RectangleGeometry> geom;
		_details::com_check(_d2d_factory->CreateRectangleGeometry(_details::cast_rect(rect), geom.get_ref()));
		_draw_geometry(std::move(geom), brush, pen);
	}

	void renderer::draw_rounded_rectangle(
		rectd region, double radiusx, double radiusy,
		const ui::generic_brush_parameters &brush, const ui::generic_pen_parameters &pen
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
		const ui::generic_brush_parameters &brush, const ui::generic_pen_parameters &pen
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

	std::unique_ptr<ui::formatted_text> renderer::create_formatted_text(
		std::u8string_view text, const ui::font_parameters &params, colord c, vec2d maxsize, ui::wrapping_mode wrap,
		ui::horizontal_text_alignment halign, ui::vertical_text_alignment valign
	) {
		auto converted = _details::utf8_to_wstring(text);
		return _create_formatted_text_impl(converted, params, c, maxsize, wrap, halign, valign);
	}

	std::unique_ptr<ui::formatted_text> renderer::create_formatted_text(
		std::basic_string_view<codepoint> text, const ui::font_parameters &params, colord c,
		vec2d maxsize, ui::wrapping_mode wrap,
		ui::horizontal_text_alignment halign, ui::vertical_text_alignment valign
	) {
		std::basic_string<std::byte> bytestr;
		for (codepoint cp : text) {
			bytestr += encodings::utf16<>::encode_codepoint(cp);
		}
		return _create_formatted_text_impl(
			std::basic_string_view<WCHAR>(reinterpret_cast<const WCHAR*>(bytestr.c_str()), bytestr.size() / 2),
			params, c, maxsize, wrap, halign, valign
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

	std::unique_ptr<ui::plain_text> renderer::create_plain_text(
		std::u8string_view text, ui::font &font, double size
	) {
		auto utf16 = _details::utf8_to_wstring(text);
		return _create_plain_text_impl(utf16, font, size);
	}

	std::unique_ptr<ui::plain_text> renderer::create_plain_text(
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
			// determine if & how to apply pixel snapping
			if (!trans.has_rotation_or_nonrigid()) {
				double ypos = trans[1][2] + pos.y;
				// snap to physical pixels
				ypos *= scale.y;
				pos.y += (std::round(ypos) - ypos) / scale.y;
			}

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
						auto brush = _create_brush(ui::brush_parameters::solid_color(color));
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
						auto brush = _create_brush(ui::brush_parameters::solid_color(run_color));
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
		const ui::generic_brush_parameters &brush_def,
		const ui::generic_pen_parameters &pen_def
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
		// that everything blends correctly
		// also the mode is set here instead of during PopLayer() as per the documentation at:
		// https://docs.microsoft.com/en-us/windows/win32/direct2d/direct2d-layers-overview#blend-modes
		_d2d_device_context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
		_d2d_device_context->PushLayer(D2D1::LayerParameters1(
			D2D1::InfiniteRect(), clip.get(), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
			D2D1::IdentityMatrix(), 1.0f, nullptr, D2D1_LAYER_OPTIONS1_INITIALIZE_FROM_BACKGROUND
		), nullptr);
		/*_d2d_device_context->PushLayer(D2D1::LayerParameters(
			D2D1::InfiniteRect(), clip.get(), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
			D2D1::IdentityMatrix(), 1.0f, nullptr, D2D1_LAYER_OPTIONS_INITIALIZE_FOR_CLEARTYPE
		), nullptr);*/
		_d2d_device_context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
	}

	_details::com_wrapper<ID2D1SolidColorBrush> renderer::_create_brush(
		const ui::brush_parameters::solid_color &brush_def
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
		const ui::brush_parameters::linear_gradient &brush_def
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
		const ui::brush_parameters::radial_gradient &brush_def
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
		const ui::brush_parameters::bitmap_pattern &brush_def
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

	_details::com_wrapper<ID2D1Brush> renderer::_create_brush(const ui::brush_parameters::none&) {
		return _details::com_wrapper<ID2D1Brush>();
	}

	_details::com_wrapper<ID2D1Brush> renderer::_create_brush(const ui::generic_brush_parameters &b) {
		auto brush = std::visit([this](auto &&brush) {
			return _details::com_wrapper<ID2D1Brush>(_create_brush(brush));
			}, b.value);
		if (brush) {
			brush->SetTransform(_details::cast_matrix(b.transform));
		}
		return brush;
	}


	std::unique_ptr<formatted_text> renderer::_create_formatted_text_impl(
		std::basic_string_view<WCHAR> text, const ui::font_parameters &fmt, colord c,
		vec2d maxsize, ui::wrapping_mode wrap,
		ui::horizontal_text_alignment halign, ui::vertical_text_alignment valign
	) {
		// use new to access protedted constructor
		auto res = std::unique_ptr<formatted_text>(new formatted_text(*this));
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
		_details::com_check(format->SetWordWrapping(_details::cast_wrapping_mode(wrap)));
		_details::com_check(format->SetTextAlignment(_details::cast_horizontal_text_alignment(halign)));
		_details::com_check(format->SetParagraphAlignment(_details::cast_vertical_text_alignment(valign)));
		_details::com_check(_dwrite_factory->CreateTextLayout(
			text.data(), static_cast<UINT32>(text.size()),
			format.get(),
			static_cast<FLOAT>(maxsize.x), static_cast<FLOAT>(maxsize.y),
			res->_text.get_ref()
		));
		res->set_text_color(c, 0, std::numeric_limits<std::size_t>::max());
		return res;
	}

	std::unique_ptr<plain_text> renderer::_create_plain_text_impl(
		std::basic_string_view<WCHAR> text, ui::font &f, double size
	) {
		auto result = std::make_unique<plain_text>();
		result->_font_face = _details::cast_font(f)._font_face;

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

		result->_font_size = size;

		// since directwrite treats a surrogate pair (one codepoint) as two characters, here we find all
		// surrogate pairs, remove duplicate entries in the cluster map, and correct the number of characters
		result->_char_count = text.size();
		for (std::size_t i = 0, reduced = 0; i < text.size(); ++i, ++reduced) {
			// if the last unit starts a surrogate pair, then the codepoint is invalid; however this means that
			// there's no duplicate stuff so nothing needs to be done
			if (i + 1 < text.size()) {
				if (
					(static_cast<std::uint16_t>(text[i]) & encodings::utf16<>::mask_pair) ==
					encodings::utf16<>::patt_pair
					) {
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

	void renderer::_new_window(ui::window_base &w) {
		window &wnd = _details::cast_window(w);
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
			_d3d_device.get(), wnd.get_native_handle(), &swapchain_desc,
			nullptr, nullptr, actual_data.swap_chain.get_ref()
		));
		// set data
		actual_data.target = _create_bitmap_from_swap_chain(
			actual_data.swap_chain.get(), wnd.get_scaling_factor()
		);
		data.emplace<_window_data>(actual_data);
		// resize buffer when the window size has changed
		wnd.size_changed += [this, pwnd = &wnd](ui::window_base::size_changed_info&) {
			auto &data = _window_data::get(*pwnd);
			data.target.reset(); // must release bitmap before resizing
			_details::com_check(data.swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0));
			// recreate bitmap
			data.target = _create_bitmap_from_swap_chain(data.swap_chain.get(), pwnd->get_scaling_factor());
		};
		// reallocate buffer when the window scaling has changed
		wnd.scaling_factor_changed += [this, pwnd = &wnd](ui::window_base::scaling_factor_changed_info &p) {
			auto &data = _window_data::get(*pwnd);
			data.target.reset(); // must release bitmap before resizing
			_details::com_check(data.swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0));
			// recreate bitmap
			data.target = _create_bitmap_from_swap_chain(data.swap_chain.get(), p.new_value);
		};
	}

	void renderer::_delete_window(ui::window_base &w) {
		_get_window_data(w).reset();
	}


	renderer::_window_data &renderer::_window_data::get(ui::window_base &wnd) {
		_window_data *d = std::any_cast<_window_data>(&_get_window_data(wnd));
		assert_true_usage(d, "window has no associated data");
		return *d;
	}
}
