// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/hotkey_registry.h"

/// \file
/// Implementation of the hotkey registry.

namespace codepad {
	std::optional<ui::modifier_keys> enum_parser<ui::modifier_keys>::parse(std::u8string_view str) {
		// TODO caseless comparison
		if (str == u8"ctrl") {
			return ui::modifier_keys::control;
		} else if (str == u8"alt") {
			return ui::modifier_keys::alt;
		} else if (str == u8"shift") {
			return ui::modifier_keys::shift;
		} else if (str == u8"super") {
			return ui::modifier_keys::super;
		}
		return std::nullopt;
	}

	namespace ui {
		namespace gestures {
			std::pair<modifier_keys, std::u8string_view> split(std::u8string_view text) {
				auto start = text.begin(), cur = start;
				modifier_keys mod = modifier_keys::none;
				for (auto last = cur; cur != text.end(); last = cur) {
					if (codepoint cp; encodings::utf8::next_codepoint(cur, text.end(), cp)) {
						if (last != start && cp == delimiter) {
							std::u8string_view comp_str(&*start, last - start);
							if (auto comp = enum_parser<modifier_keys>::parse(comp_str)) {
								mod |= comp.value();
							} else {
								logger::get().log_warning(CP_HERE) << "failed to parse modifier keys: " << comp_str;
							}
							start = cur;
						}
					}
				}
				return { mod, std::u8string_view(&*start, text.end() - start) };
			}
		}


		bool hotkey_group::register_hotkey(const std::vector<key_gesture> &sks, std::u8string action) {
			if (sks.empty()) {
				return false;
			}
			auto i = sks.begin();
			_gesture_rec_t *c = &_reg;
			// find the corresponding leaf node
			for (; i != sks.end(); ++i) {
				typename _gesture_rec_t::layer_rec_t &children = c->get_children();
				auto nl = children.find(*i);
				if (nl == children.end()) {
					break; // need to create new nodes
				}
				if (nl->second.is_leaf()) {
					return false; // conflict - a prefix of sks
				}
				c = &nl->second;
			}
			if (i == sks.end()) {
				return false; // conflict - sks is a prefix of another hotkey
			}
			// create new nodes on the path
			for (; i + 1 != sks.end(); ++i) {
				c = &c->get_children()[*i];
			}
			c->get_children().emplace(
				std::piecewise_construct,
				std::forward_as_tuple(sks.back()),
				std::forward_as_tuple(std::in_place_type<std::u8string>, std::move(action))
			);
			return true;
		}

		void hotkey_group::unregister_hotkey(const std::vector<key_gesture> &sks) {
			_gesture_rec_t *c = &_reg;
			std::vector<_gesture_rec_t*> stk;
			// find the path to the leaf node
			for (auto i = sks.begin(); i != sks.end(); ++i) {
				stk.push_back(c);
				typename _gesture_rec_t::layer_rec_t &children = c->get_children();
				auto nl = children.find(*i);
				assert_true_logical(nl != children.end());
				c = &nl->second;
			}
			assert_true_logical(c->is_leaf(), "invalid hotkey chain to unregister");
			std::size_t kid = sks.size();
			// find the last node on the path with more than one child
			for (auto i = stk.rbegin(); i != stk.rend(); ++i, --kid) {
				typename _gesture_rec_t::layer_rec_t &children = (*i)->get_children();
				if (children.size() > 1) {
					children.erase(sks[--kid]);
					return;
				}
			}
			// didn't find one, only one hotkey in the group
			_reg.get_children().clear();
		}

		hotkey_group::state hotkey_group::update_state(key_gesture kg, const state &s) const {
			if (
				kg.primary == key::control ||
				kg.primary == key::alt ||
				kg.primary == key::shift
				) { // if the primary key is a modifier, then return s unmodified
				return s;
			}
			const _gesture_rec_t *clvl = s._ptr ? s._ptr : &_reg;
			if (!clvl->is_leaf()) {
				const typename _gesture_rec_t::layer_rec_t &children = clvl->get_children();
				auto cstat = children.find(kg);
				if (cstat != children.end()) {
					return state(&cstat->second);
				}
			}
			return state(); // not a valid gesture; back to initial state
		}
	}
}
