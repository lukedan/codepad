// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/decoration.h"

/// \file
/// Classes used to store and render text decoration.

#include "codepad/editors/manager.h"

namespace codepad::editors {
	/// Value handler for decoration renderers.
	class _decoration_renderer_value_handler :
		public ui::typed_animation_value_handler<std::shared_ptr<decoration_renderer>> {
	public:
		/// Initializes \ref _manager and \ref _editor_manager.
		_decoration_renderer_value_handler(ui::manager &ui_man, manager &man) :
			_manager(ui_man), _editor_manager(man) {
		}

		/// Creates a \ref decoration_renderer using the \p type attribute and then parses it using
		/// \ref decoration_renderer::parse().
		[[nodiscard]] std::optional<std::shared_ptr<decoration_renderer>> parse(
			const json::value_storage &storage
		) const override {
			auto val = storage.get_value();
			if (auto obj = val.cast<json::storage::object_t>()) {
				if (auto type = obj->parse_member<std::u8string_view>(u8"type")) {
					auto rend = _editor_manager.decoration_renderers.create_renderer(std::u8string(type.value()));
					if (rend) {
						rend->parse(obj.value(), _manager);
						return rend;
					}
				}
			}
			return std::nullopt;
		}
	protected:
		ui::manager &_manager; ///< The \ref ui::manager.
		manager &_editor_manager; ///< The associated \ref manager that contains the registry for decoration types.
	};
	ui::property_info decoration_renderer::find_property_info_handler(
		ui::component_property_accessor_builder &builder, ui::manager &man, manager &editor_man
	) {
		auto *next = builder.peek_next();
		if (!next) {
			ui::property_info result;
			result.accessor = builder.finish_and_create_accessor<std::shared_ptr<decoration_renderer>>();
			result.value_handler = std::make_shared<_decoration_renderer_value_handler>(man, editor_man);
			return result;
		}
		if (auto *rend_type = editor_man.decoration_renderers.find_renderer_type(next->type)) {
			builder.make_append_accessor_component<
				ui::property_path::address_accessor_components::dereference_component<
					std::shared_ptr<decoration_renderer>
				>
			>();
			return rend_type->property_finder(builder, man);
		}
		logger::get().log_error(CP_HERE) << "unregistered decoration renderer type: " << next->type;
		return builder.fail();
	}


	namespace decoration_renderers {
		void rounded_renderer::render(ui::renderer_base &rend, const decoration_layout &deco, vec2d client) const {
			if (deco.line_bounds.empty()) {
				return;
			}

			auto &builder = rend.start_path();

			double
				toprx = _half_radius(deco.line_bounds.front().second - deco.line_bounds.front().first),
				ry = _half_radius(deco.line_height);

			builder.move_to(vec2d(deco.line_bounds.front().first, deco.top + ry));
			builder.add_arc(
				vec2d(deco.line_bounds.front().first + toprx, deco.top), vec2d(toprx, ry), 0.0,
				ui::sweep_direction::clockwise, ui::arc_type::minor
			);
			builder.add_segment(vec2d(deco.line_bounds.front().second - toprx, deco.top));
			builder.add_arc(
				vec2d(deco.line_bounds.front().second, deco.top + ry), vec2d(toprx, ry), 0.0,
				ui::sweep_direction::clockwise, ui::arc_type::minor
			);

			double y = deco.top + deco.line_height;
			{ // downwards
				auto last = deco.line_bounds.begin(), cur = last;
				for (++cur; cur != deco.line_bounds.end(); last = cur, ++cur, y += deco.line_height) {
					bool right = cur->second > last->second;
					double
						rx = _half_radius(std::abs(last->second - cur->second)),
						posrx = right ? rx : -rx;

					builder.add_segment(vec2d(last->second, y - ry));
					builder.add_arc(
						vec2d(last->second + posrx, y), vec2d(rx, ry), 0.0,
						right ? ui::sweep_direction::counter_clockwise : ui::sweep_direction::clockwise,
						ui::arc_type::minor
					);
					builder.add_segment(vec2d(cur->second - posrx, y));
					builder.add_arc(
						vec2d(cur->second, y + ry), vec2d(rx, ry), 0.0,
						right ? ui::sweep_direction::clockwise : ui::sweep_direction::counter_clockwise,
						ui::arc_type::minor
					);
				}
			}

			double botrx = _half_radius(deco.line_bounds.back().second - deco.line_bounds.back().first);

			builder.add_segment(vec2d(deco.line_bounds.back().second, y - ry));
			builder.add_arc(
				vec2d(deco.line_bounds.back().second - botrx, y), vec2d(botrx, ry), 0.0,
				ui::sweep_direction::clockwise, ui::arc_type::minor
			);
			builder.add_segment(vec2d(deco.line_bounds.back().first + botrx, y));
			builder.add_arc(
				vec2d(deco.line_bounds.back().first, y - ry), vec2d(botrx, ry), 0.0,
				ui::sweep_direction::clockwise, ui::arc_type::minor
			);

			{ // upwards
				y -= deco.line_height;
				auto last = deco.line_bounds.rbegin(), cur = last;
				for (++cur; cur != deco.line_bounds.rend(); last = cur, ++cur, y -= deco.line_height) {
					bool right = cur->first > last->first;
					double
						rx = _half_radius(std::abs(cur->first - last->first)),
						posrx = right ? rx : -rx;

					builder.add_segment(vec2d(last->first, y + ry));
					builder.add_arc(
						vec2d(last->first + posrx, y), vec2d(rx, ry), 0.0,
						right ? ui::sweep_direction::clockwise : ui::sweep_direction::counter_clockwise,
						ui::arc_type::minor
					);
					builder.add_segment(vec2d(cur->first - posrx, y));
					builder.add_arc(
						vec2d(cur->first, y - ry), vec2d(rx, ry), 0.0,
						right ? ui::sweep_direction::counter_clockwise : ui::sweep_direction::clockwise,
						ui::arc_type::minor
					);
				}
			}

			builder.close();

			rend.end_and_draw_path(brush.get_parameters(client), pen.get_parameters(client));
		}

