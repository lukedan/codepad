#pragma once

/// \file
/// Implementation of platform-independent functions of the software renderer.

#include <cstdlib>
#include <vector>

#ifdef CP_USE_SSE2
#   include <xmmintrin.h> // SSE
#   include <emmintrin.h> // SSE2
#endif

#include "renderer.h"

namespace codepad::os {
	/// Base class of software renderers. This class implements basic routines for software rendering,
	/// and derived classes should implement platform-specific routines for displaying the rendering result.
	class software_renderer_base : public renderer_base {
	public:
		/// Adds the default texture into the texture registry.
		software_renderer_base() : renderer_base() {
			_txs.emplace_back();
		}

		/// Simply renderers the character as a quad.
		void draw_character_custom(const char_texture &tex, rectd r, colord c) override {
			rectd t(0.0, 1.0, 0.0, 1.0);
			vec2d vs[6]{
				r.xmin_ymin(), r.xmax_ymin(), r.xmin_ymax(),
				r.xmax_ymin(), r.xmax_ymax(), r.xmin_ymax()
			}, uvs[6]{
				t.xmin_ymin(), t.xmax_ymin(), t.xmin_ymax(),
				t.xmax_ymin(), t.xmax_ymax(), t.xmin_ymax()
			};
			colord cs[6] = {c, c, c, c, c, c};
			_draw_triangles_impl(_get_id(tex), vs, uvs, cs, 6);
		}
		/// Calls \ref _draw_triangles_impl to render a series of triangles.
		void draw_triangles(
			const texture &tid, const vec2d *poss, const vec2d *uvs, const colord *cs, size_t sz
		) override {
			_draw_triangles_impl(_get_id(tid), poss, uvs, cs, sz);
		}
		/// Calls \ref _draw_line to draw a series of lines.
		///
		/// \todo The lines are currently rendered only with the color of the first vertex.
		void draw_lines(const vec2d *vs, const colord *cs, size_t sz) override {
			for (size_t i = 0; i < sz; i += 2) {
				_draw_line(
					_matstk.back().transform(vs[i]).convert<_real_t>(),
					_matstk.back().transform(vs[i + 1]).convert<_real_t>(),
					cs[i].convert<_color_t::value_type>()
				);
			}
		}
		/// Calls _render_target_stackframe::end, clears the stacks of information about the
		/// current render target, and continues the last render target if there is one.
		void end() override {
			if (_rtfstk.back().end) {
				_rtfstk.back().end();
			}
			_rtfstk.pop_back();
			_matstk.pop_back();
			_blendstk.pop_back();
			_clipstk.pop_back();
			if (_rtfstk.empty()) {
				assert_true_usage(_matstk.empty(), "pushmatrix/popmatrix mismatch");
				assert_true_usage(_blendstk.empty(), "pushblendfunc/popblendfunc mismatch");
			}
		}

		/// Allocates space for the new texture, copies the data,
		/// and returns a \ref texture with the allocated ID.
		texture new_texture(size_t w, size_t h, const void *rgba) override {
			texture::id_t nid = _alloc_id();
			_txs[nid].set_rgba(w, h, static_cast<const unsigned char*>(rgba));
			return _make_texture(nid, w, h);
		}
		/// Calls \ref _delete_texture_impl to delete the given texture.
		void delete_texture(texture &id) override {
			_delete_texture_impl(id);
		}

		/// Allocates space for the new character texture, copies the data,
		/// and returns a \ref char_texture with the allocated ID.
		/// This is the same as for normal textures.
		char_texture new_character_texture(size_t w, size_t h, const void *gs) override {
			char_texture::id_t nid = _alloc_id();
			_txs[nid].set_rgba(w, h, static_cast<const unsigned char*>(gs));
			return _make_texture<false>(nid, w, h);
		}
		/// Calls \ref _delete_texture_impl to delete the given character texture.
		void delete_character_texture(char_texture &id) override {
			_delete_texture_impl(id);
		}
		/// Pushes a clip onto _render_target_stackframe::clip_stack.
		void push_clip(recti r) override {
			r = recti::common_part(r, _clipstk.back());
			r.make_valid_max();
			_clipstk.push_back(r);
		}
		/// Pops a clip.
		void pop_clip() override {
			_clipstk.pop_back();
		}

