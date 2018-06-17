#include "visual.h"

/// \file
/// Implementation of certain methods related to the visuals of objects.

#include "element_classes.h"
#include "element.h"
#include "draw.h"

using namespace std;
using namespace codepad::os;

namespace codepad::ui {
	rectd visual_layer::get_center_rect(const state &s, rectd client) const {
		element::layout_on_direction(
			test_bit_all(rect_anchor, anchor::left),
			width_alloc == size_allocation_type::fixed,
			test_bit_all(rect_anchor, anchor::right),
			client.xmin, client.xmax,
			s.current_margin.current_value.left,
			width_alloc == size_allocation_type::automatic ? 1.0 : s.current_size.current_value.x,
			s.current_margin.current_value.right
		);
		element::layout_on_direction(
			test_bit_all(rect_anchor, anchor::top),
			height_alloc == size_allocation_type::fixed,
			test_bit_all(rect_anchor, anchor::bottom),
			client.ymin, client.ymax,
			s.current_margin.current_value.top,
			height_alloc == size_allocation_type::automatic ? 1.0 : s.current_size.current_value.y,
			s.current_margin.current_value.bottom
		);
		return client;
	}

	void visual_layer::render(rectd layout, const state &s) const {
		texture empty;
		reference_wrapper<texture> tex(empty); // using a pointer just doesn't seem right here
		if (s.current_texture.current_frame != texture_animation.frames.end()) {
			tex = *s.current_texture.current_frame->first;
		}
		switch (layer_type) {
		case type::solid:
			{
				rectd cln = get_center_rect(s, layout);
				renderer_base::get().draw_quad(
					tex.get(), cln, rectd(0.0, 1.0, 0.0, 1.0), s.current_color.current_value
				);
			}
			break;
		case type::grid:
			{
				size_t w = tex.get().get_width(), h = tex.get().get_height();
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
				rb.draw(tex.get());
			}
			break;
		}
	}


	visual_state::state::state(const visual_state &st) {
		layer_states.reserve(st.layers.size());
		for (auto &i : st.layers) {
			layer_states.emplace_back(i);
		}
	}
	visual_state::state::state(const visual_state &st, const state &old) {
		layer_states.reserve(st.layers.size());
		auto i = st.layers.begin();
		for (
			auto j = old.layer_states.begin();
			i != st.layers.end() && j != old.layer_states.end();
			++i, ++j
			) {
			layer_states.emplace_back(*i, *j);
		}
		for (; i != st.layers.end(); ++i) {
			layer_states.emplace_back(*i);
		}
	}


	void visual_state::update(state &s, double dt) const {
		s.all_stationary = true;
		auto j = s.layer_states.begin();
		for (auto i = layers.begin(); i != layers.end(); ++i, ++j) {
			assert_true_usage(j != s.layer_states.end(), "invalid layer state data");
			i->update(*j, dt);
			if (!j->all_stationary) {
				s.all_stationary = false;
			}
		}
	}


	void visual::render_state::set_class(str_t cls) {
		_cls = std::move(cls);
		_reset_state(
			visual_state::state(class_manager::get().visuals.get_state_or_create(_cls, _state))
		);
	}

	void visual::render_state::set_state(visual_state::id_t s) {
		_state = s;
		_reset_state(
			visual_state::state(class_manager::get().visuals.get_state_or_create(_cls, _state), _animst)
		);
	}
	bool visual::render_state::stationary() const {
		return _timestamp == class_manager::get().visuals.timestamp && _animst.all_stationary;
	}

	void visual::render_state::update() {
		if (!stationary()) {
			const visual_state &vps = class_manager::get().visuals.get_state_or_create(_cls, _state);
			if (_timestamp != class_manager::get().visuals.timestamp) {
				_reset_state(visual_state::state(vps));
			}
			auto now = std::chrono::high_resolution_clock::now();
			vps.update(_animst, std::chrono::duration<double>(now - _last).count());
			_last = now;
		}
	}

	void visual::render_state::render(rectd rgn) {
		const visual_state &ps = class_manager::get().visuals.get_state_or_create(_cls, _state);
		if (_timestamp != class_manager::get().visuals.timestamp) {
			_reset_state(visual_state::state(ps));
		}
		ps.render(rgn, _animst);
	}

	void visual::render_state::_reset_state(visual_state::state s) {
		_timestamp = class_manager::get().visuals.timestamp;
		_animst = std::move(s);
		_last = std::chrono::high_resolution_clock::now();
	}
}
