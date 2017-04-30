#pragma once

#include <functional>
#include <list>

namespace codepad {
	template <typename T> struct event {
	public:
		struct reg_token {
			friend struct event<T>;
		public:
			reg_token() = default;
		protected:
			typedef typename std::list<std::function<void(T&)>>::iterator _tok_t;
			_tok_t _tok;

			explicit reg_token(const _tok_t &tok) : _tok(tok) {
			}
		};

		event() = default;
		event(const event&) = delete;
		event &operator =(const event&) = delete;

		reg_token operator+=(const std::function<void(T&)> &handler) {
			_list.push_front(handler);
			return reg_token(_list.begin());
		}
		event &operator-=(const reg_token &tok) {
			_list.erase(tok._tok);
			return *this;
		}

		void invoke(T &param) {
			for (auto i = _list.begin(); i != _list.end(); ++i) {
				(*i)(param);
			}
		}
		void operator()(T &param) {
			invoke(param);
		}
	protected:
		std::list<std::function<void(T&)>> _list;
	};

	struct void_info {
	};
}