		/// Allocates a texture of the given size, and returns the corresponding \ref framebuffer.
		framebuffer new_framebuffer(size_t w, size_t h) override {
			texture::id_t nid = _alloc_id();
			_txs[nid].resize(w, h);
			return _make_framebuffer(nid, _make_texture(nid, w, h));
		}
		/// Deletes the texture corresponding to the frame buffer, and erases the frame buffer.
		void delete_framebuffer(framebuffer &fb) override {
			_delete_texture_by_id(_get_id(fb.get_texture()));
			_erase_framebuffer(fb);
		}
		/// Calls \ref continue_framebuffer and then clears the current buffer.
		void begin_framebuffer(const framebuffer &fb) override {
			continue_framebuffer(fb);
			_clear_texture(_rtfstk.back().buffer, _rtfstk.back().width, _rtfstk.back().height);
		}
		/// Calls \ref _begin_render_target to start rendering to the frame buffer.
		void continue_framebuffer(const framebuffer &fb) override {
			_tex_rec &tr = _txs[_get_id(fb.get_texture())];
			_begin_render_target(_render_target_stackframe(tr.w, tr.h, tr.data));
		}

		/// Pushes a \ref matd3x3 onto \ref _matstk;
		void push_matrix(const matd3x3 &m) override {
			_matstk.push_back(m);
		}
		/// Multiplies the \ref matd3x3 with the one on the top of \ref _matstk and pushes it onto the stack.
		void push_matrix_mult(const matd3x3 &m) override {
			_matstk.push_back(_matstk.back() * m);
		}
		/// Obtains the \ref matd3x3 on the top of \ref _matstk.
		matd3x3 top_matrix() const override {
			return _matstk.back();
		}
		/// Pops a \ref matd3x3 from \ref _matstk.
		void pop_matrix() override {
			_matstk.pop_back();
		}

		/// Pushes a \ref blend_function onto \ref _blendstk.
		void push_blend_function(blend_function f) override {
			_blendstk.push_back(f);
		}
		/// Pops a \ref blend_function from \ref _blendstk.
		void pop_blend_function() override {
			_blendstk.pop_back();
		}
		/// Obtains the \ref blend_function on the top of \ref _blendstk.
		blend_function top_blend_function() const override {
			return _blendstk.back();
		}
	protected:
		using _real_t = float; ///< The floating-point type used for rendering operations.
		using _color_t = color<_real_t>; ///< The type of the underlying pixels.
		using _vec2_t = vec2<_real_t>; ///< The vector type used for calculations.

		struct _ivec4i32;
		/// 4-d vector of floats, accelerated using SSE intrinsics if enabled.
		struct alignas(16) _ivec4f {
			friend struct _ivec4i32;

			/// Default constructor.
			_ivec4f() = default;

			/// Sets the values of the four components of the vector. The given address must be 16-byte-aligned.
			void set_all_aligned(const float*);
			/// Sets the values of the four components of the vector. The given address need not be 16-byte-aligned.
			void set_all(const float*);
			/// \overload
			void set_all(float, float, float, float);
			/// Sets all four components to the given value.
			void set_uniform(float);

			/// Gets the values of the four components of the vector. The given address must be 16-byte-aligned.
			void get_all_aligned(float*) const;
			/// Gets the values of the four components of the vector. The given address need not be 16-byte-aligned.
			void get_all(float*) const;
			/// Gets the first component of the four components.
			float get_x() const;

			/// Addition.
			static _ivec4f add(_ivec4f, _ivec4f);
			/// Addition.
			friend _ivec4f operator+(_ivec4f lhs, _ivec4f rhs) {
				return add(lhs, rhs);
			}
			/// Addition.
			_ivec4f &operator+=(_ivec4f rhs) {
				return *this = add(*this, rhs);
			}

