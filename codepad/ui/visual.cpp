#include "visual.h"
#include "element_classes.h"
#include "element.h"
#include "draw.h"

namespace codepad::ui {
	rectd visual_layer::get_center_rect(const state &s, rectd client) const {
		element::layout_on_direction(
			test_bit_all(rect_anchor, anchor::left),
			test_bit_all(rect_anchor, anchor::right),
			client.xmin, client.xmax,
			s.current_margin.current_value.left, s.current_margin.current_value.right,
			s.current_size.current_value.x
		);
		element::layout_on_direction(
			test_bit_all(rect_anchor, anchor::top),
			test_bit_all(rect_anchor, anchor::bottom),
			client.ymin, client.ymax,
			s.current_margin.current_value.top, s.current_margin.current_value.bottom,
			s.current_size.current_value.y
		);
		return client;
	}

	void visual_layer::render(rectd layout, const state &s) const {
		if (s.current_texture.current_frame == texture_animation.frames.end()) {
			return;
		}
		switch (layer_type) {
		case type::solid:
			{
				rectd cln = get_center_rect(s, layout);
				os::renderer_base::get().draw_quad(
					*s.current_texture.current_frame->first, cln,
					rectd(0.0, 1.0, 0.0, 1.0), s.current_color.current_value
				);
			}
			break;
		case type::grid:
			{
				size_t
					w = s.current_texture.current_frame->first->get_width(),
					h = s.current_texture.current_frame->first->get_height();
				rectd
					outer = layout, inner = get_center_rect(s, outer),
					texr(
						s.current_margin.current_value.left / static_cast<double>(w),
						1.0 - s.current_margin.current_value.right / static_cast<double>(w),
						s.current_margin.current_value.top / static_cast<double>(h),
						1.0 - s.current_margin.current_value.bottom / static_cast<double>(h)
					);
				colord curc = s.current_color.current_value;
				render_batch rb;
				rb.reserve(18);
				rb.add_quad(
					rectd(outer.xmin, inner.xmin, outer.ymin, inner.ymin),
					rectd(0.0, texr.xmin, 0.0, texr.ymin), curc
				);
				rb.add_quad(
					rectd(inner.xmin, inner.xmax, outer.ymin, inner.ymin),
					rectd(texr.xmin, texr.xmax, 0.0, texr.ymin), curc
				);
				rb.add_quad(
					rectd(inner.xmax, outer.xmax, outer.ymin, inner.ymin),
					rectd(texr.xmax, 1.0, 0.0, texr.ymin), curc
				);
				rb.add_quad(
					rectd(outer.xmin, inner.xmin, inner.ymin, inner.ymax),
					rectd(0.0, texr.xmin, texr.ymin, texr.ymax), curc
				);
				rb.add_quad(
					rectd(inner.xmin, inner.xmax, inner.ymin, inner.ymax),
					rectd(texr.xmin, texr.xmax, texr.ymin, texr.ymax), curc
				);
				rb.add_quad(
					rectd(inner.xmax, outer.xmax, inner.ymin, inner.ymax),
					rectd(texr.xmax, 1.0, texr.ymin, texr.ymax), curc
				);
				rb.add_quad(
					rectd(outer.xmin, inner.xmin, inner.ymax, outer.ymax),
					rectd(0.0, texr.xmin, texr.ymax, 1.0), curc
				);
				rb.add_quad(
					rectd(inner.xmin, inner.xmax, inner.ymax, outer.ymax),
					rectd(texr.xmin, texr.xmax, texr.ymax, 1.0), curc
				);
				rb.add_quad(
					rectd(inner.xmax, outer.xmax, inner.ymax, outer.ymax),
					rectd(texr.xmax, 1.0, texr.ymax, 1.0), curc
				);
				rb.draw(*s.current_texture.current_frame->first);
			}
			break;
		}
	}


	visual_state::state visual_state::init_state() const {
		state res;
		res.timestamp = class_manager::get().visuals.timestamp;
		for (auto i = _layers.begin(); i != _layers.end(); ++i) {
			res.layer_states.push_back(i->init_state());
		}
		return res;
	}

	visual_state::state visual_state::init_state(const state &old) const {
		state res;
		res.timestamp = class_manager::get().visuals.timestamp;
		auto i = _layers.begin();
		for (
			auto j = old.layer_states.begin();
			i != _layers.end() && j != old.layer_states.end();
			++i, ++j
			) {
			res.layer_states.push_back(i->init_state(*j));
		}
		for (; i != _layers.end(); ++i) {
			res.layer_states.push_back(i->init_state());
		}
		return res;
	}

	void visual_state::update(state &s, double dt) const {
		if (s.timestamp != class_manager::get().visuals.timestamp) {
			s = init_state();
		}
		s.all_stationary = true;
		auto j = s.layer_states.begin();
		for (auto i = _layers.begin(); i != _layers.end(); ++i, ++j) {
			assert_true_usage(j != s.layer_states.end(), "invalid layer state data");
			i->update(*j, dt);
			if (!j->all_stationary) {
				s.all_stationary = false;
			}
		}
	}


	void visual::render_state::set_class(str_t cls) {
		_cls = std::move(cls);
		_set_animst_and_last(
			class_manager::get().visuals.get_state_or_create(_cls, _state).init_state()
		);
	}

	void visual::render_state::set_state(visual_state::id_t s) {
		_state = s;
		_set_animst_and_last(
			class_manager::get().visuals.get_state_or_create(_cls, _state).init_state(_animst)
		);
	}
	bool visual::render_state::stationary() const {
		return _animst.timestamp == class_manager::get().visuals.timestamp && _animst.all_stationary;
	}

	void visual::render_state::update() {
		if (!stationary()) {
			const visual_state &vps = class_manager::get().visuals.get_state_or_create(_cls, _state);
			if (_animst.timestamp != class_manager::get().visuals.timestamp) {
				_set_animst_and_last(vps.init_state());
			}
			auto now = std::chrono::high_resolution_clock::now();
			vps.update(_animst, std::chrono::duration<double>(now - _last).count());
			_last = now;
		}
	}

	void visual::render_state::render(rectd rgn) {
		const visual_state &ps = class_manager::get().visuals.get_state_or_create(_cls, _state);
		if (_animst.timestamp != class_manager::get().visuals.timestamp) {
			_set_animst_and_last(ps.init_state());
		}
		ps.render(rgn, _animst);
	}
}
