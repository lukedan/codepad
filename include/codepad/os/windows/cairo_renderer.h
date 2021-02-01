// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Windows implementation of the Cairo renderer.

#include <cairo-win32.h>

#include "../../ui/backends/cairo_renderer_base.h"

namespace codepad::os {
	/// Windows implementation of the Cairo renderer.
	class cairo_renderer : public ui::cairo::renderer_base {
	public:
		/// Loads a \ref bitmap from disk as an image surface.
		std::shared_ptr<ui::bitmap> load_bitmap(const std::filesystem::path &bmp, vec2d scaling_factor) override {
			auto resbmp = std::make_shared<ui::cairo::bitmap>();

			_details::com_wrapper<IWICBitmapSource> img = _details::wic_image_loader::get().load_image(bmp);
			_details::com_wrapper<IWICBitmapSource> converted;
			// cairo uses premultiplied alpha
			_details::com_check(WICConvertBitmapSource(GUID_WICPixelFormat32bppPBGRA, img.get(), converted.get_ref()));

			UINT width = 0, height = 0;
			_details::com_check(converted->GetSize(&width, &height));
			_bitmap_size(*resbmp) = vec2d(width / scaling_factor.x, height / scaling_factor.y);
			_bitmap_surface(*resbmp) = _create_offscreen_surface(width, height, scaling_factor);

			// map surface
			cairo_surface_t *bmp_surface = _bitmap_surface(*resbmp).get();
			cairo_surface_flush(bmp_surface); // since unmap_image doesn't mark dirty we're adding this here too
			cairo_surface_t *mapped_surface = cairo_surface_map_to_image(bmp_surface, nullptr);
			cairo_surface_flush(bmp_surface); // doesn't seem necessary, but added for good measure
			// copy image data
			unsigned char *data = cairo_image_surface_get_data(mapped_surface);
			assert_true_sys(
				cairo_image_surface_get_width(mapped_surface) == width &&
				cairo_image_surface_get_height(mapped_surface) == height
			);
			int stride = cairo_image_surface_get_stride(mapped_surface);
			_details::com_check(converted->CopyPixels(nullptr, stride, height * stride, data));
			// "Each pixel is a 32-bit quantity, with alpha in the upper 8 bits, then red, then green, then blue.
			// The 32-bit quantities are stored native-endian."
			// https://cairographics.org/manual/cairo-Image-Surfaces.html#cairo-format-t
			// this means that for little endian, CAIRO_FORMAT_ARGB32 is exactly the same as WICPixelFormat32bppPBGRA
			// since for WIC the order is that in which each color channel appears in the bit stream
			if constexpr (system_endianness == endianness::big_endian) { // convert from BGRA to ARGB
				for (UINT y = 0; y < height; ++y) {
					unsigned char *row = data + stride * y;
					for (UINT x = 0; x < width; ++x, row += 4) {
						std::swap(row[0], row[3]);
						std::swap(row[1], row[2]);
					}
				}
			}
			// unmap
			cairo_surface_mark_dirty(mapped_surface);
			cairo_surface_unmap_image(bmp_surface, mapped_surface);
			// why the hell does unmap_image not handle this? without this the surface would be blank
			cairo_surface_mark_dirty(bmp_surface);

			return resbmp;
		}

		/// Pushes the window and its context onto the stack.
		void begin_drawing(ui::window &w) override {
			auto &data = _get_window_data_as<_window_data>(w);
			_render_stack.emplace(data.context.get(), &w);
		}
	protected:
		/// Flushes the surface if the target is a window.
		void _finish_drawing_to_target() override {
			if (_render_stack.top().window) {
				cairo_surface_flush(cairo_get_target(_render_stack.top().context));
			}
		}

		// TODO if you drag the window to the corner of the screen so that it's only partially visible, resize it
		//      (which causes the surface to be re-created), then drag it back, the resulting surface will
		//      incorrectly have a smaller size
		/// Creates a cairo context from a newly created Win32 surface.
		ui::_details::gtk_object_ref<cairo_t> _create_context_for_window(
			ui::window &w, vec2d scaling
		) {
			auto surface = ui::_details::make_gtk_object_ref_give(cairo_win32_surface_create_with_format(
				GetDC(_details::cast_window_impl(w.get_impl()).get_native_handle()), CAIRO_FORMAT_ARGB32
			));
			cairo_surface_set_device_scale(surface.get(), scaling.x, scaling.y);
			auto result = ui::_details::make_gtk_object_ref_give(cairo_create(surface.get()));
			return result;
		}

		/// Creates a Cairo surface for the window, and listens to specific events to resize the surface as needed.
		void _new_window(ui::window &wnd) override {
			renderer_base::_new_window(wnd);

			// create context
			_get_window_data_as<_window_data>(wnd).context = _create_context_for_window(wnd, wnd.get_scaling_factor());
			// resize buffer when the window size has changed
			wnd.size_changed += [this, pwnd = &wnd](ui::window::size_changed_info&) {
				auto &data = _get_window_data_as<_window_data>(*pwnd);
				data.context.reset();
				data.context = _create_context_for_window(*pwnd, pwnd->get_scaling_factor());
				pwnd->invalidate_visual();
			};
			// reallocate buffer when the window scaling has changed
			wnd.scaling_factor_changed += [this, pwnd = &wnd](ui::window::scaling_factor_changed_info &p) {
				auto &data = _get_window_data_as<_window_data>(*pwnd);
				data.context.reset();
				data.context = _create_context_for_window(*pwnd, p.new_value);
				pwnd->invalidate_visual();
			};
		}
	};
}
