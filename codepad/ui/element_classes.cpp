#include "element_classes.h"

/// \file
/// Implementation of certain functions about element classes.

#include "manager.h"

namespace codepad::ui {
	element_state_id ui_config_parser::_parse_state_id(const json::value_t &val, bool configonly) {
		element_state_id id = normal_element_state_id;
		if (val.IsString()) {
			id = manager::get().get_state_info(json::get_as_string(val)).id;
		} else if (val.IsArray()) {
			for (auto j = val.Begin(); j != val.End(); ++j) {
				if (j->IsString()) {
					element_state_info st = manager::get().get_state_info(json::get_as_string(*j));
					if (configonly && st.type != element_state_type::configuration) {
						logger::get().log_warning(CP_HERE, "non-config state bit encountered in config-only state");
					} else {
						id |= st.id;
					}
				}
			}
		} else {
			logger::get().log_warning(CP_HERE, "invalid state ID format");
		}
		return id;
	}

	void ui_config_parser::parse_layer(visual_layer &layer, const json::value_t &val, texture_table &table) {
		if (val.IsObject()) {
			_find_and_parse_ani(val, CP_STRLIT("texture"), layer.texture_animation, table);
			_find_and_parse_ani(val, CP_STRLIT("color"), layer.color_animation);
			_find_and_parse_ani(val, CP_STRLIT("size"), layer.size_animation);
			_find_and_parse_ani(val, CP_STRLIT("margin"), layer.margin_animation);
			_try_find_and_parse(val, CP_STRLIT("type"), layer.layer_type);
			_try_find_and_parse(val, CP_STRLIT("anchor"), layer.rect_anchor);
			_try_find_and_parse(val, CP_STRLIT("width_alloc"), layer.width_alloc);
			_try_find_and_parse(val, CP_STRLIT("height_alloc"), layer.height_alloc);
		} else if (val.IsString()) {
			layer = visual_layer();
			parse_animation(layer.texture_animation, val, table);
			return;
		} else {
			logger::get().log_warning(CP_HERE, "invalid layer info");
		}
	}

	void ui_config_parser::parse_metrics_state(const json::value_t &val, element_metrics &value) {
		if (val.IsObject()) {
			metrics_state mst, *dest = &mst;
			state_pattern pattern = _parse_state_pattern(val);
			json::value_t::ConstMemberIterator fmem;
			fmem = val.FindMember(CP_STRLIT("inherit_from"));
			if (fmem != val.MemberEnd()) {
				state_pattern frompat = _parse_state_pattern(fmem->value);
				metrics_state *st = value.try_get_state(frompat);
				if (st != nullptr) {
					mst = *st;
				} else {
					logger::get().log_warning(CP_HERE, "invalid inheritance");
				}
			} else {
				metrics_state *present = value.try_get_state(pattern);
				if (present != nullptr) {
					dest = present;
				}
			}
			fmem = val.FindMember(CP_STRLIT("value"));
			if (fmem != val.MemberEnd() && fmem->value.IsObject()) {
				_find_and_parse_ani(fmem->value, CP_STRLIT("size"), dest->size_animation);
				_find_and_parse_ani(fmem->value, CP_STRLIT("margin"), dest->margin_animation);
				_find_and_parse_ani(fmem->value, CP_STRLIT("padding"), dest->padding_animation);
				_try_find_and_parse(fmem->value, CP_STRLIT("anchor"), dest->elem_anchor);
				_try_find_and_parse(fmem->value, CP_STRLIT("width_alloc"), dest->width_alloc);
				_try_find_and_parse(fmem->value, CP_STRLIT("height_alloc"), dest->height_alloc);
			} else {
				logger::get().log_warning(CP_HERE, "cannot find metrics value");
			}
			if (dest == &mst) {
				value.register_state(pattern, std::move(mst));
			}
		} else {
			logger::get().log_warning(CP_HERE, "invalid metrics state format");
		}
	}
}
