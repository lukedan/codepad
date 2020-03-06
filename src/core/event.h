// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Contains structs used to pass information around components.

#include <functional>
#include <list>
#include <optional>

#include "misc.h"

namespace codepad {
	/// Template for event structs. Users can register and unregister handlers.
	/// The handlers will be invoked in the order in which they're registered.
	/// To register an event:
	/// \code auto token = event += []() {...}; \endcode
	/// To unregister an event:
	/// \code event -= token; \endcode
	///
	/// \tparam Args Types of arguments that registered handlers accept.
	template <typename ...Args> struct event_base {
		/// Wrapper type for handlers.
		using handler = std::function<void(Args...)>;

		/// Returned by operator+=(), used to unregister a handler.
		/// Used instead of raw iterators to prohibit direct access to the elements.
		struct token {
			friend event_base<Args...>;
		public:
			/// Default construtor.
			token() = default;

			/// Returns whether this token represents a registered event handler. Note that this may not be accurate
			/// if the event itself has been disposed.
			bool valid() const {
				return _text_tok.has_value();
			}
		protected:
			/// Iterator to the corresponding handler.
			std::optional<typename std::list<handler>::iterator> _text_tok;

			/// Protected constructor that sets the value of \ref _text_tok.
			explicit token(const typename std::list<handler>::iterator &tok) : _text_tok(tok) {
			}
		};

		/// Default constructor.
		event_base() = default;
		/// Events cannot be copied.
		event_base(const event_base&) = delete;
		/// Events cannot be copied.
		event_base &operator=(const event_base&) = delete;

		/// Registers a handler to this event.
		///
		/// \param h The handler.
		/// \return The corresponding token, which can be used to unregister \p h.
		template <typename T> token operator+=(T &&h) {
			_list.emplace_back(std::forward<T>(h));
			auto it = _list.end();
			return token(--it);
		}
		/// Unregisters a handler and resets the given token.
		///
		/// \param tok The token returned when registering the handler.
		event_base &operator-=(token &tok) {
			if (_state && _state->executing == tok._text_tok.value()) {
				_state->erase_when_done = true;
			} else {
				_list.erase(tok._text_tok.value());
			}
			tok._text_tok.reset();
			return *this;
		}

		/// Invokes all handlers with the given parameters.
		///
		/// \param p The parameters used to invoke the handlers.
		template <typename ...Ts> void invoke(Ts &&...p) {
			_invoke_state st;
			_state = &st;
			for (auto it = _list.begin(); it != _list.end(); ) {
				st.executing = it;
				(*it)(std::forward<Ts>(p)...);
				if (st.erase_when_done) {
					it = _list.erase(it);
					st.erase_when_done = false;
				} else {
					++it;
				}
			}
			_state = nullptr;
		}
		/// Invokes all handlers with the given parameters.
		template <typename ...Ts> void operator()(Ts &&...p) {
			invoke(std::forward<Ts>(p)...);
		}
	protected:
		/// Records the state when \ref invoke() is called. Used to support removing handlers when invoking them.
		struct _invoke_state {
			typename std::list<handler>::const_iterator executing; ///< The handler that's being executed.
			bool erase_when_done = false; ///< Whether to unregister the current handler when it's finished executing.
		};

		std::list<handler> _list; ///< The list of handlers that have been registered to this event.
		_invoke_state *_state = nullptr; ///< Records the state of the current invocation to \ref invoke().
	};

	/// Represents an event whose handlers receive a non-const reference to a struct containing information about
	/// the event.
	///
	/// \tparam T The type of information about the event.
	template <typename T = void> struct info_event : public event_base<T&> {
		/// Used to invoke the event when the parameters doesn't change in a meaningful way,
		/// and when the caller doesn't wish to explicitly create an instance of the parameters.
		///
		/// \param args Arguments used to initialize an instance, which is then passed to the handlers.
		template <typename ...Con> void invoke_noret(Con &&...args) {
			T p(std::forward<Con>(args)...);
			this->invoke(p);
		}
	};
	/// Specialization of \ref info_event when there's no parameters.
	template <> struct info_event<void> : public event_base<> {
	};


	/// The contents of a \ref value_update_info.
	enum class value_update_info_contents : unsigned char {
		old_value = 1, ///< The structure contains the old value.
		new_value = 2, ///< The structure contains the new value.
		old_and_new_values = old_value | new_value ///< The structure contains both the old value and the new value.
	};
	/// Enables bitwise operators for \ref value_update_info_contents.
	template <> struct enable_enum_bitwise_operators<value_update_info_contents> : public std::true_type {
	};

	/// Generic parameters for events in which a value is updated.
	///
	/// \tparam T The type of the parameter.
	/// \tparam Contents The contents of this struct.
	template <typename T, value_update_info_contents Contents> struct value_update_info;

	/// Specialization of \ref value_update_info where only the old value is stored in the struct.
	template <typename T> struct value_update_info<T, value_update_info_contents::old_value> {
		using value_type = T; ///< The value type.
		/// The contents of this struct.
		constexpr static value_update_info_contents contents = value_update_info_contents::old_value;

		/// Constructor.
		explicit value_update_info(T val) : old_value(std::move(val)) {
		}
		const T old_value; ///< The old value.
	};
	/// Specialization of \ref value_update_info where only the new value is stored in the struct.
	template <typename T> struct value_update_info<T, value_update_info_contents::new_value> {
		using value_type = T; ///< The value type.
		/// The contents of this struct.
		constexpr static value_update_info_contents contents = value_update_info_contents::new_value;

		/// Constructor.
		explicit value_update_info(T val) : new_value(std::move(val)) {
		}
		const T new_value; ///< The new value.
	};
	/// Specialization of \ref value_update_info where both the old and the new values are stored in the struct.
	template <typename T> struct value_update_info<T, value_update_info_contents::old_and_new_values> {
		using value_type = T; ///< The value type.
		/// The contents of this struct.
		constexpr static value_update_info_contents contents = value_update_info_contents::old_and_new_values;

		/// Constructor.
		value_update_info(T oldv, T newv) : old_value(std::move(oldv)), new_value(std::move(newv)) {
		}
		const T
			old_value, ///< The old value.
			new_value; ///< The new value.
	};
}