			/// Subtraction.
			static _ivec4f subtract(_ivec4f, _ivec4f);
			/// Subtraction.
			friend _ivec4f operator-(_ivec4f lhs, _ivec4f rhs) {
				return subtract(lhs, rhs);
			}
			/// Subtraction.
			_ivec4f &operator-=(_ivec4f rhs) {
				return *this = subtract(*this, rhs);
			}

			/// Element-wise multiplication.
			static _ivec4f multiply_elem(_ivec4f, _ivec4f);
			/// Element-wise multiplication.
			friend _ivec4f operator*(_ivec4f lhs, _ivec4f rhs) {
				return multiply_elem(lhs, rhs);
			}
			/// Element-wise multiplication.
			_ivec4f &operator*=(_ivec4f rhs) {
				return *this = multiply_elem(*this, rhs);
			}

			/// Scalar multiplication.
			inline static _ivec4f multiply(_ivec4f lhs, float rhs) {
				_ivec4f v;
				v.set_uniform(rhs);
				return multiply_elem(lhs, v);
			}
			/// Scalar multiplication.
			friend _ivec4f operator*(float lhs, _ivec4f rhs) {
				return multiply(rhs, lhs);
			}
			/// Scalar multiplication.
			friend _ivec4f operator*(_ivec4f lhs, float rhs) {
				return multiply(lhs, rhs);
			}
			/// Scalar multiplication.
			_ivec4f &operator*=(float rhs) {
				return *this = multiply(*this, rhs);
			}

			/// Element-wise division.
			static _ivec4f divide_elem(_ivec4f, _ivec4f);
			/// Element-wise division.
			friend _ivec4f operator/(_ivec4f lhs, _ivec4f rhs) {
				return divide_elem(lhs, rhs);
			}
			/// Element-wise division.
			_ivec4f &operator/=(_ivec4f rhs) {
				return *this = divide_elem(*this, rhs);
			}

			/// Scalar division.
			inline static _ivec4f divide(_ivec4f lhs, float rhs) {
				_ivec4f v;
				v.set_uniform(rhs);
				return divide_elem(lhs, v);
			}
			/// Scalar division.
			friend _ivec4f operator/(_ivec4f lhs, float rhs) {
				return divide(lhs, rhs);
			}
			/// Scalar division.
			_ivec4f &operator/=(float rhs) {
				return *this = divide(*this, rhs);
			}

			/// Convert the vector into a \ref _ivec4i32 with truncation.
			_ivec4i32 convert_to_int_truncate() const;

			/// Returns a shuffled version of this vector.
			template <int I1, int I2, int I3, int I4> _ivec4f shuffle() const {
#ifdef CP_USE_SSE2
				return _ivec4f(_mm_shuffle_ps(xyzw, xyzw, _MM_SHUFFLE(I4, I3, I2, I1)));
#else
				return _ivec4f((&x)[I1], (&x)[I2], (&x)[I3], (&x)[I4]);
#endif
			};

		protected:
#ifdef CP_USE_SSE2
			/// Initializes the vector from a \p __m128.
			explicit _ivec4f(__m128 v) : xyzw(v) {
			}

			__m128 xyzw; ///< Stores the four components of the vector.
#else
			/// Initializes the vector.
			_ivec4f(float xv, float yv, float zv, float wv) : x(xv), y(yv), z(zv), w(wv) {
			}

			float
				x, ///< The X component.
				y, ///< The Y component.
				z, ///< The Z component.
				w; ///< The W component.
#endif
		};
		/// 4-d vector of ints, accelerated using SSE intrinsics if enabled.
		struct alignas(16) _ivec4i32 {
			friend struct _ivec4f;

			/// Returns the X component of the vector.
			int get_x() const;
			/// Packs all four elements, which are assumed to be in range [0, 255], into a single \p int.
			int pack() const;

