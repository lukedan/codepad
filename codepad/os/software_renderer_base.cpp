// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "software_renderer_base.h"

/// \file
/// Implementation of certain functions of codepad::os::software_renderer_base.

using namespace codepad::ui;

namespace codepad::os {
#ifdef CP_USE_SSE2
	void software_renderer_base::_ivec4f::set_all_aligned(const float *ptr) {
		xyzw = _mm_load_ps(ptr);
	}
	void software_renderer_base::_ivec4f::set_all(const float *ptr) {
		xyzw = _mm_loadu_ps(ptr);
	}
	void software_renderer_base::_ivec4f::set_all(float x, float y, float z, float w) {
		xyzw = _mm_set_ps(w, z, y, x);
	}
	void software_renderer_base::_ivec4f::set_uniform(float v) {
		xyzw = _mm_load1_ps(&v);
	}
	void software_renderer_base::_ivec4f::get_all_aligned(float *v) const {
		_mm_store_ps(v, xyzw);
	}
	void software_renderer_base::_ivec4f::get_all(float *v) const {
		_mm_storeu_ps(v, xyzw);
	}
	float software_renderer_base::_ivec4f::get_x() const {
		float v;
		_mm_store_ss(&v, xyzw);
		return v;
	}
	software_renderer_base::_ivec4f software_renderer_base::_ivec4f::add(_ivec4f lhs, _ivec4f rhs) {
		return _ivec4f(_mm_add_ps(lhs.xyzw, rhs.xyzw));
	}
	software_renderer_base::_ivec4f software_renderer_base::_ivec4f::subtract(_ivec4f lhs, _ivec4f rhs) {
		return _ivec4f(_mm_sub_ps(lhs.xyzw, rhs.xyzw));
	}
	software_renderer_base::_ivec4f software_renderer_base::_ivec4f::multiply_elem(_ivec4f lhs, _ivec4f rhs) {
		return _ivec4f(_mm_mul_ps(lhs.xyzw, rhs.xyzw));
	}
	software_renderer_base::_ivec4f software_renderer_base::_ivec4f::divide_elem(_ivec4f lhs, _ivec4f rhs) {
		return _ivec4f(_mm_div_ps(lhs.xyzw, rhs.xyzw));
	}
	software_renderer_base::_ivec4i32 software_renderer_base::_ivec4f::convert_to_int_truncate() const {
		return _ivec4i32(_mm_cvttps_epi32(xyzw));
	}

	software_renderer_base::_ivec4f software_renderer_base::_ivec4i32::convert_to_float() const {
		return _ivec4f(_mm_cvtepi32_ps(xyzw));
	}
	int software_renderer_base::_ivec4i32::pack() const {
		return _mm_cvtsi128_si32(_mm_packus_epi16(_mm_packs_epi32(xyzw, __m128i()), __m128i()));
	}
	int software_renderer_base::_ivec4i32::get_x() const {
		return _mm_cvtsi128_si32(xyzw);
	}
#else
	void software_renderer_base::_ivec4f::set_all_aligned(const float *ptr) {
		x = ptr[0];
		y = ptr[1];
		z = ptr[2];
		w = ptr[3];
	}
	void software_renderer_base::_ivec4f::set_all(const float *ptr) {
		x = ptr[0];
		y = ptr[1];
		z = ptr[2];
		w = ptr[3];
	}
	void software_renderer_base::_ivec4f::set_all(float xv, float yv, float zv, float wv) {
		x = xv;
		y = yv;
		z = zv;
		w = wv;
	}
	void software_renderer_base::_ivec4f::set_uniform(float v) {
		x = y = z = w = v;
	}
	void software_renderer_base::_ivec4f::get_all_aligned(float *v) const {
		v[0] = x;
		v[1] = y;
		v[2] = z;
		v[3] = w;
	}
	void software_renderer_base::_ivec4f::get_all(float *v) const {
		v[0] = x;
		v[1] = y;
		v[2] = z;
		v[3] = w;
	}
	float software_renderer_base::_ivec4f::get_x() const {
		return x;
	}
	software_renderer_base::_ivec4f software_renderer_base::_ivec4f::add(_ivec4f lhs, _ivec4f rhs) {
		return _ivec4f(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w);
	}
	software_renderer_base::_ivec4f software_renderer_base::_ivec4f::subtract(_ivec4f lhs, _ivec4f rhs) {
		return _ivec4f(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w);
	}
	software_renderer_base::_ivec4f software_renderer_base::_ivec4f::multiply_elem(_ivec4f lhs, _ivec4f rhs) {
		return _ivec4f(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w);
	}
	software_renderer_base::_ivec4f software_renderer_base::_ivec4f::divide_elem(_ivec4f lhs, _ivec4f rhs) {
		return _ivec4f(lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z, lhs.w / rhs.w);
	}
	software_renderer_base::_ivec4i32 software_renderer_base::_ivec4f::convert_to_int_truncate() const {
		return software_renderer_base::_ivec4i32(
			static_cast<int>(x), static_cast<int>(y), static_cast<int>(z), static_cast<int>(w)
		);
	}

