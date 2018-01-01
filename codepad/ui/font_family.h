#pragma once

#include <string>
#include <memory>

#include "../utilities/encodings.h"
#include "../utilities/misc.h"
#include "../os/current.h"

namespace codepad {
	namespace ui {
		struct font_family {
			struct baseline_info {
				baseline_info() = default;
				baseline_info(double n, double b, double i, double bi) :
					normal_diff(n), bold_diff(b), italic_diff(i), bold_italic_diff(bi) {
				}

				double normal_diff, bold_diff, italic_diff, bold_italic_diff;

				double get(font_style fs) {
					switch (fs) {
					case font_style::normal:
						return normal_diff;
					case font_style::bold:
						return bold_diff;
					case font_style::italic:
						return italic_diff;
					case font_style::bold_italic:
						return bold_italic_diff;
					}
					assert_true_usage(false, "invalid font style encountered");
					return 0.0;
				}
			};

			font_family() = default;
			font_family(const str_t &family, double size) :
				normal(std::make_shared<os::default_font>(family, size, font_style::normal)),
				bold(std::make_shared<os::default_font>(family, size, font_style::bold)),
				italic(std::make_shared<os::default_font>(family, size, font_style::italic)),
				bold_italic(std::make_shared<os::default_font>(family, size, font_style::bold_italic)) {
			}
			font_family(
				std::shared_ptr<const os::font> n, std::shared_ptr<const os::font> b,
				std::shared_ptr<const os::font> i, std::shared_ptr<const os::font> bi
			) : normal(std::move(n)), bold(std::move(b)), italic(std::move(i)), bold_italic(std::move(bi)) {
			}

			std::shared_ptr<const os::font> normal, bold, italic, bold_italic;

			double maximum_width() const {
				return std::max({normal->max_width(), bold->max_width(), italic->max_width(), bold_italic->max_width()});
			}
			double maximum_height() const {
				return std::max({normal->height(), bold->height(), italic->height(), bold_italic->height()});
			}
			double common_baseline() const {
				return std::max({normal->baseline(), bold->baseline(), italic->baseline(), bold_italic->baseline()});
			}
			baseline_info get_baseline_info() const {
				double bl = common_baseline();
				return baseline_info(
					bl - normal->baseline(), bl - bold->baseline(), bl - italic->baseline(), bl - bold_italic->baseline()
				);
			}
			const std::shared_ptr<const os::font> &get_by_style(font_style fs) const {
				switch (fs) {
				case font_style::normal:
					return normal;
				case font_style::bold:
					return bold;
				case font_style::italic:
					return italic;
				case font_style::bold_italic:
					return bold_italic;
				}
				assert_true_usage(false, "invalid font style encountered");
				return normal;
			}
		};
	}
}
