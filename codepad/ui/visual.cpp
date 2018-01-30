#include "visual.h"
#include "element_classes.h"
#include "element.h"

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
