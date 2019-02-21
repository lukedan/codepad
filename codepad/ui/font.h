// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Generic font related enums and classes.

#include <map>
#include <unordered_map>
#include <deque>
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
	class manager;
	class font_manager;

	/// The base class that declares the basic interface of a font.
	class font {
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
			character_rendering_info(rectd p, atlas::id_t tex, entry &ent) :
				placement(p), texture(tex), char_entry(ent) {
			}

			rectd placement; ///< The placement of the texture.
			atlas::id_t texture; ///< The texture of this character, which may be different from the default texture.
			entry &char_entry; ///< Contains information about the metrics of the character.
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

	/// The parameters that uniquely identify a loaded font.
	struct font_parameters {
		/// Default constructor.
		font_parameters() = default;
		/// Initializes all fields of this struct.
		explicit font_parameters(str_t n, size_t sz = 10, font_style st = font_style::normal) :
			name(std::move(n)), size(sz), style(st) {
		}

		str_t name; ///< The name of the font.
		size_t size = 10; ///< Font size.
		font_style style = font_style::normal; ///< The \ref font_style.

		/// Equality.
		friend bool operator==(const font_parameters &lhs, const font_parameters &rhs) {
			return lhs.style == rhs.style && lhs.size == rhs.size && lhs.name == rhs.name;
		}
		/// Inequality.
		friend bool operator!=(const font_parameters &lhs, const font_parameters &rhs) {
			return !(lhs == rhs);
		}
		/// Comparison for this to be used in \p std::map.
		friend bool operator<(const font_parameters &lhs, const font_parameters &rhs) {
			if (lhs.style == rhs.style) {
				if (lhs.size == rhs.size) {
					return lhs.name < rhs.name;
				}
				return lhs.size < rhs.size;
			}
			return lhs.style < rhs.style;
		}
	};
	/// Manages a list of font names and fonts, and creates a texture atlas for all characters.
	class font_manager {
	public:
		/// The default value for \ref _max_preserved_fonts.
		constexpr static size_t default_max_preserved_fonts = 10;

		/// Initializes the class with the corresponding \ref manager, and the \ref atlas with its \ref renderer_base.
		explicit font_manager(manager&);

		/// Returns the atlas.
		atlas &get_atlas() {
			return _atlas;
		}
		/// \overload
		const atlas &get_atlas() const {
			return _atlas;
		}
		/// Returns the corresponding \ref manager.
		manager &get_manager() const {
			return _manager;
		}

		/// Returns the maximum number of fonts that are not unloaded even if all references are removed.
		size_t get_max_preserved_fonts() const {
			return _max_preserved_fonts;
		}
		/// Sets the maximum number of fonts that are not unloaded even if all references are removed.
		void set_max_preserved_fonts(size_t value) {
			_max_preserved_fonts = value;
			while (_preserved_fonts.size() > _max_preserved_fonts) {
				_preserved_fonts.pop_front();
			}
		}

		/// Returns the \ref font that corresponds to the given \ref font_parameters.
		std::shared_ptr<const font> get_font(const font_parameters &params) {
			auto[it, inserted] = _font_mapping.try_emplace(params);
			if (inserted) {
				auto res = it->second.lock();
				if (res) {
					return res;
				}
			}
			// needs to be loaded
			std::shared_ptr<const font> res = _load_font(params);
			it->second = res;
			// add to preserved font
			_preserved_fonts.emplace_back(res);
			if (_preserved_fonts.size() > _max_preserved_fonts) {
				_preserved_fonts.pop_front();
			}
			return res;
		}
		/// \overload
		std::shared_ptr<const font> get_font(str_view_t name, size_t size, font_style style = font_style::normal) {
			return get_font(font_parameters(str_t(name), size, style));
		}

		/// Returns the parameters of the system-default UI font. This function is platform-dependent.
		static font_parameters get_default_ui_font_parameters();
	protected:
		atlas _atlas; ///< The texture atlas.
		/// The mapping between font parameers and possibly loaded fonts.
		std::map<font_parameters, std::weak_ptr<const font>> _font_mapping;
		/// The list of recently loaded fonts that are not unloaded even if all other <tt>std::shared_ptr</tt>s have
		/// been released. Its contents are not actually used.
		std::deque<std::shared_ptr<const font>> _preserved_fonts;
		size_t _max_preserved_fonts = 10; ///< The maximum cardinality of \ref _preserved_fonts.
		manager &_manager; ///< The manager.

		/// Actually loads a font without inserting it into \ref _font_mapping or \ref _preserved_fonts. This function
		/// is platform-dependent.
		std::shared_ptr<const font> _load_font(const font_parameters&);
	};
}
