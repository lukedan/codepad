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

#include "../core/misc.h"
#include "../core/json/misc.h"
#include "../os/misc.h"
#include "misc.h"

namespace codepad {
	namespace ui {
		/// Modifier keys of a key gesture.
		enum class modifier_keys {
			none = 0, ///< No modifier keys.

			control = 1, ///< The `control' key.
			shift = 2, ///< The `shift' key.
			alt = 4, ///< The `alt' key.
			super = 8 ///< The `super' key, corresponds to either the `win' key or the `command' key.
		};
	}
	/// Enables bitwise operators for \ref ui::modifier_keys.
	template <> struct enable_enum_bitwise_operators<ui::modifier_keys> : public std::true_type {
	};
	/// Parser for \ref ui::modifier_keys.
	template <> struct enum_parser<ui::modifier_keys> {
		/// The parser interface.
		static std::optional<ui::modifier_keys> parse(std::u8string_view);
	};
	namespace json {
		/// Parser for \ref ui::modifier_keys.
		template <> struct default_parser<ui::modifier_keys> {
			/// The parser interface.
			template <typename Value> std::optional<ui::modifier_keys> operator()(const Value &val) const {
				if (auto str = val.template cast<std::u8string_view>()) {
					return enum_parser<ui::modifier_keys>::parse(str.value());
				}
				return std::nullopt;
			}
		};
	}

	namespace ui {
		/// Contains functions related to gestures.
		namespace gestures {
			constexpr static char32_t delimiter = U'+'; /// The delimiter of gesture components.

			/// Splits the given gesture description into \ref modifier_keys and the primary key or mouse button.
			std::pair<modifier_keys, std::u8string_view> split(std::u8string_view);
		}

		/// A key gesture, corresponds to one key stroke with or without modifier keys.
		struct key_gesture {
			/// Default contructor.
			key_gesture() = default;
			/// Constructs a key gesture with a primary key and optionally modifiers.
			explicit key_gesture(key prim, modifier_keys mod = modifier_keys::none) :
				primary(prim), mod_keys(mod) {
			}

			key primary = key::escape; ///< The primary key.
			modifier_keys mod_keys = modifier_keys::none; ///< The modifiers.

			/// Parses a \ref key_gesture from a string.
			inline static std::optional<key_gesture> parse(std::u8string_view text) {
				auto [mod, primary] = gestures::split(text);
				if (auto primary_key = enum_parser<key>::parse(primary)) {
					return key_gesture(primary_key.value(), mod);
				}
				return std::nullopt;
			}

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

		/// A mouse gesture, corresponds to one mouse button click with or without modifier keys.
		struct mouse_gesture {
			/// Default contructor.
			mouse_gesture() = default;
			/// Constructs a mouse gesture with a primary mouse button and optionally modifiers.
			explicit mouse_gesture(mouse_button prim, modifier_keys mod = modifier_keys::none) :
				primary(prim), mod_keys(mod) {
			}

			mouse_button primary = mouse_button::primary; ///< The primary button.
			modifier_keys mod_keys = modifier_keys::none; ///< The modifiers.

			/// Parses a \ref mouse_gesture from a string.
			inline static std::optional<mouse_gesture> parse(std::u8string_view text) {
				auto [mod, primary] = gestures::split(text);
				if (auto primary_btn = enum_parser<mouse_button>::parse(primary)) {
					return mouse_gesture(primary_btn.value(), mod);
				}
				return std::nullopt;
			}

			/// Equality of two mouse gestures.
			friend bool operator==(mouse_gesture lhs, mouse_gesture rhs) {
				return lhs.primary == rhs.primary && lhs.mod_keys == rhs.mod_keys;
			}
			/// Inequality of two mouse gestures.
			friend bool operator!=(mouse_gesture lhs, mouse_gesture rhs) {
				return !(lhs == rhs);
			}
			/// Comparison of two mouse gestures.
			friend bool operator<(mouse_gesture lhs, mouse_gesture rhs) {
				return lhs.primary == rhs.primary ? lhs.mod_keys < rhs.mod_keys : lhs.primary < rhs.primary;
			}
			/// Comparison of two mouse gestures.
			friend bool operator>(mouse_gesture lhs, mouse_gesture rhs) {
				return rhs < lhs;
			}
			/// Comparison of two mouse gestures.
			friend bool operator<=(mouse_gesture lhs, mouse_gesture rhs) {
				return !(rhs < lhs);
			}
			/// Comparison of two mouse gestures.
			friend bool operator>=(mouse_gesture lhs, mouse_gesture rhs) {
				return !(lhs < rhs);
			}
		};

		/// A group of non-conflicting hotkeys. A hotkey contains one or more gestures.
		/// To activate a hotkey, the corresponding gestures need to be performed.
		///
		/// \tparam T Type of data associated with hotkeys.
		class hotkey_group {
		public:
			/// Registers a hotkey to this group.
			///
			/// \param gs The hotkey, which consists of a series of gestures.
			/// \param action Name of the corresponding action.
			/// \return \p true if the registration succeeded. The registration may fail if the hotkey is empty or
			///         if there are conflicting hotkeys.
			bool register_hotkey(const std::vector<key_gesture> &gs, std::u8string action);
			/// Unregister a hotkey from this group. The entry must exist.
			///
			/// \param gs The hotkey.
			void unregister_hotkey(const std::vector<key_gesture> &gs);
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
					return std::holds_alternative<std::u8string>(_v);
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
				std::u8string &get_data() {
					return std::get<std::u8string>(_v);
				}
				/// Const version of get_data().
				const std::u8string &get_data() const {
					return std::get<std::u8string>(_v);
				}
			protected:
				std::variant<layer_rec_t, std::u8string> _v; ///< The underlying union.
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
				const std::u8string &get_data() const {
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

			/// Update a \ref state given a gesture. This funciton checks if there are any matching hotkeys in this
			/// group, and returns the corresponding next state if one is found. Otherwise, if there's no matching
			/// hotkeys, this function returns an empty state. This function also returns the input state unchanged
			/// if \ref key_gesture::primary is a modifier key.
			state update_state(key_gesture, const state&) const;
		};
	}
}