	software_renderer_base::_ivec4f software_renderer_base::_ivec4i32::convert_to_float() const {
		return _ivec4f(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(w));
	}
	int software_renderer_base::_ivec4i32::pack() const {
		return (x & 0xFF) | ((y & 0xFF) << 8) | ((z & 0xFF) << 16) | ((w & 0xFF) << 24);
	}
	int software_renderer_base::_ivec4i32::get_x() const {
		return x;
	}
#endif


	software_renderer_base::_ivec4f software_renderer_base::_get_blend_diff(
		_ivec4f src, _ivec4f dst, _ivec4f target, blend_factor factor
	) {
		switch (factor) {
		case blend_factor::one:
			return target;
		case blend_factor::zero:
			{
				_ivec4f v;
				v.set_uniform(0.0f);
				return v;
			}
		case blend_factor::source_alpha:
			return target * src.get_x();
		case blend_factor::one_minus_source_alpha:
			return target * (1.0f - src.get_x());
		case blend_factor::destination_alpha:
			return target * dst.get_x();
		case blend_factor::one_minus_destination_alpha:
			return target * (1.0f - dst.get_x());
		case blend_factor::source_color:
			return target * src;
		case blend_factor::one_minus_source_color:
			{
				_ivec4f v;
				v.set_uniform(1.0f);
				return target * (v - src);
			}
		case blend_factor::destination_color:
			return target * dst;
		case blend_factor::one_minus_destination_color:
			{
				_ivec4f v;
				v.set_uniform(1.0f);
				return target * (v - dst);
			}
		}
		return _ivec4f(); // shouldn't happen
	}

	software_renderer_base::_ivec4f software_renderer_base::_blend_colors(
		_ivec4f src, _ivec4f dst, blend_factor srcf, blend_factor dstf
	) {
		return _get_blend_diff(src, dst, src, srcf) + _get_blend_diff(src, dst, dst, dstf);
	}

	/// Moves a point on \f$y = kx + d\f$ from \f$(x, y)\f$ to \f$(v, kv + d)\f$.
	template <typename Real> inline void _clip_line_onedir_fixup(Real &x, Real &y, Real v, Real k) {
		y += k * (v - x);
		x = v;
	}
	/// Clips the line \f$(fx, fy) \rightarrow (tx, ty)\f$ to fit in the range \f$[xmin, xmax]\f$.
	template <typename Real> inline bool _clip_line_onedir(
		Real &fx, Real &fy, Real &tx, Real &ty, Real xmin, Real xmax
	) {
		if (fx < tx) {
			if (tx < xmin || fx > xmax) {
				return false;
			}
			Real k = (ty - fy) / (tx - fx);
			if (fx < xmin) {
				_clip_line_onedir_fixup(fx, fy, xmin, k);
			}
			if (tx > xmax) {
				_clip_line_onedir_fixup(tx, ty, xmax, k);
			}
		} else {
			if (fx < xmin || tx > xmax) {
				return false;
			}
			Real k = (ty - fy) / (tx - fx);
			if (fx > xmax) {
				_clip_line_onedir_fixup(fx, fy, xmax, k);
			}
			if (tx < xmin) {
				_clip_line_onedir_fixup(tx, ty, xmin, k);
			}
		}
		return true;
	}

	void software_renderer_base::_draw_line(_vec2_t p1, _vec2_t p2, _color_t c) {
		if (p1.x + p1.y > p2.x + p2.y) {
			std::swap(p1, p2);
		}
		_vec2_t diff = p2 - p1;
		recti crgn = _clipstk.back();
		if (diff.x < 0.0f ? true : (diff.y < 0.0f ? false : (std::fabs(diff.y) > std::fabs(diff.x)))) {
			if (_clip_line_onedir(
				p1.x, p1.y, p2.x, p2.y,
				static_cast<_real_t>(crgn.xmin) + 0.5f,
				static_cast<_real_t>(crgn.xmax) - 0.5f
			)) {
				_draw_line_up(p1.y, p1.x, p2.y, diff.x / diff.y, c);
			}
		} else {
			if (_clip_line_onedir(
				p1.y, p1.x, p2.y, p2.x,
				static_cast<_real_t>(crgn.ymin) + 0.5f,
				static_cast<_real_t>(crgn.ymax) - 0.5f
			)) {
				_draw_line_right(p1.x, p1.y, p2.x, diff.y / diff.x, c);
			}
		}
	}
}