			/// Converts all four components to float and returns the result.
			_ivec4f convert_to_float() const;
		protected:
#ifdef CP_USE_SSE2
			/// Initializes the vector from a \p __m128i.
			explicit _ivec4i32(__m128i v) : xyzw(v) {
			}

			__m128i xyzw; ///< Stores the four components of the vector.
#else
			/// Initializes the vector.
			_ivec4i32(int xv, int yv, int zv, int wv) : x(xv), y(yv), z(zv), w(wv) {
			}

			int
				x, ///< The X component.
				y, ///< The Y component.
				z, ///< The Z component.
				w; ///< The W component.
#endif
		};


		/// Converts a \ref _color_t to a \ref _ivec4f.
		inline static _ivec4f _convert_from_color(_color_t v) {
			_ivec4f res;
			res.set_all(v.a, v.r, v.g, v.b);
			return res;
		}
		/// Converts a \ref _ivec4f to a \ref colorf.
		inline static colorf _convert_to_colorf(_ivec4f v) {
			float argb[4];
			v.get_all(argb);
			return colorf(argb[1], argb[2], argb[3], argb[0]);
		}

		/// Sets every pixel in the given to transparent black.
		inline static void _clear_texture(_ivec4f *arr, size_t w, size_t h) {
			for (size_t i = w * h; i > 0; --i, ++arr) {
				arr->set_uniform(0.0f);
			}
		}

		/// Stores a texture. There is no extra padding between rows of the texture.
		struct _tex_rec {
			/// Default constructor.
			_tex_rec() = default;
			/// Move constructor.
			_tex_rec(_tex_rec &&src) : w(src.w), h(src.h), data(src.data) {
				src.w = src.h = 0;
				src.data = nullptr;
			}
			/// No copy construction.
			_tex_rec(const _tex_rec&) = delete;
			/// Move assignment.
			_tex_rec &operator=(_tex_rec &&src) {
				std::swap(w, src.w);
				std::swap(h, src.h);
				std::swap(data, src.data);
				return *this;
			}
			/// No copy assignment.
			_tex_rec &operator=(const _tex_rec&) = delete;
			/// Destructor. Frees \ref data if it's non-empty.
			~_tex_rec() {
				if (data) {
					dispose();
				}
			}

			/// Resizes the texture, freeing previously allocated memory if there's any.
			void resize(size_t ww, size_t hh) {
				if (data) {
					dispose();
				}
				w = ww;
				h = hh;
				if (w > 0 && h > 0) {
					data = new _ivec4f[w * h];
				}
			}
			/// Sets the content of the texture, freeing previously allocated memory if there's any.
			///
			/// \param ww The width of the texture.
			/// \param hh The height of the texture.
			/// \param rgba The pixel data, in 8-bit RGBA format.
			/// \todo Optimization.
			void set_rgba(size_t ww, size_t hh, const unsigned char *rgba) {
				resize(ww, hh);
				_ivec4f *target = data;
				float fv[4];
				for (size_t i = w * h; i > 0; --i, ++target, rgba += 4) {
					fv[1] = rgba[0] / 255.0f;
					fv[2] = rgba[1] / 255.0f;
					fv[3] = rgba[2] / 255.0f;
					fv[0] = rgba[3] / 255.0f;
					target->set_all(fv);
				}
			}
			/// Frees the allocated pixel data and resets the texture to empty. Must not be called on
			/// an empty \ref _tex_rec. The memory is not automatically freed so this function must
			/// be called before the object itself is disposed.
			void dispose() {
				delete[] data;
				data = nullptr;
			}

			/// Retrieves the pixel data at the given position.
			_ivec4f fetch(size_t x, size_t y) const {
				return data[y * w + x];
			}

