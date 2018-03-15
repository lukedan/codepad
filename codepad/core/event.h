#pragma once

/// \file
/// Contains structs used to pass information around components.

#include <functional>
#include <list>

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
			friend struct event_base<Args...>;
		public:
			/// Default construtor.
			token() = default;
		protected:
			typename std::list<handler>::iterator _text_tok; ///< Iterator to the corresponding handler.

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
			_list.push_back(handler(std::forward<T>(h)));
			auto it = _list.end();
			return token(--it);
		}
		/// Unregisters a handler.
		///
		/// \param tok The token returned when registering the handler.
		event_base &operator-=(const token &tok) {
			_list.erase(tok._text_tok);
			return *this;
		}

		/// Invokes all handlers with the given parameters.
		///
		/// \param p The parameters used to invoke the handlers.
		template <typename ...Ts> void invoke(Ts &&...p) {
			for (handler &h : _list) {
				h(std::forward<Ts>(p)...);
			}
		}
		/// Invokes all handlers with the given parameters.
		template <typename ...Ts> void operator()(Ts &&...p) {
			invoke(std::forward<Ts>(p)...);
		}
	protected:
		std::list<handler> _list; ///< The list of handlers that have been registered to this event.
	};

	/// Represents an event. Handlers receive a (non-const) reference
	/// to a struct containing information about the event.
	///
	/// \tparam T The type of information about the event.
	template <typename T> struct event : public event_base<T&> {
		/// Used to invoke the event when the parameters doesn't change in a meaningful way,
		/// and when the caller doesn't wish to explicitly create an instance of the parameters.
		///
		/// \param args Arguments used to initialize an instance, which is then passed to the handlers.
		template <typename ...Con> void invoke_noret(Con &&...args) {
			T p(std::forward<Con>(args)...);
			this->invoke(p);
		}
	};
	/// Specialization of \ref event when there's no parameters.
	template <> struct event<void> : public event_base<> {
	};

	/// Generic parameters for events in which a value is updated. This struct holds the old value.
	///
	/// \tparam T The type of the parameter.
	template <typename T> struct value_update_info {
		/// Constructor.
		///
		/// \param ov The old value.
		value_update_info(T ov) : old_value(std::move(ov)) {
		}
		const T old_value; ///< The old value.
	};
}
