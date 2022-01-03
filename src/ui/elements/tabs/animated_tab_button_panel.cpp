// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/tabs/animated_tab_button_panel.h"

/// \file
/// Implementation of the animated tab button panel.

namespace codepad::ui::tabs {
	/// Returns whether the given tab button is being dragged.
	[[nodiscard]] inline static bool _is_tab_button_being_dragged(tab_manager &tabman, element &e) {
		if (tabman.is_dragging_any_tab()) {
			for (auto &t : tabman.get_dragging_tabs()) {
				if (&t.target->get_button() == &e) {
					return true;
				}
			}
		}
		return false;
	}
	void animated_tab_buttons_panel::_child_data::set_offset(
		animated_tab_buttons_panel &pnl, element &e, double offset
	) {
		if (!task.empty()) {
			e.get_manager().get_scheduler().cancel_synchronous_task(task);
		}
		current_offset = offset;
		if (_is_tab_button_being_dragged(pnl._get_tab_manager(), e)) {
			// the tab is being dragged, do not start animation
			e.invalidate_layout();
		} else {
			starting_offset = current_offset;
			start = scheduler::clock_t::now();
			task = e.get_manager().get_scheduler().register_synchronous_task(
				scheduler::clock_t::now(), &e,
				[this, panel = &pnl](element *e) -> std::optional<scheduler::clock_t::time_point> {
					e->invalidate_layout();
					double dur = std::chrono::duration<double>(scheduler::clock_t::now() - start).count();
					if (dur > panel->get_animation_duration()) {
						current_offset = 0.0;
						task = scheduler::sync_task_token();
						return std::nullopt;
					}
					double progress = dur / panel->get_animation_duration();
					double position = panel->get_transition_function()(progress);
					current_offset = starting_offset * (1.0 - position);
					return scheduler::clock_t::now();
				}
			);
		}
	}

	void animated_tab_buttons_panel::_child_data::set_dragging_offset(
		animated_tab_buttons_panel&, element &e, double offset
	) {
		if (!task.empty()) {
			e.get_manager().get_scheduler().cancel_synchronous_task(task);
		}
		current_offset = offset;
		e.invalidate_layout();
	}


	void animated_tab_buttons_panel::_on_child_order_changing(element &elem, element *before) {
		stack_panel::_on_child_order_changing(elem, before);

		// find positions
		auto elemit = _children.items().begin(), beforeit = _children.items().begin();
		bool move_forward = true;
		for (; elemit != _children.items().end() && *elemit != &elem; ++elemit) {
		}
		for (; beforeit != _children.items().end() && *beforeit != before; ++beforeit) {
			if (*beforeit == &elem) {
				move_forward = false;
			}
		}
		if (auto next = elemit; ++next == beforeit) { // just in place
			return;
		}
		auto layout_helper = _get_children_layout_helper();
		double offset = layout_helper.compute_span_for(elem), elemoffset = 0.0;
		// add offset
		if (move_forward) {
			for (auto it = beforeit; it != elemit; ++it) {
				auto *data = _get_data(**it);
				data->set_offset(*this, **it, data->current_offset - offset);
				elemoffset += layout_helper.compute_span_for(**it);
			}
		} else {
			for (auto it = ++elemit; it != beforeit; ++it) {
				auto *data = _get_data(**it);
				data->set_offset(*this, **it, data->current_offset + offset);
				elemoffset -= layout_helper.compute_span_for(**it);
			}
		}
		auto *data = _get_data(elem);
		data->set_offset(*this, elem, data->current_offset + elemoffset);
	}

	/// Information about a tab being dragged.
	struct _tab_record {
		/// Default constructor.
		_tab_record() = default;
		/// Initializes all fields of this struct.
		_tab_record(tab &tb, double m_before, double sz, double m_after, double pos) :
			target(&tb), margin_before(m_before), size(sz), margin_after(m_after), position(pos) {
		}

		tab *target = nullptr; ///< The tab.
		double margin_before = 0.0; ///< Margin before the element.
		double size = 0.0; ///< Size of the element.
		double margin_after = 0.0; ///< Margin after the element.
		double position = 0.0; ///< Position of the top or left side of this tab, after any margins.

		tab *put_before = nullptr; ///< Which tab to put this before.
		double final_offset = 0.0; ///< Final computed offset for this tab.
	};
	void animated_tab_buttons_panel::_on_drag_update(tab_manager::drag_update_info &info) {
		tab_manager &man = _get_tab_manager();
		stack_layout_helper layout = _get_children_layout_helper();

		// gather size information for all tabs being dragged & sort
		std::vector<_tab_record> records;
		records.reserve(man.get_dragging_tabs().size());
		for (auto &t : man.get_dragging_tabs()) {
			auto [mbefore, sz, mafter] = layout.compute_detailed_span_for(t.target->get_button());
			double pos =
				get_orientation() == orientation::vertical ?
				info.position.y + t.offset.ymin : info.position.x + t.offset.xmin;
			records.emplace_back(*t.target, mbefore, sz, mafter, pos);
		}
		std::sort(records.begin(), records.end(), [](const _tab_record &lhs, const _tab_record &rhs) {
			return lhs.position < rhs.position;
		});

		// compute positions for all tabs
		double sum_position = 0.0;
		auto foreground = records.begin();
		for (auto bkg : _host->get_tabs().items()) {
			if (auto *t = dynamic_cast<tab*>(bkg)) {
				if (_is_tab_button_being_dragged(man, t->get_button())) {
					continue;
				}
				double span = layout.compute_span_for(t->get_button());
				while (
					foreground != records.end() &&
					foreground->position - foreground->margin_before < sum_position + 0.5 * span
				) {
					// put this tab here
					foreground->put_before = t;
					foreground->final_offset = foreground->position - foreground->margin_before - sum_position;
					sum_position += foreground->margin_before + foreground->size + foreground->margin_after;
					++foreground;
				}
				if (foreground == records.end()) {
					break; // early exit
				}
				sum_position += span;
			}
		}
		for (; foreground != records.end(); ++foreground) {
			foreground->final_offset = foreground->position - foreground->margin_before - sum_position;
			sum_position += foreground->margin_before + foreground->size + foreground->margin_after;
		}

		// move all tabs
		for (const auto &rec : records) {
			_host->move_tab_before(*rec.target, rec.put_before);
		}
		// since moving tabs modifies the offset of existing tabs, we need to wait until we've made all calls before
		// setting and overriding the offset values
		for (const auto &rec : records) {
			auto *data = _get_data(rec.target->get_button());
			data->set_dragging_offset(*this, rec.target->get_button(), rec.final_offset);
		}
	}
}