			/// Returns the corresponding color given the texture coordinates. (0, 0) stands for the
			/// top left corner, while (1, 1) stands for the bottom right corner. The coordinates are
			/// wrapped around the borders, and bilinear interpolation is used to obtain the final result.
			///
			/// \todo Acceleration using intrinsics.
			_ivec4f sample(_vec2_t uv) const {
				_real_t xf = uv.x * static_cast<_real_t>(w) - 0.5f, yf = uv.y * static_cast<_real_t>(h) - 0.5f;
				int x = static_cast<int>(std::floor(xf)), y = static_cast<int>(std::floor(yf)), x1 = x + 1, y1 = y + 1;
				xf -= static_cast<_real_t>(x);
				yf -= static_cast<_real_t>(y);
				x %= static_cast<int>(w);
				x1 %= static_cast<int>(w);
				if (x < 0) {
					x += static_cast<int>(w);
					if (x1 < 0) {
						x1 += static_cast<int>(w);
					}
				}
				y %= static_cast<int>(h);
				y1 %= static_cast<int>(h);
				if (y < 0) {
					y += static_cast<int>(h);
					if (y1 < 0) {
						y1 += static_cast<int>(h);
					}
				}
				_ivec4f v[4] = {
					fetch(static_cast<size_t>(x), static_cast<size_t>(y)),
					fetch(static_cast<size_t>(x1), static_cast<size_t>(y)),
					fetch(static_cast<size_t>(x), static_cast<size_t>(y1)),
					fetch(static_cast<size_t>(x1), static_cast<size_t>(y1))
				};
				return v[0] + (v[1] - v[0]) * (1.0f - yf) * xf + (v[2] - v[0] + (v[3] - v[2]) * xf) * yf;
			}

			size_t
				w = 0, ///< The width of the texture.
				h = 0; ///< The height of the texture.
			_ivec4f *data = nullptr; ///< The pixel data in ARGB format.
		};
		/// Stores the information of a render target that's being rendered to.
		struct _render_target_stackframe {
			/// Constructs the stack frame with the given size and buffer,
			/// but without a function to call after it's finished.
			_render_target_stackframe(size_t w, size_t h, _ivec4f *buf) :
				_render_target_stackframe(w, h, buf, nullptr) {
			}
			/// Constructs the stack frame with the given size, buffer,
			/// and function to call after the rendering's finished.
			template <typename T> _render_target_stackframe(size_t w, size_t h, _ivec4f *buf, T &&e) :
				width(w), height(h), buffer(buf), end(std::forward<T>(e)) {
			}

			size_t
				width = 0, ///< The width of the render target.
				height = 0; ///< The height of the render target.
			_ivec4f *buffer = nullptr; ///< The buffer to render to.
			std::function<void()> end; ///< The function to be called after the rendering's finished.
		};

		std::vector<_tex_rec> _txs; ///< The list of textures.
		std::vector<texture::id_t> _id_realloc; ///< Stores the indices of deallocated textures.
		std::vector<_render_target_stackframe> _rtfstk; ///< The stack of render target stackframes.
		std::vector<matd3x3> _matstk; ///< The stack of matrices shared by all render targets.
		std::vector<blend_function> _blendstk; ///< The stack of blend functions shared by all render targets.
		std::vector<recti> _clipstk; ///< The stack of clip regions shared by all render targets.

		/// Pushes the given \ref _render_target_stackframe onto \ref _rtfstk and starts rendering to it.
		void _begin_render_target(_render_target_stackframe sf) {
			_matstk.emplace_back();
			_matstk.back().set_identity();
			_blendstk.emplace_back();
			_clipstk.emplace_back(0, static_cast<int>(sf.width), 0, static_cast<int>(sf.height));
			_rtfstk.emplace_back(std::move(sf));
		}

