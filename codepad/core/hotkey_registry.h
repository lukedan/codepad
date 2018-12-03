// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of a registry for hotkey gestures.

#include <type_traits>
#include <vector>
#include <map>
#include <functional>
#include <variant>

#include "../os/misc.h"
#include "encodings.h"

namespace codepad {
	/// Modifier keys of a key gesture.
	enum class modifier_keys {
		none = 0, ///< No modifier keys.

		control = 1, ///< The `control' key.
		shift = 2, ///< The `shift' key.
		alt = 4, ///< The `alt' key.
		super = 8, ///< The `super' key, corresponds to either the `win' key or the `command' key.

		control_shift = control | shift, ///< Contrl + Shift.
		control_alt = control | alt, ///< Control + Alt.
		shift_alt = shift | alt, ///< Shift + Alt.
		control_super = control | super, ///< Control + Super.
		shift_super = shift | super, ///< Shift + Super.
		alt_super = alt | super, ///< Alt + Super.

		control_shift_alt = control | shift | alt, ///< Control + Shift + Alt.
		control_shift_super = control | shift | super, ///< Control + Shift + Super.
		control_alt_super = control | alt | super, ///< Control + Alt + Super.
		shift_alt_super = shift | alt | super, ///< Shift + Alt + Super.

		control_shift_alt_super = control | shift | alt | super ///< Control + Shift + Alt + Super.
	};
	/// Enables bitwise operators for \ref modifier_keys.
	template <> struct enable_enum_bitwise_operators<modifier_keys> : std::true_type {
	};
	/// A key gesture, corresponds to one key stroke with or without modifier keys.
	struct key_gesture {
		/// Default contructor.
		key_gesture() = default;
		/// Constructs a key gesture with a primary key and optionally modifiers.
		///
		/// \param prim The primary key.
		/// \param mod The modifiers.
		explicit key_gesture(os::input::key prim, modifier_keys mod = modifier_keys::none) :
			primary(prim), mod_keys(mod) {
		}

		os::input::key primary = os::input::key::escape; ///< The primary key.
		modifier_keys mod_keys = modifier_keys::none; ///< The modifiers.

		/// Equality of two key gestures.
		friend bool operator==(key_gesture lhs, key_gesture rhs) {
			return lhs.primary == rhs.primary && lhs.mod_keys == rhs.mod_keys;
		}
		/// Inequality of two key gestures.
		friend bool operator!=(key_gesture lhs, key_gesture rhs) {
			return !(lhs == rhs);
		}
		/// Comparison of two key gestures.
		friend bool operator<(key_gesture lhs, key_gesture rhs) {
			return lhs.primary == rhs.primary ? lhs.mod_keys < rhs.mod_keys : lhs.primary < rhs.primary;
		}
		/// Comparison of two key gestures.
		friend bool operator>(key_gesture lhs, key_gesture rhs) {
			return rhs < lhs;
		}
		/// Comparison of two key gestures.
		friend bool operator<=(key_gesture lhs, key_gesture rhs) {
			return !(rhs < lhs);
		}
		/// Comparison of two key gestures.
		friend bool operator>=(key_gesture lhs, key_gesture rhs) {
			return !(lhs < rhs);
		}
	};

	/// A group of non-conflicting hotkeys. A hotkey contains one or more gestures.
	/// To activate a hotkey, the corresponding gestures need to be performed.
	///
	/// \tparam T Type of data associated with hotkeys.
	template <typename T> class hotkey_group {
	public:
		/// Registers a hotkey to this group.
		///
		/// \param sks The hotkey, which consists of a series of gestures. Must not be empty.
		/// \param func The corresponding data.
		/// \return \p true if the registration succeeded.
		///         The registration may fail if there are conflicting hotkeys.
		bool register_hotkey(const std::vector<key_gesture> &sks, T func) {
			assert_true_usage(!sks.empty(), "hotkey is blank");
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
				std::forward_as_tuple(std::in_place_type<T>, std::move(func))
			);
			return true;
		}
		/// Unregister a hotkey from this group. The entry must exist.
		///
		/// \param sks The hotkey.
		void unregister_hotkey(const std::vector<key_gesture> &sks) {
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
			size_t kid = sks.size();
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
	protected:
		/// A node of the registration tree.
		struct _gesture_rec_t {
		public:
			/// Type used to keep track of all available gestures to the next level.
			using layer_rec_t = std::map<key_gesture, _gesture_rec_t>;

			/// Constructs the underlying \p std::variant \ref _v.
			template <typename ...Args> explicit _gesture_rec_t(Args &&...args) :
				_v(std::forward<Args>(args)...) {
			}

			/// Checks if this is a leaf node.
			bool is_leaf() const {
				return std::holds_alternative<T>(_v);
			}

			/// Returns all children gestures if is_leaf() returns \p true.
			layer_rec_t &get_children() {
				return std::get<layer_rec_t>(_v);
			}
			/// Const version of get_children().
			const layer_rec_t &get_children() const {
				return std::get<layer_rec_t>(_v);
			}

			/// Returns the data if is_leaf() returns \p false.
			T &get_data() {
				return std::get<T>(_v);
			}
			/// Const version of get_data().
			const T &get_data() const {
				return std::get<T>(_v);
			}
		protected:
			std::variant<layer_rec_t, T> _v; ///< The underlying union.
		};

		_gesture_rec_t _reg; ///< The root node.
	public:
		/// Struct used to keep track of user input and find corresponding hotkeys.
		struct state {
			friend hotkey_group;
		public:
			/// Default constructor.
			state() = default;

			/// Resets the object to its default state.
			void clear() {
				_ptr = nullptr;
			}
			/// Checks if this object is empty, i.e., if \ref _ptr doesn't point to any node.
			bool is_empty() const {
				return _ptr == nullptr;
			}
			/// Checks if the user has triggered a hotkey.
			bool is_trigger() const {
				return _ptr && _ptr->is_leaf();
			}

			/// Returns the data of the leaf node if is_trigger() returns \p true.
			const T &get_data() const {
				assert_true_logical(is_trigger(), "intermediate nodes doesn't have callbacks");
				return _ptr->get_data();
			}

			/// Equality.
			friend bool operator==(state lhs, state rhs) {
				return lhs._ptr == rhs._ptr;
			}
			/// Inequality.
			friend bool operator!=(state lhs, state rhs) {
				return !(lhs == rhs);
			}
		protected:
			/// Protected constructor, so that only \ref hotkey_group can create a non-empty instance.
			explicit state(const _gesture_rec_t *rec) : _ptr(rec) {
			}

			const _gesture_rec_t *_ptr = nullptr; ///< Points to the node that the user has reached so far.
		};

		/// Update a \ref state given a gesture.
		///
		/// \param kg The key gesture, possibly from user input.
		/// \param s The old state.
		/// \return The updated state.
		state update_state(key_gesture kg, const state &s) const {
			if (
				kg.primary == os::input::key::control ||
				kg.primary == os::input::key::alt ||
				kg.primary == os::input::key::shift
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
	};
}
