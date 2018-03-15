#include "software_renderer_base.h"

/// \file
/// Implementation of certain functions of codepad::os::software_renderer_base.

namespace codepad::os {
	/// Returns the color multiplied by the given \ref blend_factor.
	///
	/// \param src The `source' color.
	/// \param dst The `destination' color.
	/// \param target The color to be multiplied, usually either \p src or \p dst.
	/// \param factor The blend factor to multiply \p target with.
	template <typename Color> inline static Color _get_blend_diff(
		Color src, Color dst, Color target, blend_factor factor
	) {
		switch (factor) {
		case blend_factor::one:
			return target;
		case blend_factor::zero:
			return Color(0.0f, 0.0f, 0.0f, 0.0f);
		case blend_factor::source_alpha:
			return target * src.a;
		case blend_factor::one_minus_source_alpha:
			return target * (1.0f - src.a);
		case blend_factor::destination_alpha:
			return target * dst.a;
		case blend_factor::one_minus_destination_alpha:
			return target * (1.0f - dst.a);
		case blend_factor::source_color:
			return Color(target.r * src.r, target.g * src.g, target.b * src.b, target.a * src.a);
		case blend_factor::one_minus_source_color:
			return Color(
				target.r * (1.0f - src.r), target.g * (1.0f - src.g),
				target.b * (1.0f - src.b), target.a * (1.0f - src.a)
			);
		case blend_factor::destination_color:
			return Color(target.r * dst.r, target.g * dst.g, target.b * dst.b, target.a * dst.a);
		case blend_factor::one_minus_destination_color:
			return Color(
				target.r * (1.0f - dst.r), target.g * (1.0f - dst.g),
				target.b * (1.0f - dst.b), target.a * (1.0f - dst.a)
			);
		}
		return Color(); // shouldn't happen
	}

	/// Blends the two colors together, and returns the result.
	///
	/// \param src The `source' color.
	/// \param dst The `destination' color.
	/// \param srcf The factor to blend \p src with.
	/// \param dstf The factor to blend \p dst with.
	software_renderer_base::_color_t software_renderer_base::_blend_colors(
		_color_t src, _color_t dst, blend_factor srcf, blend_factor dstf
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