		/// Allocates and returns an ID for a new texture.
		texture::id_t _alloc_id() {
			texture::id_t nid;
			if (_id_realloc.empty()) {
				nid = _txs.size();
				_txs.emplace_back();
			} else {
				nid = _id_realloc.back();
				_id_realloc.pop_back();
			}
			return nid;
		}
		/// Disposes the texture with the given ID, and pushes the ID onto \ref _id_realloc.
		void _delete_texture_by_id(texture::id_t id) {
			_txs[id].dispose();
			_id_realloc.push_back(id);
		}
		/// Calls _delete_texture_by_id() to dispose of the texture, and then erases the \ref texture_base.
		template <bool V> void _delete_texture_impl(texture_base<V> &tex) {
			_delete_texture_by_id(_get_id(tex));
			_erase_texture(tex);
		}
		/// Draws a batch of triangles with the given texture and vertices.
		void _draw_triangles_impl(
			texture::id_t tid, const vec2d *poss, const vec2d *uvs, const colord *cs, size_t sz
		) {
			const vec2d *cp = poss, *cuv = uvs;
			const colord *cc = cs;
			vec2d tmp[3];
			const blend_function &bfun = _blendstk.back();
			for (; sz > 2; sz -= 3, cp += 3, cuv += 3, cc += 3) {
				tmp[0] = _matstk.back().transform(cp[0]);
				tmp[1] = _matstk.back().transform(cp[1]);
				tmp[2] = _matstk.back().transform(cp[2]);
				_draw_triangle(tmp, cuv, cc, tid, bfun.source_factor, bfun.destination_factor);
			}
		}

		/// Returns the color multiplied by the given \ref blend_factor.
		///
		/// \param src The `source' color.
		/// \param dst The `destination' color.
		/// \param target The color to be multiplied, usually either \p src or \p dst.
		/// \param factor The blend factor to multiply \p target with.
		static _ivec4f _get_blend_diff(_ivec4f src, _ivec4f dst, _ivec4f target, blend_factor factor);
		/// Blends the two colors together, and returns the result.
		///
		/// \param src The `source' color.
		/// \param dst The `destination' color.
		/// \param srcf The factor to blend \p src with.
		/// \param dstf The factor to blend \p dst with.
		static _ivec4f _blend_colors(_ivec4f src, _ivec4f dst, blend_factor srcf, blend_factor dstf);
		/// Blends \p src with \p pix with the given blend factors.
		inline static void _draw_pixel_with_blend(_ivec4f &pix, _ivec4f src, blend_factor srcf, blend_factor dstf) {
			pix = _blend_colors(src, pix, srcf, dstf);
		}

		/// Stores information used to quickly calculate the color and texture coordinates
		/// at a certain position of a triangle.
		struct _pq_params {
			/// Calculates the information based on the three vertices of the triangle.
			_pq_params(_vec2_t p1, _vec2_t p2, _vec2_t p3) {
				_vec2_t v12 = p2 - p1, v23 = p3 - p2, v31 = p1 - p3;
				_real_t div_c = 1.0f / (p1.y * v23.x + p2.y * v31.x + p3.y * v12.x);
				xpi = -v23.y * div_c;
				m12c = v23.x * div_c;
				xqi = -v31.y * div_c;
				m22c = v31.x * div_c;
				vxc = 0.5f - p3.x;
				vyc = 0.5f - p3.y;
				xri = -xpi - xqi;
			}

			/// \todo Document these parameters.
			_real_t xpi, m12c, xqi, m22c, vxc, vyc, xri;
			/// Obtains the values used to interpolate the colors and UVs of the three vertices.
			/// The value of the third vertex should be multiplied by <tt>1 - p - q</tt>
			///
			/// \param x The horizontal position of the pixel.
			/// \param y The vertical position of the pixel.
			/// \param p The parameter to multiply the value of the first vertex with.
			/// \param q The parameter to multiply the value of the second vertex with.
			void get_pq(size_t x, size_t y, _real_t &p, _real_t &q) const {
				_real_t vx = vxc + static_cast<_real_t>(x), vy = vyc + static_cast<_real_t>(y);
				p = xpi * vx + m12c * vy;
				q = xqi * vx + m22c * vy;
			}
		};

