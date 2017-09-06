#pragma once

#include "../utilities/misc.h"
#include "renderer.h"

namespace codepad {
	enum class font_style {
		normal = 0,
		bold = 1,
		italic = 2,
		bold_italic = bold | italic
	};
	namespace os {
		class font {
		public:
			struct entry {
				rectd placement;
				double advance;
				os::texture texture;
			};

			font() = default;
			font(const font&) = delete;
			font(font&&) = delete;
			font &operator=(const font&) = delete;
			font &operator=(font&&) = delete;
			virtual ~font() {
			}

			virtual const entry &get_char_entry(char_t) const = 0;

			virtual double height() const = 0;
			virtual double max_width() const = 0;
			virtual double baseline() const = 0;
			virtual vec2d get_kerning(char_t, char_t) const = 0;
		};
	}
}
