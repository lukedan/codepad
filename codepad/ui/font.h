// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Generic font related enums and classes.

#include <map>
#include <unordered_map>
#include <variant>
#include <optional>

#include "../core/misc.h"
#include "renderer.h"
#include "atlas.h"

namespace codepad::ui {
	/// The style of a font's characters.
	enum class font_style {
		normal = 0, ///< Normal.
		bold = 1, ///< Bold.
		italic = 2, ///< Italic.
		bold_italic = bold | italic ///< Bold and italic.
	};
}
namespace codepad {
	/// Enables bitwise operators for \ref font_style.
	template <> struct enable_enum_bitwise_operators<ui::font_style> : std::true_type {
	};
}

namespace codepad::ui {
	class font_manager;

	template <typename Prim, typename Bkup> class backed_up_font;
	/// The base class that declares the basic interface of a font.
	class font {
		template <typename Prim, typename Bkup> friend class backed_up_font;
	public:
		/// Represents a character of the font.
		struct entry {
			/// The placement of the texture, with respect to the `pen', when the character is rendered.
			rectd placement;
			/// The distance that the `pen' should be moved forward to render the next character.
			double advance = 0.0;
			/// The texture of the character.
			atlas::id_t texture;
		};
		/// Information about how a character should be rendered.
		struct character_rendering_info {
			/// Initializes all fields of this struct.
			character_rendering_info(rectd p, atlas::id_t tex, entry &ent) : placement(p), texture(tex), entry(ent) {
			}

			rectd placement; ///< The placement of the texture.
			atlas::id_t texture; ///< The texture of this character, which may be different from the default texture.
			entry &entry; ///< Contains information about the metrics of the character.
		};

		/// Initializes this font with the corresponding \ref font_manager.
		explicit font(font_manager &man) : _manager(man) {
		}
		/// No move construction.
		font(font&&) = delete;
		/// No copy construction.
		font(const font&) = delete;
		/// No move assignment.
		font &operator=(font&&) = delete;
		/// No copy assignment.
		font &operator=(const font&) = delete;
		/// Default destructor.
		virtual ~font() = default;

		/// Returns whether this font has a valid character entry for the given codepoint.
		virtual bool has_valid_char_entry(codepoint) const = 0;
		/// Returns the font entry corresponding to the given codepoint.
 		virtual const entry &get_char_entry(codepoint c) const {
			bool dummy;
			return _get_modify_char_entry(c, dummy);
		}

		/// Returns information used to render a character.
		virtual character_rendering_info draw_character(codepoint, vec2d) const = 0;

		/// Returns the width of the widest character of the given string.
		double get_max_width_charset(const std::u32string &s) const {
			double maxw = 0.0;
			for (codepoint c : s) {
				maxw = std::max(maxw, get_char_entry(c).advance);
			}
			return maxw;
		}

		/// Returns the height of a line for this font.
		virtual double height() const = 0;
		/// Returns the maximum width of a character of this font.
		virtual double max_width() const = 0;
		/// Returns the distance from the top of a line to the baseline.
		virtual double baseline() const = 0;
		/// Returns the kerning between the two given characters.
		virtual vec2d get_kerning(codepoint, codepoint) const = 0;

		/// Returns the corresponding \ref font_manager.
		font_manager &get_manager() const {
			return _manager;
		}
	protected:
		/// Returns a non-const reference to the \ref entry of the given character.
		///
		/// \param c The character.
		/// \param isnew Returns whether the entry has just been recorded.
		virtual entry &_get_modify_char_entry(codepoint c, bool &isnew) const = 0;
	private:
		font_manager &_manager; ///< The manager of this font.
	};

	/// Creates a font with the given parameters. This function is system-dependent.
	std::shared_ptr<font> create_font(font_manager&, str_view_t, double, font_style);
}