		/// Draws a triangle with the given parameters.
		///
		/// \param ps The positions of the vertices.
		/// \param uvs The UV coordinates of the vertices.
		/// \param cs The colors of the vertices.
		/// \param tex The texture used to render the triangle.
		/// \param srcf The factor to blend the colors to be drawn with.
		/// \param dstf The factor to blend the colors already on the canvas with.
		void _draw_triangle(
			const vec2d *ps, const vec2d *uvs, const colord *cs, texture::id_t tex,
			blend_factor srcf, blend_factor dstf
		) {
			_vec2_t rps[3]{ps[0].convert<_real_t>(), ps[1].convert<_real_t>(), ps[2].convert<_real_t>()};
			_vec2_t ruvs[3]{uvs[0].convert<_real_t>(), uvs[1].convert<_real_t>(), uvs[2].convert<_real_t>()};
			_ivec4f rcs[3]{
				_convert_from_color(cs[0].convert<float>()),
				_convert_from_color(cs[1].convert<float>()),
				_convert_from_color(cs[2].convert<float>())
			};
			const _vec2_t *yi[3]{rps, rps + 1, rps + 2};
			if (yi[0]->y > yi[1]->y) {
				std::swap(yi[0], yi[1]);
			}
			if (yi[1]->y > yi[2]->y) {
				std::swap(yi[1], yi[2]);
			}
			if (yi[0]->y > yi[1]->y) {
				std::swap(yi[0], yi[1]);
			}
			const _tex_rec &tr = _txs[tex];
			_real_t
				invk_01 = (yi[1]->x - yi[0]->x) / (yi[1]->y - yi[0]->y),
				invk_02 = (yi[2]->x - yi[0]->x) / (yi[2]->y - yi[0]->y),
				invk_12 = (yi[2]->x - yi[1]->x) / (yi[2]->y - yi[1]->y);
			_pq_params pq(rps[0], rps[1], rps[2]);
			if (invk_01 > invk_02) {
				_draw_triangle_half(
					yi[0]->x, yi[0]->y, invk_02, invk_01, yi[0]->y, yi[1]->y, tr, pq,
					ruvs[0], ruvs[1], ruvs[2], rcs[0], rcs[1], rcs[2], srcf, dstf
				);
			} else {
				_draw_triangle_half(
					yi[0]->x, yi[0]->y, invk_01, invk_02, yi[0]->y, yi[1]->y, tr, pq,
					ruvs[0], ruvs[1], ruvs[2], rcs[0], rcs[1], rcs[2], srcf, dstf
				);
			}
			if (invk_02 > invk_12) {
				_draw_triangle_half(
					yi[2]->x, yi[2]->y, invk_02, invk_12, yi[1]->y, yi[2]->y, tr, pq,
					ruvs[0], ruvs[1], ruvs[2], rcs[0], rcs[1], rcs[2], srcf, dstf
				);
			} else {
				_draw_triangle_half(
					yi[2]->x, yi[2]->y, invk_12, invk_02, yi[1]->y, yi[2]->y, tr, pq,
					ruvs[0], ruvs[1], ruvs[2], rcs[0], rcs[1], rcs[2], srcf, dstf
				);
			}
		}
		/// Draws the upper or lower sub-triangle of a triangle.
		///
		/// \todo Document all parameters of this function.
		void _draw_triangle_half(
			_real_t sx, _real_t sy, _real_t invk1, _real_t invk2, _real_t ymin, _real_t ymax,
			const _tex_rec &tex, _pq_params params, _vec2_t uv1, _vec2_t uv2, _vec2_t uv3,
			_ivec4f c1, _ivec4f c2, _ivec4f c3, blend_factor srcf, blend_factor dstf
		) {
			sx += 0.5f;
			sy -= 0.5f;
			recti crgn = _clipstk.back();
			_ivec4f *tarbuf = _rtfstk.back().buffer;
			size_t
				miny = static_cast<size_t>(std::max<_real_t>(ymin + 0.5f, static_cast<_real_t>(crgn.ymin))),
				maxy = static_cast<size_t>(std::clamp<_real_t>(
					ymax + 0.5f, static_cast<_real_t>(crgn.ymin), static_cast<_real_t>(crgn.ymax)
					)),
				scrw = _rtfstk.back().width;
			_vec2_t uvd = uv1 * params.xpi + uv2 * params.xqi + uv3 * params.xri;
			_ivec4f cd = c1 * params.xpi + c2 * params.xqi + c3 * params.xri;
			for (size_t y = miny; y < maxy; ++y) {
				_real_t diff = static_cast<_real_t>(y) - sy, left = diff * invk1 + sx, right = diff * invk2 + sx;
				size_t
					l = static_cast<size_t>(std::max<_real_t>(left, static_cast<_real_t>(crgn.xmin))),
					r = static_cast<size_t>(std::clamp<_real_t>(
						right, static_cast<_real_t>(crgn.xmin), static_cast<_real_t>(crgn.xmax)
						));
				_ivec4f *pixel = tarbuf + y * scrw + l;
				_real_t p, q, mpq;
				params.get_pq(l, y, p, q);
				mpq = 1.0f - p - q;
				_vec2_t uv = uv1 * p + uv2 * q + uv3 * mpq;
				_ivec4f cc = c1 * p + c2 * q + c3 * mpq;
				for (size_t cx = l; cx < r; ++cx, ++pixel, uv += uvd, cc += cd) {
					_ivec4f ncc = cc;
					if (tex.data) {
						ncc *= tex.sample(uv);
					}
					_draw_pixel_with_blend(*pixel, ncc, srcf, dstf);
				}
			}
		}