		ui::property_info rounded_renderer::find_property_info(
			ui::component_property_accessor_builder &builder, ui::manager &man
		) {
			if (!builder.move_next()) {
				return builder.fail(); // full parsing handled separately
			}
			builder.expect_type(u8"rounded_decoration_renderer");
			if (builder.current_component().property == u8"stroke") {
				return builder.append_member_and_find_property_info_managed<&rounded_renderer::pen>(man);
			}
			if (builder.current_component().property == u8"fill") {
				return builder.append_member_and_find_property_info_managed<&rounded_renderer::brush>(man);
			}
			if (builder.current_component().property == u8"radius") {
				return builder.append_member_and_find_property_info<&rounded_renderer::radius>();
			}
			return builder.fail();
		}

		void rounded_renderer::parse(const json::storage::object_t &obj, ui::manager &man) {
			if (auto stroke = obj.parse_optional_member<ui::generic_pen_parameters>(
				u8"stroke", ui::managed_json_parser<ui::generic_pen_parameters>(man)
			)) {
				pen = std::move(stroke.value());
			}
			if (auto fill = obj.parse_optional_member<ui::generic_brush_parameters>(
				u8"fill", ui::managed_json_parser<ui::generic_brush_parameters>(man)
			)) {
				brush = std::move(fill.value());
			}
			if (auto r = obj.parse_optional_member<double>(u8"radius")) {
				radius = r.value();
			}
		}


		void squiggle_renderer::render(ui::renderer_base &rend, const decoration_layout &layout, vec2d unit) const {
			double y = layout.top + layout.baseline + offset;
			auto &builder = rend.start_path();
			for (auto [beg, end] : layout.line_bounds) {
				builder.move_to(vec2d(beg, y));
				bool top = true;
				for (double x = beg; x < end; top = !top) {
					vec2d
						control = control_offset,
						left = vec2d(x, y),
						right = vec2d(x + width, y);
					if (top) {
						control.y = -control.y;
					}
					builder.add_cubic_bezier(right, left + control, right + vec2d(-control.x, control.y));
					x = right.x;
				}

				y += layout.line_height;
			}
			rend.end_and_draw_path(ui::generic_brush(), pen.get_parameters(unit));
		}

		ui::property_info squiggle_renderer::find_property_info(
			ui::component_property_accessor_builder &builder, ui::manager &man
		) {
			if (!builder.move_next()) {
				return builder.fail(); // full parsing handled separately
			}
			builder.expect_type(u8"squiggle_decoration_renderer");
			if (builder.current_component().property == u8"stroke") {
				return builder.append_member_and_find_property_info_managed<&squiggle_renderer::pen>(man);
			}
			if (builder.current_component().property == u8"control_offset") {
				return builder.append_member_and_find_property_info<&squiggle_renderer::control_offset>();
			}
			if (builder.current_component().property == u8"offset") {
				return builder.append_member_and_find_property_info<&squiggle_renderer::offset>();
			}
			if (builder.current_component().property == u8"width") {
				return builder.append_member_and_find_property_info<&squiggle_renderer::width>();
			}
			return builder.fail();
		}

		void squiggle_renderer::parse(const json::storage::object_t &obj, ui::manager &man) {
			if (auto stroke = obj.parse_optional_member<ui::generic_pen_parameters>(
				u8"stroke", ui::managed_json_parser<ui::generic_pen_parameters>(man)
			)) {
				pen = std::move(stroke.value());
			}
			if (auto control = obj.parse_optional_member<vec2d>(u8"control_offset")) {
				control_offset = control.value();
			}
			if (auto off = obj.parse_optional_member<double>(u8"offset")) {
				offset = off.value();
			}
			if (auto w = obj.parse_optional_member<double>(u8"width")) {
				width = w.value();
			}
		}
	}
}
