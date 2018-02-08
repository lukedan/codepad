#pragma once

#include <vector>
#include <map>
#include <functional>
#include <variant>

#include "../os/misc.h"
#include "encodings.h"

namespace codepad {
	enum class modifier_keys {
		none = 0,

		control = 1,
		shift = 2,
		alt = 4,
		super = 8,

		control_shift = control | shift,
		control_alt = control | alt,
		shift_alt = shift | alt,
		control_super = control | super,
		shift_super = shift | super,
		alt_super = alt | super,

		control_shift_alt = control | shift | alt,
		control_shift_super = control | shift | super,
		control_alt_super = control | alt | super,
		shift_alt_super = shift | alt | super,

		control_shift_alt_super = control | shift | alt | super
	};
	template <> inline str_t to_str<modifier_keys>(modifier_keys mk) {
		str_t ss;
		bool first = true;
		if (test_bit_all(mk, modifier_keys::control)) {
			ss.append(CP_STRLIT("Control"));
			first = false;
		}
		if (test_bit_all(mk, modifier_keys::shift)) {
			if (first) {
				ss.append(CP_STRLIT("+"));
				first = false;
			}
			ss.append(CP_STRLIT("Shift"));
		}
		if (test_bit_all(mk, modifier_keys::alt)) {
			if (first) {
				ss.append(CP_STRLIT("+"));
				first = false;
			}
			ss.append(CP_STRLIT("Alt"));
		}
		if (test_bit_all(mk, modifier_keys::super)) {
			if (first) {
				ss.append(CP_STRLIT("+"));
			}
			ss.append(CP_STRLIT("Super"));
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

	template <typename T> class hotkey_group {
	public:
		bool register_hotkey(const std::vector<key_gesture> &sks, T func) {
			assert_true_usage(sks.size() > 0, "hotkey is blank");
			auto i = sks.begin();
			_gesture_rec_t *c = &_reg;
			for (; i != sks.end(); ++i) {
				typename _gesture_rec_t::layer_rec_t &children = c->get_children();
				auto nl = children.find(*i);
				if (nl == children.end()) {
					break;
				} else if (nl->second.is_leaf()) {
					return false;
				}
				c = &nl->second;
			}
			if (i == sks.end()) {
				return false;
			}
			for (; i + 1 != sks.end(); ++i) {
				c = &c->get_children()[*i];
			}
			c->get_children().emplace(
				std::piecewise_construct,
				std::forward_as_tuple(sks.back()),
				std::forward_as_tuple(std::in_place_type<T>, std::move(func))
			);
			return true;
		}
		void unregister_hotkey(const std::vector<key_gesture> &sks) {
			_gesture_rec_t *c = &_reg;
			std::vector<_gesture_rec_t*> stk;
			for (auto i = sks.begin(); i != sks.end(); ++i) {
				stk.push_back(c);
				typename _gesture_rec_t::layer_rec_t &children = c->get_children();
				auto nl = children.find(*i);
				assert_true_logical(nl != children.end());
				c = &nl->second;
			}
			assert_true_logical(c->is_leaf(), "invalid hotkey chain to unregister");
			size_t kid = sks.size();
			for (auto i = stk.rbegin(); i != stk.rend(); ++i, --kid) {
				typename _gesture_rec_t::layer_rec_t &children = (*i)->get_children();
				if (children.size() > 1) {
					children.erase(sks[--kid]);
					break;
				}
			}
		}
	protected:
		struct _gesture_rec_t {
		public:
			using layer_rec_t = std::map<key_gesture, _gesture_rec_t>;

			template <typename ...Args> explicit _gesture_rec_t(Args &&...args) :
				_v(std::forward<Args>(args)...) {
			}

			bool is_leaf() const {
				return std::holds_alternative<T>(_v);
			}

			layer_rec_t &get_children() {
				return std::get<layer_rec_t>(_v);
			}
			const layer_rec_t &get_children() const {
				return std::get<layer_rec_t>(_v);
			}

			T &get_data() {
				return std::get<T>(_v);
			}
			const T &get_data() const {
				return std::get<T>(_v);
			}
		protected:
			std::variant<layer_rec_t, T> _v;
		};

		_gesture_rec_t _reg;
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
				return _ptr && _ptr->is_leaf();
			}

			const T &get_data() const {
				assert_true_usage(is_trigger(), "intermediate nodes doesn't have callbacks");
				return _ptr->get_data();
			}

			friend bool operator==(state lhs, state rhs) {
				return lhs._ptr == rhs._ptr;
			}
			friend bool operator!=(state lhs, state rhs) {
				return !(lhs == rhs);
			}
		private:
			explicit state(const _gesture_rec_t *rec) : _ptr(rec) {
			}

			const _gesture_rec_t *_ptr = nullptr;
		};

		state update_state(key_gesture kg, state &s) const {
			if (
				kg.primary == os::input::key::control ||
				kg.primary == os::input::key::alt ||
				kg.primary == os::input::key::shift
				) {
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
			return state();
		}
	};
}