		/// Draws a line with the given color.
		///
		/// \todo Clipping and rounding issues.
		void _draw_line(_vec2_t, _vec2_t, _color_t);
		/// Draws the line \f$y = k(x - fx) + fy\f$ from \p fx to \p tx with the given color.
		/// Must satisfy that \f$k \leq 1\f$ and \f$fx < tx\f$.
		void _draw_line_right(_real_t fx, _real_t fy, _real_t tx, _real_t k, _color_t c) {
			size_t t = static_cast<size_t>(std::clamp<_real_t>(
				tx, 0.0f, static_cast<_real_t>(_rtfstk.back().width - 1)
				));
			tx -= fx;
			fx -= 0.5f;
			_ivec4f colorv = _convert_from_color(c.convert<float>());
			blend_function fun = _blendstk.back();
			for (size_t cx = static_cast<size_t>(std::max<_real_t>(fx + 0.5f, 0.0f)); cx < t; ++cx) {
				auto y = static_cast<size_t>(fy + k * (static_cast<_real_t>(cx) - fx));
				_ivec4f *pixel = _rtfstk.back().buffer + (y * _rtfstk.back().width + cx);
				_draw_pixel_with_blend(*pixel, colorv, fun.source_factor, fun.destination_factor);
			}
		}
		/// Draws the line \f$x = invk(y - by) + bx\f$ from \p by to \p ty with the given color.
		/// Must satisfy that \f$invk \leq 1\f$ and \f$by < ty\f$.
		void _draw_line_up(_real_t by, _real_t bx, _real_t ty, _real_t invk, _color_t c) {
			size_t t = static_cast<size_t>(std::clamp<_real_t>(
				ty, 0.0f, static_cast<_real_t>(_rtfstk.back().height - 1)
				));
			ty -= by;
			by -= 0.5f;
			_ivec4f colorv = _convert_from_color(c.convert<float>());
			blend_function fun = _blendstk.back();
			for (size_t cy = static_cast<size_t>(std::max<_real_t>(by + 0.5f, 0.0f)); cy < t; ++cy) {
				auto x = static_cast<size_t>(bx + invk * (static_cast<_real_t>(cy) - by));
				_ivec4f *pixel = _rtfstk.back().buffer + (cy * _rtfstk.back().width + x);
				_draw_pixel_with_blend(*pixel, colorv, fun.source_factor, fun.destination_factor);
			}
		}
	};
}
