#pragma once

/// \file
/// Manages different styles and weights of the same font.

#include <string>
#include <memory>

#include "../core/encodings.h"
#include "../core/misc.h"
#include "../os/current/font.h"

namespace codepad::ui {
	/// Contains four styles of the same font: normal, bold, italic, bold-italic.
	struct font_family {
		/// Contains information about the offsets required for all four styles to have the common baseline.
		struct baseline_info {
			/// Default constructor.
			baseline_info() = default;
			/// Initializes the four offsets with the values provided.
			baseline_info(double n, double b, double i, double bi) :
				normal_diff(n), bold_diff(b), italic_diff(i), bold_italic_diff(bi) {
			}

			double
				normal_diff = 0.0, ///< The offset for font_style::normal.
				bold_diff = 0.0, ///< The offset for font_style::bold.
				italic_diff = 0.0, ///< The offset for font_style::italic.
				bold_italic_diff = 0.0; ///< The offset for font_style::bold_italic.

			/// Returns the offset that corresponds to a given \ref font_style.
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

		/// Default constructor.
		font_family() = default;
		/// Constructs the font family by loading the four fonts with different
		/// styles that correspond to the given font name and size.
		font_family(const str_t &family, double size) :
			normal(std::make_shared<os::default_font>(family, size, font_style::normal)),
			bold(std::make_shared<os::default_font>(family, size, font_style::bold)),
			italic(std::make_shared<os::default_font>(family, size, font_style::italic)),
			bold_italic(std::make_shared<os::default_font>(family, size, font_style::bold_italic)) {
		}
		/// Constructs the font family with four <tt>std::shared_ptr</tt>s
		/// that either are empty or point to existing fonts.
		font_family(
			std::shared_ptr<const os::font> n, std::shared_ptr<const os::font> b,
			std::shared_ptr<const os::font> i, std::shared_ptr<const os::font> bi
		) : normal(std::move(n)), bold(std::move(b)), italic(std::move(i)), bold_italic(std::move(bi)) {
		}

		std::shared_ptr<const os::font>
			normal, ///< Pointer to the font that corresponds to font_style::normal.
			bold, ///< Pointer to the font that corresponds to font_style::bold.
			italic, ///< Pointer to the font that corresponds to font_style::italic.
			bold_italic; ///< Pointer to the font that corresponds to font_style::bold_italic.

		/// Returns the maximum width of all characters of the four fonts.
		/// Assumes that all four pointers are non-empty.
		double maximum_width() const {
			return std::max({normal->max_width(), bold->max_width(), italic->max_width(), bold_italic->max_width()});
		}
		/// Returns the maximum height of the four fonts.
		/// Assumes that all four pointers are non-empty.
		double maximum_height() const {
			return std::max({normal->height(), bold->height(), italic->height(), bold_italic->height()});
		}
		/// Returns the common baseline of the four fonts, i.e., the maximum baseline of the four fonts.
		/// Assumes that all four pointers are non-empty.
		double common_baseline() const {
			return std::max({normal->baseline(), bold->baseline(), italic->baseline(), bold_italic->baseline()});
		}
		/// Returns a \ref baseline_info containing the offsets required for the four fonts to
		/// have a common baseline. Assumes that all four pointers are non-empty.
		baseline_info get_baseline_info() const {
			double bl = common_baseline();
			return baseline_info(
				bl - normal->baseline(), bl - bold->baseline(), bl - italic->baseline(), bl - bold_italic->baseline()
			);
		}
		/// Returns the pointer to the font corresponding to the given \ref font_style.
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
