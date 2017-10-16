#pragma once

#include <vector>
#include <map>
#include <functional>

#include "../os/misc.h"
#include "textconfig.h"
#include "textproc.h"

namespace codepad {
	enum class modifier_keys {
		none = 0,

		control = 1,
		shift = 2,
		alt = 4,
		cmd = 8,

		control_shift = control | shift,
		control_alt = control | alt,
		shift_alt = shift | alt,
		control_cmd = control | cmd,
		shift_cmd = shift | cmd,
		alt_cmd = alt | cmd,

		control_shift_alt = control | shift | alt,
		control_shift_cmd = control | shift | cmd,
		control_alt_cmd = control | alt | cmd,
		shift_alt_cmd = shift | alt | cmd,

		control_shift_alt_cmd = control | shift | alt | cmd
	};
	template <> inline str_t to_str<modifier_keys>(modifier_keys mk) {
		str_t ss;
		bool first = true;
		auto mkv = static_cast<unsigned>(mk);
		if (test_bit_all(mkv, modifier_keys::control)) {
			ss.append(U"Control");
			first = false;
		}
		if (test_bit_all(mkv, modifier_keys::shift)) {
			if (first) {
				ss.append(U"+");
				first = false;
			}
			ss.append(U"Shift");
		}
		if (test_bit_all(mkv, modifier_keys::alt)) {
			if (first) {
				ss.append(U"+");
				first = false;
			}
			ss.append(U"Alt");
		}
		if (test_bit_all(mkv, modifier_keys::cmd)) {
			if (first) {
				ss.append(U"+");
			}
			ss.append(U"Cmd");
		}
		return ss;
	}
	struct key_gesture {
		key_gesture() = default;
		key_gesture(os::input::key prim, modifier_keys k) : primary(prim), mod_keys(k) {
		}
		key_gesture(os::input::key k) : key_gesture(k, modifier_keys::none) {
		}

		os::input::key primary = os::input::key::escape;
		modifier_keys mod_keys = modifier_keys::none;

		friend bool operator==(key_gesture lhs, key_gesture rhs) {
			return lhs.primary == rhs.primary && lhs.mod_keys == rhs.mod_keys;
		}
		friend bool operator!=(key_gesture lhs, key_gesture rhs) {
			return !(lhs == rhs);
		}
		friend bool operator<(key_gesture lhs, key_gesture rhs) {
			return lhs.primary == rhs.primary ? lhs.mod_keys < rhs.mod_keys : lhs.primary < rhs.primary;
		}
		friend bool operator>(key_gesture lhs, key_gesture rhs) {
			return rhs < lhs;
		}
		friend bool operator<=(key_gesture lhs, key_gesture rhs) {
			return !(rhs < lhs);
		}
		friend bool operator>=(key_gesture lhs, key_gesture rhs) {
			return !(lhs < rhs);
		}

		inline static key_gesture get_current(os::input::key k) {
			return key_gesture(k, detect_modifier_keys());
		}
		inline static modifier_keys detect_modifier_keys() {
			return static_cast<modifier_keys>(
				static_cast<unsigned>(os::input::is_key_down(os::input::key::control) ? modifier_keys::control : modifier_keys::none) |
				static_cast<unsigned>(os::input::is_key_down(os::input::key::alt) ? modifier_keys::alt : modifier_keys::none) |
				static_cast<unsigned>(os::input::is_key_down(os::input::key::shift) ? modifier_keys::shift : modifier_keys::none)
				);
		}
	};

	template <typename Func> class hotkey_group {
	public:
		template <typename T> bool register_hotkey(const std::vector<key_gesture> &sks, T &&func) {
			assert_true_usage(sks.size() > 0, "hotkey is blank");
			auto i = sks.begin();
			_gesture_rec *c = &_reg;
			for (; i != sks.end(); ++i) {
				auto nl = c->next_layer.find(*i);
				if (nl == c->next_layer.end()) {
					break;
				} else if (nl->second.is_leaf) {
					return false;
				}
				c = &nl->second;
			}
			if (i == sks.end()) {
				return false;
			}
			for (; i + 1 != sks.end(); ++i) {
				c = &c->next_layer[*i];
			}
			c->next_layer.emplace(
				std::piecewise_construct,
				std::forward_as_tuple(sks.back()),
				std::forward_as_tuple(std::function<Func>(std::forward<T>(func)))
			);
			return true;
		}
		void unregister_hotkey(const std::vector<key_gesture> &sks) {
			_gesture_rec *c = &_reg;
			std::vector<_gesture_rec*> stk;
			for (auto i = sks.begin(); i != sks.end(); ++i) {
				stk.push_back(c);
				auto nl = c->next_layer.find(*i);
				assert_true_logical(nl != c->next_layer.end());
				c = &nl->second;
			}
			assert_true_logical(c->is_leaf, "invalid hotkey chain to unregister");
			size_t kid = sks.size();
			for (auto i = stk.rbegin(); i != stk.rend(); ++i, --kid) {
				if ((*i)->next_layer.size() > 1) {
					(*i)->next_layer.erase(sks[--kid]);
					break;
				}
			}
		}
	protected:
		struct _gesture_rec {
			_gesture_rec() : next_layer(), is_leaf(false) {
			}
			explicit _gesture_rec(std::function<Func> cb) : callback(std::move(cb)), is_leaf(true) {
			}
			_gesture_rec(const _gesture_rec&) = delete;
			_gesture_rec &operator=(const _gesture_rec&) = delete;
			~_gesture_rec() {
				if (is_leaf) {
					callback.~function();
				} else {
					next_layer.~map();
				}
			}

			union {
				std::map<key_gesture, _gesture_rec> next_layer;
				std::function<Func> callback;
			};
			bool is_leaf;
		};

		_gesture_rec _reg;
	public:
		struct state {
			friend class hotkey_group;
		public:
			state() = default;

			void clear() {
				_ptr = nullptr;
			}
			bool is_empty() const {
				return _ptr == nullptr;
			}
			bool is_trigger() const {
				return _ptr && _ptr->is_leaf;
			}

			const std::function<Func> &get_callback() const {
				assert_true_usage(is_trigger(), "intermediate nodes doesn't have callbacks");
				return _ptr->callback;
			}

			friend bool operator==(state lhs, state rhs) {
				return lhs._ptr == rhs._ptr;
			}
			friend bool operator!=(state lhs, state rhs) {
				return !(lhs == rhs);
			}
		private:
			explicit state(const _gesture_rec *rec) : _ptr(rec) {
			}

			const _gesture_rec *_ptr = nullptr;
		};

		state update_state(key_gesture kg, state &s) const {
			if (
				kg.primary == os::input::key::control ||
				kg.primary == os::input::key::alt ||
				kg.primary == os::input::key::shift
				) {
				return s;
			}
			const _gesture_rec *clvl = s._ptr ? s._ptr : &_reg;
			auto cstat = clvl->next_layer.find(kg);
			if (cstat == clvl->next_layer.end()) {
				return state();
			}
			return state(&cstat->second);
		}
	};
}
