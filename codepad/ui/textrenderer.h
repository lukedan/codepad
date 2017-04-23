#pragma once

#include "../utilities/textconfig.h"
#include "../utilities/textproc.h"
#include "../utilities/font.h"
#include "../platform/interface.h"

namespace codepad {
	namespace ui {
		namespace text_renderer {
			inline void render_plain_text(const str_t &str, font &fnt, vec2d topleft, colord color) {
				int sx = static_cast<int>(std::round(topleft.x)), dy = static_cast<int>(std::ceil(fnt.height()));
				vec2i cur = vec2i(sx, static_cast<int>(std::round(topleft.y)));
				char last = '\0';
				double lastw = 0.0;
				for (auto i = str.begin(); i != str.end(); ++i) {
					if (is_newline(*i)) {
						cur.x = sx;
						cur.y += dy;
						last = '\0';
					} else {
						const font::entry &et = fnt.get_char_entry(*i);
						if (last != '\0') {
							cur.x += static_cast<int>(std::round(lastw + fnt.get_kerning(last, *i).x));
						}
						fnt.bound_renderer().draw_character(et.texture, cur.convert<double>() + et.placement.xmin_ymin(), color);
						last = *i;
						lastw = et.advance;
					}
				}
			}
			inline vec2d measure_plain_text(const str_t &str, font &fnt) {
				char last = '\0';
				double lastw = 0.0, curline = 0.0, maxw = 0.0;
				size_t linen = 1;
				for (auto i = str.begin(); i != str.end(); ++i) {
					const font::entry &et = fnt.get_char_entry(*i);
					if (last != '\0') {
						curline += static_cast<int>(std::round(lastw + fnt.get_kerning(last, *i).x));
					}
					if (is_newline(*i)) {
						++linen;
						last = '\0';
						maxw = std::max(maxw, curline + et.advance);
						lastw = curline = 0.0;
					} else {
						last = *i;
						lastw = et.advance;
					}
				}
				return vec2d(std::max(maxw, curline + lastw), linen * std::ceil(fnt.height()));
			}
		}
	}
}
