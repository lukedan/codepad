#pragma once

#include <mutex>
#include <thread>
#include <functional>
#include <fstream>

#include "../ui/element.h"
#include "../ui/textrenderer.h"

namespace codepad {
	namespace editor {
		// TODO syntax highlighting, line numbers, etc.
		class file_context {
		public:
			file_context(const str_t &fn, size_t buffer_size = 32768) : path(fn) {
				std::list<line> ls;
				std::stringstream ss;
				{
					assert(buffer_size > 1);
					char *buffer = static_cast<char*>(std::malloc(sizeof(char) * buffer_size));
					std::ifstream fin(convert_to_utf8(path), std::ios::binary);
					while (fin) {
						fin.read(buffer, buffer_size - 1);
						buffer[fin.gcount()] = '\0';
						ss << buffer;
					}
					std::free(buffer);
				}
				str_t fullcontent = convert_from_utf8<char_t>(ss.str());
				std::basic_ostringstream<char_t> nss;
				char_t last = U'\0';
				blocks.push_back(block());
				for (auto i = fullcontent.begin(); i != fullcontent.end(); ++i) {
					if (last == U'\r') {
						bool addbef = (*i == U'\n');
						if (addbef) {
							nss << *i;
						}
						init_append_line(nss.str());
						nss = std::basic_ostringstream<char_t>();
						if (!addbef) {
							nss << *i;
						}
					} else {
						nss << *i;
						if (*i == U'\n') {
							init_append_line(nss.str());
							nss = std::basic_ostringstream<char_t>();
						}
					}
					last = *i;
				}
				init_append_line(nss.str());
				if (last == U'\r') {
					init_append_line(str_t());
				}
			}

			struct line {
				line() = default;
				line(const str_t &c) : content(c) {
				}
				str_t content;
			};
			struct block {
				constexpr static size_t advised_lines = 200;
				std::list<line> lines;
			};

			struct line_iterator {
				friend class file_context;
			public:
				line_iterator(const std::list<block>::iterator bk, const std::list<line>::iterator l) : _block(bk), _line(l) {
				}

				line &operator*() const {
					return *_line;
				}
				line *operator->() const {
					return &*_line;
				}

				line_iterator &operator++() {
					if ((++_line) == _block->lines.end()) {
						++_block;
						_line = _block->lines.begin();
					}
					return *this;
				}
				line_iterator operator++(int) {
					line_iterator tmp = *this;
					++*this;
					return tmp;
				}
				line_iterator &operator--() {
					if (_line == _block->lines.begin()) {
						--_block;
						_line = _block->lines.end();
					}
					--_line;
					return *this;
				}
				line_iterator operator--(int) {
					line_iterator tmp = *this;
					--*this;
					return tmp;
				}

				friend bool operator==(const line_iterator &l1, const line_iterator &l2) {
					return l1._block == l2._block && l1._line == l2._line;
				}
				friend bool operator!=(const line_iterator &l1, const line_iterator &l2) {
					return !(l1 == l2);
				}
			protected:
				std::list<block>::iterator _block;
				std::list<line>::iterator _line;
			};

			line_iterator at(size_t v) {
				for (auto i = blocks.begin(); i != blocks.end(); ++i) {
					if (i->lines.size() < v) {
						auto j = i->lines.begin();
						for (size_t k = 0; k < v; ++k, ++j) {
						}
						return line_iterator(i, j);
					} else {
						v -= i->lines.size();
					}
				}
			}
			line_iterator begin() {
				return line_iterator(blocks.begin(), blocks.begin()->lines.begin());
			}
			line_iterator before_end() {
				auto lastblk = blocks.end();
				--lastblk;
				auto lastln = lastblk->lines.end();
				return line_iterator(lastblk, --lastln);
			}

			size_t get_line_number() const {
				size_t res = 0;
				for (auto i = blocks.begin(); i != blocks.end(); ++i) {
					res += i->lines.size();
				}
				return res;
			}

			const str_t path;
			std::list<block> blocks;
			std::mutex modify_mutex;

			void init_append_line(const str_t &s) {
				if (blocks.back().lines.size() == block::advised_lines) {
					blocks.push_back(block());
				}
				blocks.back().lines.push_back(line(s));
			}
		};
		class codebox : public ui::element {
		public:
			file_context *context;
			font_family font;

			void set_text_position(double v) {
				_pos = clamp(v, 0.0, (context->get_line_number() - 1) * font.maximum_height());
				invalidate_visual();
			}
			double get_text_position() {
				return _pos;
			}

			ui::cursor get_default_cursor() const override {
				return ui::cursor::text_beam;
			}
		protected:
			double _pos = 0.0;

			void _on_mouse_scroll(ui::mouse_scroll_info &info) override {
				double oldp = _pos;
				set_text_position(_pos - info.delta * 3.0 * font.maximum_height()); // TODO magic values
				if (std::abs(oldp - _pos) > 1.0) {
					info.mark_handled();
				}
			}

			void _render() const override {
				if (_layout.height() < 0.0) {
					return;
				}
				double lh = font.maximum_height();
				size_t
					line_beg = static_cast<size_t>(_pos / lh),
					line_end = static_cast<size_t>((_pos + _layout.height()) / lh) + 1;
				auto it = context->begin();
				size_t i = 0;
				for (; i < line_beg && it != context->before_end(); ++i, ++it) {
				}
				if (i == line_beg) {
					double cury = _layout.ymin - _pos + line_beg * lh;
					for (; i <= line_end; ++i, ++it, cury += lh) {
						ui::text_renderer::render_plain_text(it->content, *font.normal, vec2d(_layout.xmin, cury), colord());
						if (it == context->before_end()) {
							break;
						}
					}
				}
			}
		};
	}
}
