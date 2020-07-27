// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of an intrusive priority queue where each element is notified when its position in the array
/// changes.

#include <deque>

namespace codepad {
	/// An intrusive priority queue (maximum heap) where each element is notified when its position in the array
	/// changes. The \p Elem class must implement \p _on_position_changed(std::size_t).
	template <
		typename Elem, typename Comp = std::less<Elem>, typename Container = std::deque<Elem>
	> class intrusive_priority_queue {
	public:
		/// Returns a reference to the largest (when using \p std::less) element.
		Elem &top() {
			return _arr.front();
		}
		/// \overload
		const Elem &top() const {
			return _arr.front();
		}

		/// Returns the underlying container.
		const Container &get_container() const {
			return _arr;
		}

		/// Adds an item into the queue.
		void push(const Elem &e) {
			_arr.push_back(e);
			_adjust_element_up(_arr.size() - 1);
		}
		/// \overload
		void push(Elem &&e) {
			_arr.push_back(std::move(e));
			_adjust_element_up(_arr.size() - 1);
		}
		/// Adds a new item into the queue.
		template <typename ...Args> Elem &emplace(Args &&...args) {
			_arr.emplace_back(std::forward<Args>(args)...);
			std::size_t i = _adjust_element_up(_arr.size() - 1);
			return _arr[i];
		}
		/// Removes the top item from the queue.
		void pop() {
			std::swap(_arr[0], _arr[_arr.size() - 1]);
			_arr.pop_back();
			if (_arr.size() > 0) {
				_adjust_element_down(0);
			}
		}

		/// Erases the given element.
		void erase(std::size_t index) {
			std::swap(_arr[index], _arr[_arr.size() - 1]);
			_arr.pop_back();
			if (index < _arr.size()) {
				std::size_t new_id = _adjust_element_down(index);
				if (new_id == index) {
					_adjust_element_up(index);
				}
			}
		}
		/// \overload
		void erase(typename Container::const_iterator it) {
			erase(it - _arr.begin());
		}

		/// Invokes \ref _adjust_element_down().
		void on_key_decreased(std::size_t index) {
			_adjust_element_down(index);
		}
		/// \overload
		void on_key_decreased(typename Container::const_iterator it) {
			on_key_decreased(it - _arr.begin());
		}

		/// Returns the number of elements in this queue.
		std::size_t size() const {
			return _arr.size();
		}
		/// Returns \p true if this queue is empty.
		bool empty() const {
			return _arr.empty();
		}
	private:
		Container _arr; ///< The underlying container.

		/// Returns the index of the parent element.
		inline static std::size_t _parent(std::size_t i) {
			return (i - 1) / 2;
		}
		/// Returns the index of the left child.
		inline static std::size_t _left_child(std::size_t i) {
			return i * 2 + 1;
		}
		/// Returns the index of the right child. This is equal to \ref _left_child() + 1.
		inline static std::size_t _right_child(std::size_t i) {
			return _left_child(i) + 1;
		}

		/// Invokes \p _on_position_changed() on the given element.
		void _notify_index_change(std::size_t index) {
			_arr[index]._on_position_changed(index);
		}

		/// Moves the element up the heap until it's in the correct place.
		///
		/// \return New index of the element.
		std::size_t _adjust_element_up(std::size_t index) {
			Comp comp;
			while (true) {
				if (index == 0) {
					break;
				}
				std::size_t parent = _parent(index);
				if (!comp(_arr[parent], _arr[index])) {
					break;
				}
				std::swap(_arr[parent], _arr[index]);
				_notify_index_change(index);
				index = parent;
			}
			_notify_index_change(index);
			_verify_integrity();
			return index;
		}
		/// Moves the element up the heap until it's in the correct place.
		///
		/// \return New index of the element.
		std::size_t _adjust_element_down(std::size_t index) {
			Comp comp;
			while (true) {
				std::size_t left_child = _left_child(index);
				if (left_child >= size()) {
					break;
				}
				std::size_t next = left_child, right_child = left_child + 1;
				if (right_child < size() && !comp(_arr[right_child], _arr[left_child])) {
					next = right_child;
				}
				if (!comp(_arr[index], _arr[next])) {
					break;
				}
				std::swap(_arr[next], _arr[index]);
				_notify_index_change(index);
				index = next;
			}
			_notify_index_change(index);
			_verify_integrity();
			return index;
		}

		/// For debugging purposes: checks that this heap is valid.
		void _verify_integrity() const {
			Comp comp;
			for (std::size_t i = 1; i < _arr.size(); ++i) {
				assert_true_logical(!comp(_arr[_parent(i)], _arr[i]), "heap integrity compromised");
			}
		}
	};
}
