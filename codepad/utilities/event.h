#pragma once

#include <functional>
#include <list>

namespace codepad {
	template <typename ...Args> struct event_base {
		typedef std::function<void(Args...)> handler;

		struct token {
			friend struct event_base<Args...>;
		public:
			token() = default;
		protected:
			typedef typename std::list<handler>::iterator _tok_t;
			_tok_t _text_tok;

			explicit token(_tok_t tok) : _text_tok(tok) {
			}
		};

		event_base() = default;
		event_base(const event_base&) = delete;
		event_base &operator=(const event_base&) = delete;

		template <typename T> token operator+=(T &&h) {
			_list.push_front(handler(std::forward<T>(h)));
			return token(_list.begin());
		}
		event_base &operator-=(const token &tok) {
			_list.erase(tok._text_tok);
			return *this;
		}

		void invoke(Args &&...p) {
			for (auto i = _list.begin(); i != _list.end(); ++i) {
				(*i)(std::forward<Args>(p)...);
			}
		}
		void operator()(Args &&...p) {
			invoke(std::forward<Args>(p)...);
		}
	protected:
		std::list<handler> _list;
	};

	template <typename T> struct event : public event_base<T&> {
		template <typename ...Con> void invoke_noret(Con &&...args) {
			T p(std::forward<Con>(args)...);
			this->invoke(p);
		}
	};
	template <> struct event<void> : public event_base<> {
	};

	template <typename T> struct value_update_info {
		value_update_info(T ov) : old_value(ov) {
		}
		const T old_value;
	};
}