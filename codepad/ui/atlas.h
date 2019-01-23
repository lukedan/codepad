#pragma once

/// \file
/// Implementation of a texture atlas.

#include <vector>

#include "renderer.h"

namespace codepad::ui {
	/// Merges lots of small textures that are usually used together to reduce the number of draw calls.
	class atlas {
	public:
		/// The type of IDs that uniquely identify each texture.
		using id_t = size_t;

		/// Initializes this \ref atlas with the corresponding \ref renderer_base.
		atlas(renderer_base &r) : _r(r) {
		}
		/// Frees \ref _page_data if necessary.
		~atlas() {
			if (_page_data) {
				std::free(_page_data);
			}
		}

		/// Adds a new texture to this atlas.
		id_t add(size_t w, size_t h, const std::uint8_t *data) {
			if (_pages.empty()) {
				_new_page();
			}
			// allocate slot & id
			id_t id = _cd_slots.size();
			char_data &cd = _cd_slots.emplace_back();
			if (w == 0 || h == 0) { // the character is blank
				cd.uv = rectd(0.0, 0.0, 0.0, 0.0);
				cd.page = _pages.size() - 1;
			} else {
				texture *curp = &_pages.back();
				if (_cur_x + w + 2 * _border > curp->get_width()) {
					// the current row doesn't have enough space; move to next row
					_cur_x = 0;
					_cur_y += _max_height;
					_max_height = 0;
				}
				size_t t, l; // coords of the the new character's top left corner
				if (_cur_y + h + 2 * _border > curp->get_height()) {
					// the current page doesn't have enough space; create new page
					_checked_flush();
					curp = &_new_page();
					_cur_y = 0;
					l = t = _border;
					_max_height = h + 2 * _border;
				} else {
					l = _cur_x + _border;
					t = _cur_y + _border;
					_max_height = std::max(_max_height, h + 2 * _border);
				}
				_cur_x = l + w;
				// copy image data
				auto *src = data;
				for (size_t y = 0; y < h; ++y) {
					std::uint8_t *cur = _page_data + ((y + t) * curp->get_width() + l) * 4;
					for (size_t x = 0; x < w; ++x, src += 4, cur += 4) {
						cur[0] = src[0];
						cur[1] = src[1];
						cur[2] = src[2];
						cur[3] = src[3];
					}
				}
				// calculate UV coordinates
				cd.uv = rectd(
					static_cast<double>(l) / static_cast<double>(curp->get_width()),
					static_cast<double>(l + w) / static_cast<double>(curp->get_width()),
					static_cast<double>(t) / static_cast<double>(curp->get_height()),
					static_cast<double>(t + h) / static_cast<double>(curp->get_height())
				);
				cd.page = _pages.size() - 1;
				_page_dirty = true; // mark the last page as dirty
			}
			return id;
		}

		/// Stores the information about a character in the atlas.
		struct char_data {
			rectd uv; ///< The UV coordinates of the character in the page.
			size_t page = 0; ///< The number of the page that the character is on.
		};

		/// Returns the \ref char_data corresponding to the given ID.
		const char_data &get_data(size_t id) const {
			return _cd_slots[id];
		}
		/// Retrieves a \ref page for rendering. If the requested page is the last page and is dirty, then its
		/// contents will be flushed.
		const texture &get_page(size_t page) {
			if (page + 1 == _pages.size()) {
				_checked_flush();
			}
			return _pages[page];
		}

		/// Sets the width of a page. This takes effect the next time a page is created.
		void set_page_width(size_t w) {
			_page_width = w;
			_size_changed = true;
		}
		/// Sets the height of a page. This takes effect the next time a page is created.
		void set_page_height(size_t h) {
			_page_height = h;
			_size_changed = true;
		}
		/// Sets the width of the border.
		void set_border_width(size_t w) {
			_border = w;
		}
		/// Returns the desired width of a page.
		size_t get_page_width() const {
			return _page_width;
		}
		/// Returns the desired height of a page.
		size_t get_page_height() const {
			return _page_height;
		}
		/// Returns the width of borders.
		size_t get_border_width() const {
			return _border;
		}
	protected:
		std::vector<texture> _pages; ///< The pages.
		std::vector<char_data> _cd_slots; ///< Stores all \ref char_data "char_datas".
		size_t
			_cur_x = 0, ///< The X coordinate of the next character, including its border.
			_cur_y = 0, ///< The Y coordinate of the next character, including its border.
			_max_height = 0, ///< The height of the tallest character of this row, including both of its borders.
			_page_width = 1024, ///< The width of a page.
			_page_height = 1024, ///< The height of a page.
			_border = 1; ///< The number of pixels that's left as a border between textures.
		renderer_base &_r; ///< The renderer.
		std::uint8_t *_page_data = nullptr; ///< The contents of the current unfinished page.
		bool
			_page_dirty = false, ///< Marks whether the last page is dirty.
			/// Indicate if the page size has been changed, which means that next time a page is created \ref _page_data
			/// has to be re-allocated.
			_size_changed = false;

		/// Creates a new page and initializes all its pixels to transparent white.
		///
		/// \return An reference to the new page.
		texture &_new_page() {
			if (_page_data == nullptr || _size_changed) {
				if (_page_data) {
					std::free(_page_data);
				}
				_page_data = static_cast<std::uint8_t*>(std::malloc(_page_width * _page_height * 4));
				_size_changed = false;
			}
			for (size_t y = 0; y < _page_height; ++y) {
				std::uint8_t *cur = _page_data + y * _page_width * 4;
				for (size_t x = 0; x < _page_width; ++x) {
					*(cur++) = 255;
					*(cur++) = 255;
					*(cur++) = 255;
					*(cur++) = 0;
				}
			}
			return _pages.emplace_back(_r.new_texture(_page_width, _page_height));
		}
		/// If \ref _page_dirty is \p true, copies data from \ref _page_data to the last page.
		void _checked_flush() {
			if (_page_dirty) {
				_r.update_texture(_pages.back(), _page_data);
				_page_dirty = false;
			}
		}
	};
}
