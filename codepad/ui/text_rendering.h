#pragma once

/// \file
/// Classes that manages fonts, creates text atlases, and measures and renders text.

#include "font.h"
#include "font_family.h"
#include "draw.h"

namespace codepad::ui {
	class manager;

	/// Manages a list of font names and fonts, and creates a texture atlas for all characters.
	class font_manager {
	public:
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
	protected:
		manager &_manager; ///< The manager.
		std::map<std::string, std::shared_ptr<font>> _font_mapping; ///< The mapping between font names and fonts.
		atlas _atlas; ///< The texture atlas.
	};


	/// Struct for measuring and calculating text layout for a single line. For gizmos, the character code is set to
	/// zero, and no kerning is taken into consideration.
	struct character_metrics_accumulator {
	public:
		/// Constructs the struct, given a \ref font_family and a tab size.
		///
		/// \param ff The \ref font_family used for all the calculations.
		/// \param tabsize Specifies how many whitespaces the width of a tab is.
		character_metrics_accumulator(ui::font_family ff, double tabsize) : _font(std::move(ff)) {
			set_tab_width(tabsize);
		}

		/// Appends a character to the end of the line, with the given \ref font_style.
		void next_char(codepoint c, font_style fs) {
			const font &fnt = *_font.get_by_style(fs);
			double kerning = 0.0;
			if (_last_char != 0 && _last_style == fs) {
				kerning = fnt.get_kerning(_last_char, c).x;
			}
			auto *entry = &fnt.get_char_entry(c);
			_next_impl(c, fs, kerning, entry->advance, entry);
		}
		/// Appends a gizmo to the end of the line.
		void next_gizmo(double width) {
			_next_impl(0, font_style::normal, 0.0, width, nullptr);
		}

		/// Returns the position of the left boundary of the current character or gizmo.
		double char_left() const {
			return _cur_left;
		}
		/// Returns the position of the right boundary of the current character or gizmo.
		double char_right() const {
			return _cur_left + _cur_width;
		}
		/// Returns the width of the current character or gizmo.
		double char_width() const {
			return _cur_width;
		}
		/// Returns the position of the right boundary of the previous character or gizmo.
		double prev_char_right() const {
			return _last_right;
		}
		/// Returns the current character. The return value is 0 if the current token is a gizmo.
		codepoint current_char() const {
			return _cur_char;
		}
		/// Returns the \ref font::entry for the current character. This is valid only if \ref current_char() returns
		/// a non-zero value.
		const font::entry &current_char_entry() const {
			assert_true_logical(_cur_entry != nullptr, "the current token is a gizmo");
			return *_cur_entry;
		}

		/// Returns the current font family in use.
		const font_family &get_font_family() const {
			return _font;
		}

		/// Sets the width of the tab, in whitespaces.
		void set_tab_width(double tw) {
			_tab_width = tw * _font.normal->get_char_entry(' ').advance;
		}

		/// Resets the struct to the initial state.
		void reset() {
			_last_style = _cur_style = font_style::normal;
			_last_right = _cur_width = _cur_left = 0.0;
			_last_char = _cur_char = 0;
			_cur_entry = nullptr;
		}
	protected:
		font_style
			_last_style = font_style::normal, ///< The font style of the last char.
			_cur_style = font_style::normal; ///< The font style of the current char.
		font_family _font; ///< The font family used for all the calculations.
		double
			_last_right = 0.0, ///< The position of the right boundary of the previous character.
			_cur_width = 0.0, ///< The width of the current character.
			_cur_left = 0.0, ///< The position of the left boundary of the current character.
			_tab_width = 0.0; ///< The width of a tab character, in pixels.
		codepoint
			_last_char = 0, ///< The last character.
			_cur_char = 0; ///< The current character.
		const font::entry *_cur_entry = nullptr; ///< The entry for the current character.

		/// Adds the given metrics to the end of the line. This function supports both normal characters and gizmos.
		void _next_impl(codepoint cp, font_style style, double kerning, double width, const font::entry *entry) {
			_last_style = _cur_style;
			_last_char = _cur_char;
			_cur_style = style;
			_cur_char = cp;
			_cur_entry = entry;

			_last_right = _cur_left + _cur_width;
			_cur_left = _last_right + kerning;
			_cur_width = width;
		}

		/// Returns the width of the tab character, in pixels, considering its position.
		double _get_target_tab_width() const {
			return _tab_width * (std::floor(_cur_left / _tab_width) + 1.0) - _cur_left;
		}
	};

	/// Constains functions for rendering text.
	namespace text_renderer {
		/// Renders the given text, using the specified font, position, and color.
		///
		/// \return The size of the rendered text.
		template <typename It> inline vec2d render_plain_text(
			It beg, It end, const font &fnt, vec2d topleft, colord color
		) {
			double dy = std::ceil(fnt.height());
			vec2d cur(topleft.x, static_cast<int>(std::round(topleft.y)));
			codepoint last = 0;
			vec2d size(0.0, dy);
			atlas::batch_renderer rend(fnt.get_manager().get_atlas());
			while (beg != end) {
				codepoint cp;
				if (!encodings::utf8::next_codepoint(beg, end, cp)) {
					cp = encodings::replacement_character;
				}
				if (is_newline(cp)) {
					size.x = std::max(size.x, cur.x - topleft.x);
					size.y += dy;
					cur.x = topleft.x;
					cur.y += dy;
					last = 0;
				} else {
					if (last != 0) {
						cur.x += fnt.get_kerning(last, cp).x;
					}
					font::character_rendering_info info = fnt.draw_character(cp, cur);
					rend.add_sprite(info.texture, info.placement, color);
					cur.x += info.entry.advance;
					last = cp;
				}
			}
			size.x = std::max(size.x, cur.x - topleft.x);
			return size;
		}
		/// \overload
		template <typename Cont> inline vec2d render_plain_text(
			Cont &&c, const font &fnt, vec2d topleft, colord color
		) {
			return render_plain_text(c.begin(), c.end(), fnt, topleft, color);
		}

		/// Measures the size of the bounding box of the text, using the given font.
		template <typename It> inline vec2d measure_plain_text(
			It beg, It end, const std::shared_ptr<const font> &fnt
		) {
			codepoint last = 0;
			double curline = 0.0, maxw = 0.0;
			size_t linen = 1;
			while (beg != end) {
				codepoint cp;
				if (!encodings::utf8::next_codepoint(beg, end, cp)) {
					cp = encodings::replacement_character;
				}
				if (is_newline(cp)) {
					++linen;
					last = 0;
					// TODO should the size of a whitespace be appended?
					maxw = std::max(maxw, curline);
					curline = 0.0;
				} else {
					const font::entry &et = fnt->get_char_entry(cp);
					if (last != 0) {
						curline += fnt->get_kerning(last, cp).x;
					}
					curline += et.advance;
					last = cp;
				}
			}
			maxw = std::max(maxw, curline);
			return vec2d(maxw, static_cast<double>(linen) * std::ceil(fnt->height()));
		}
		/// \overload
		template <typename Cont> inline vec2d measure_plain_text(
			Cont &&cont, const std::shared_ptr<const font> &fnt
		) {
			return measure_plain_text(cont.begin(), cont.end(), fnt);
		}
	}
}
