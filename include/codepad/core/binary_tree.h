// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of a generic binary tree.

#include <utility>
#include <stack>
#include <functional>

#include "misc.h"
#include "assert.h"

namespace codepad {
	/// A struct used to specify that no additional data is stored in the nodes of a \ref binary_tree.
	struct no_data {
		/// Empty. This function provides the interface necessary for using \ref default_synthesizer.
		template <typename Node> void synthesize(const Node&) {
		}
	};
	/// The default synthesizer used by \ref binary_tree. It simply calls \p %T::synthesize().
	///
	/// \tparam T Type of additional data.
	template <typename T> struct default_synthesizer {
		/// Synthesizes the additional data of \p node by calling \p %T::synthesize().
		///
		/// \tparam Node Type of tree node.
		/// \param node A \ref binary_tree_node.
		template <typename Node> void operator()(Node &&node) const {
			T::synthesize(std::forward<Node>(node));
		}
	};
	/// A struct indicating that no default synthesizer is specified for a \ref binary_tree, and that all methods
	/// that rely on a default synthesizer should fail to instantiate.
	struct lacks_synthesizer {
	};
	/// A struct indicating that no synthesization should be performed during all \ref binary_tree operations. This
	/// differs from \ref lacks_synthesizer in that \ref lacks_synthesizer will force the user to specify another
	/// synthesizer.
	struct no_synthesizer {
		/// Empty. This function provides the interface necessary for \ref binary_tree.
		template <typename Node> void operator()(Node&&) const {
		}
	};
	/// Inserts or searches for a node in a tree as if it were a binary search tree.
	///
	/// \tparam Comp Comparison function used to compare the values held by nodes.
	template <typename Comp> struct bst_branch_selector {
	public:
		/// Used when inserting a node.
		///
		/// \tparam Node Type of tree node.
		/// \param cur A node already in the tree.
		/// \param inserting The node that's to be inserted.
		/// \return \p true if \p inserting is to be inserted into the left subtree of \p cur, \p false if otherwise.
		template <typename Node> bool select_insert(const Node &cur, const Node &inserting) const {
			return _comp(inserting.value, cur.value);
		}
		/// Used when searching for a node.
		///
		/// \tparam Node Type of tree node.
		/// \tparam U Type of the key (typically the same type as \p cur.value).
		/// \param cur A node in the tree.
		/// \param v The target value to search for.
		/// \return Indicates where to search next. 0 if \p cur is a valid result,
		///         -1 if the left subtree of \p cur should be searched,
		///         and 1 if the right subtree should be searched.
		template <typename Node, typename U> int select_find(const Node &cur, const U &v) const {
			if (_comp(v, cur.value)) {
				return -1;
			} else if (_comp(cur.value, v)) {
				return 1;
			}
			return 0;
		}
	protected:
		[[no_unique_address]] Comp _comp; ///< An instance of the comparison object.
	};


	/// Nodes of a \ref binary_tree.
	///
	/// \tparam T Type of \ref value.
	/// \tparam AdditionalData Type of \ref synth_data.
	/// \remark The design of \p AdditionalData has to ensure that for subtrees that consists of the same nodes
	///         but not necessarily in the same order, the values of this struct must be the same.
	template <typename T, typename AdditionalData = no_data> struct binary_tree_node {
		using value_type = T; ///< The type of \ref value.
		using synth_data_type = AdditionalData; ///< The type of \ref synth_data.

		/// Constructs a binary tree node, forwarding all its arguments to the constructor of \ref value.
		template <typename ...Args> explicit binary_tree_node(Args &&...args) : value(std::forward<Args>(args)...) {
		}
		/// No move construction. With all the pointers in the node, it's not logical to simply `move' or `copy' a
		/// node.
		binary_tree_node(binary_tree_node&&) = delete;
		/// No copy construction.
		binary_tree_node(const binary_tree_node&) = delete;
		/// No move assignment.
		binary_tree_node &operator=(binary_tree_node&&) = delete;
		/// No copy assignment.
		binary_tree_node &operator=(const binary_tree_node&) = delete;

		/// Returns the successor of a node.
		///
		/// \return The node's successor in the tree, or \p nullptr if there's none.
		binary_tree_node *next() const {
			if (right) {
				binary_tree_node *res = right;
				for (; res->left; res = res->left) {
				}
				return res;
			}
			const binary_tree_node *res = this;
			for (; res->parent && res == res->parent->right; res = res->parent) {
			}
			return res->parent;
		}
		/// Returns the predecessor of a node.
		///
		/// \return The node's predecessor in the tree, or \p nullptr if there's none.
		binary_tree_node *prev() const {
			if (left) {
				binary_tree_node *res = left;
				for (; res->right; res = res->right) {
				}
				return res;
			}
			const binary_tree_node *res = this;
			for (; res->parent && res == res->parent->left; res = res->parent) {
			}
			return res->parent;
		}

		T value; ///< Data held by the node.
		/// Additional data that's synthesized based on \ref value and (possibly) the values of the node's children.
		AdditionalData synth_data;
		binary_tree_node
			*left = nullptr, ///< Pointer to the left subtree of the node.
			*right = nullptr, ///< Pointer to the right subtree of the node.
			*parent = nullptr; ///< Pointer to the parent of the node.
	};
	/// A bare-bones binary tree without any balancing or default node ordering.
	///
	/// \tparam T Type of the data held by the tree's nodes.
	/// \tparam AdditionalData Type of additional data held by the nodes.
	/// \tparam Synth Provides operator() to calculate \ref binary_tree_node::synth_data. If this is
	///               \ref lacks_synthesizer, then there will be no default synthesizer and all methods that make use
	///               of it will be disabled.
	/// \sa binary_tree_node
	template <
		typename T, typename AdditionalData = no_data, typename Synth = default_synthesizer<AdditionalData>
	> struct binary_tree {
	public:
		using node = binary_tree_node<T, AdditionalData>; ///< The type of tree nodes.
		template <bool> struct iterator_base;
		/// Helper class to ensure that \ref binary_tree_node::synth_data of nodes with modified values are updated
		/// properly, by automatically calling \ref refresh_synthesized_result(binary_tree_node*) in the destructor.
		///
		/// \remark The lifetime of the synthesizer must be longer than that of this struct, and the synthesizer must
		///         not be moved.
		template <typename MySynth = Synth> struct node_value_modifier {
		public:
			/// Default constructor that initializes the struct to empty.
			node_value_modifier() = default;
			/// Initializes the struct to modify the given node with the given synthesizer.
			node_value_modifier(node &n, MySynth &synth) : _n(&n), _synth(&synth) {
			}
			/// Default copy constructor.
			node_value_modifier(const node_value_modifier&) = default;
			/// Move constructor.
			node_value_modifier(node_value_modifier &&mod) noexcept :
				_n(std::exchange(mod._n, nullptr)), _synth(std::exchange(mod._synth, nullptr)) {
			}
			/// Calls \ref manual_refresh() before copying.
			node_value_modifier &operator=(const node_value_modifier &mod) {
				manual_refresh();
				_n = mod._n;
				_synth = mod._synth;
				return *this;
			}
			/// Calls \ref manual_refresh() before moving the contents.
			node_value_modifier &operator=(node_value_modifier &&mod) noexcept {
				manual_refresh();
				_n = std::exchange(mod._n, nullptr);
				_synth = std::exchange(mod._synth, nullptr);
				return *this;
			}
			/// Calls \ref manual_refresh() to update the synthesized values in the tree.
			~node_value_modifier() {
				manual_refresh();
			}

			/// Returns a reference to the node's value for modification.
			T &operator*() const {
				return _n->value;
			}
			/// Returns a pointer to the node's value for modification.
			T *operator->() const {
				return &operator*();
			}

			/// Calls \ref refresh_synthesized_result to update the synthesized values if this
			/// \ref node_value_modifier is valid.
			void manual_refresh() {
				if (_synth) {
					// if _synth is nonempty, then _n should be nonempty too
					assert_true_logical(_n != nullptr, "invalid modifier");
					refresh_synthesized_result(_n, *_synth);
				}
			}
		protected:
			node *_n = nullptr; ///< Pointer to the \ref binary_tree_node that the user intends to modify.
			MySynth *_synth = nullptr; ///< Pointer to the synthesizer.
		};
		/// Template of const and non-const iterators.
		///
		/// \tparam Const \p true for \ref const_iterator, \p false for \ref iterator.
		template <bool Const> struct iterator_base {
			friend binary_tree<T, AdditionalData, Synth>;
		public:
			using value_type = T;

			using tree_type = binary_tree<T, AdditionalData, Synth>;
			using container_type = std::conditional_t<Const, const tree_type, tree_type>;

			/// Default constructor.
			iterator_base() = default;
			/// Converting constructor from non-const iterator to const iterator.
			///
			/// \param it A non-const iterator.
			iterator_base(const iterator_base<false> &it) noexcept :
				_con(it._con), _n(it._n) {
			}

			/// Equality - compares all fields directly.
			friend bool operator==(const iterator_base&, const iterator_base&) = default;

			/// Prefix increment.
			[[nodiscard]] iterator_base &operator++() {
				assert_true_logical(_n != nullptr, "cannot increment iterator");
				_n = _n->next();
				return *this;
			}
			/// Postfix increment.
			[[nodiscard]] const iterator_base operator++(int) {
				iterator_base ov = *this;
				++*this;
				return ov;
			}
			/// Prefix decrement.
			[[nodiscard]] iterator_base &operator--() {
				if (_n) {
					_n = _n->prev();
					assert_true_logical(_n != nullptr, "cannot decrement iterator");
				} else {
					_n = _con->max();
				}
				return *this;
			}
			/// Postfix decrement.
			[[nodiscard]] const iterator_base operator--(int) {
				iterator_base ov = *this;
				--*this;
				return ov;
			}

			/// Used to bypass \ref node_value_modifier and modify the value of the node.
			/// The caller is responsible of calling \ref refresh_synthesized_result(binary_tree_node*) afterwards.
			///
			/// \return A reference to \ref binary_tree_node::value.
			[[nodiscard]] T &get_value_rawmod() const {
				return _n->value;
			}
			/// Returns the readonly value of the node.
			[[nodiscard]] const T &get_value() const {
				return _n->value;
			}
			/// Returns a corresponding \ref node_value_modifier.
			[[nodiscard]] node_value_modifier<Synth> get_modifier() const {
				return _con->get_modifier_for(_n);
			}
			/// Returns the readonly value of the node.
			[[nodiscard]] const T &operator*() const {
				return get_value();
			}
			/// Returns the readonly value of the node.
			[[nodiscard]] const T *operator->() const {
				return &get_value();
			}

			/// Returns the underlying node.
			[[nodiscard]] node *get_node() const {
				return _n;
			}
			/// Returns the tree that this iterator belongs to.
			[[nodiscard]] container_type *get_container() const {
				return _con;
			}
		protected:
			/// Protected constructor that ensures
			/// all non-empty iterator are obtained from \ref binary_tree "binary_trees".
			iterator_base(container_type *c, node *n) : _con(c), _n(n) {
			}

			container_type *_con = nullptr; ///< The container that \ref _n belongs to, or \p nullptr.
			node *_n = nullptr; ///< The underlying node.
		};
		using iterator = iterator_base<false>; ///< Iterator.
		using const_iterator = iterator_base<true>; ///< Const iterator.
		using reverse_iterator = std::reverse_iterator<iterator>; ///< Reverse iterator.
		using const_reverse_iterator = std::reverse_iterator<iterator>; ///< Const reverse iterator.


		/// Default constructor.
		binary_tree() = default;
		/// Creates an empty tree with the given synthesizer.
		explicit binary_tree(Synth synth) : _synth(std::move(synth)) {
		}
		/// Initializes \ref _synth_storage::synth with \p args, then builds the tree with the given range of
		/// objects.
		///
		/// \tparam Cont Container type.
		/// \param beg Iterator to the first object.
		/// \param end Iterator past the last object.
		/// \param args Arguments used to construct \ref _root (and thus \ref _synth_storage::synth).
		template <typename It, typename ...Args> binary_tree(It &&beg, It &&end, Args &&...args) :
			_synth(std::forward<Args>(args)...) {
			_root = build_tree_copy(std::forward<It>(beg), std::forward<It>(end), _synth);
		}
		/// Copy constructor. Clones the tree.
		binary_tree(const binary_tree &tree) : _synth(tree._synth), _root(clone_tree(tree._root)) {
		}
		/// Move constructor. Moves the root pointer.
		binary_tree(binary_tree &&tree) noexcept :
			_synth(std::move(tree._synth)), _root(std::exchange(tree._root, nullptr)) {
		}
		/// Copy assignment. Clones the tree after deleting the old one.
		binary_tree &operator=(const binary_tree &tree) {
			delete_tree(_root);
			_synth = tree._synth;
			_root = clone_tree(tree._root);
			return *this;
		}
		/// Move assignment. Exchanges the \ref _root of two trees.
		binary_tree &operator=(binary_tree &&tree) noexcept {
			delete_tree(_root);
			_synth = std::move(tree._synth);
			_root = std::exchange(tree._root, nullptr);
			return *this;
		}
		/// Destructor. Calls delete_tree(binary_tree_node*) to dispose of all nodes.
		~binary_tree() {
			delete_tree(_root);
		}


		/// Returns an iterator for the given node.
		///
		/// \return An iterator for the given node \p n.
		iterator get_iterator_for(node *n) {
			return iterator(this, n);
		}
		/// Const version of get_iterator_for(node*).
		const_iterator get_iterator_for(node *n) const {
			return const_iterator(this, n);
		}
		/// Explicit version of get_iterator_for(node*) const.
		const_iterator get_const_iterator_for(node *n) const {
			return const_iterator(this, n);
		}

		/// Returns an iterator to the leftmost node in the tree.
		///
		/// \return Iterator to the leftmost node in the tree.
		iterator begin() {
			return iterator(this, min());
		}
		/// Const version of begin().
		const_iterator begin() const {
			return cbegin();
		}
		/// Explicit version of begin() const.
		const_iterator cbegin() const {
			return const_iterator(this, min());
		}
		/// Returns an iterator past the rightmost node of the tree.
		///
		/// \return Iterator past the rightmost node of the tree.
		iterator end() {
			return iterator(this, nullptr);
		}
		/// Const version of end().
		const_iterator end() const {
			return cend();
		}
		/// Explicit version of end() const.
		const_iterator cend() const {
			return const_iterator(this, nullptr);
		}
		/// Returns an iterator to the rightmost node of the tree.
		///
		/// \return Iterator to the rightmost node of the tree.
		reverse_iterator rbegin() {
			return reverse_iterator(end());
		}
		/// Const version of rbegin().
		const_reverse_iterator rbegin() const {
			return crbegin();
		}
		/// Explicit version of rbegin() const.
		const_reverse_iterator crbegin() const {
			return reverse_iterator(end());
		}
		/// Returns an iterator past the leftmost node of the tree.
		///
		/// \return Iterator past the leftmost node of the tree.
		reverse_iterator rend() {
			return reverse_iterator(begin());
		}
		/// Const version of rend().
		const_reverse_iterator rend() const {
			return crend();
		}
		/// Explicit version of rend() const.
		const_reverse_iterator crend() const {
			return reverse_iterator(begin());
		}


		/// Inserts a node into the tree with a custom synthesizer. Creates a \ref binary_tree_node using the
		/// provided arguments, then travels from the root downwards, using a branch selector to determine where to
		/// go.
		///
		/// \param d The branch selector. See \ref bst_branch_selector for an example.
		/// \param synth The custom synthesizer.
		/// \param args Arguments used to initialize the new node.
		template <typename BranchSelector, typename MySynth, typename ...Args> iterator select_insert(
			BranchSelector &&d, MySynth &&synth, Args &&...args
		) {
			node *n = new node(std::forward<Args>(args)...), *prev = nullptr, **pptr = &_root;
			while (*pptr) {
				prev = *pptr;
				pptr = d.select_insert(*prev, *n) ? &prev->left : &prev->right;
			}
			*pptr = n;
			n->parent = prev;
			refresh_synthesized_result(n, std::forward<MySynth>(synth));
			return iterator(this, n);
		}

		/// Inserts a node or a subtree before a node with a custom synthesizer. After the insertion, the predecesor
		/// of \p before will be the rightmost element of the subtree. If \p before is \p nullptr, \p n is inserted
		/// at the end of the tree. The node or subtree to insert is assumed to have valid synthesized values.
		///
		/// \param before Indicates where the new subtree is to be inserted.
		/// \param n The root of the subtree.
		/// \param synth The custom synthesizer.
		template <typename MySynth> void insert_before(node *before, node *n, MySynth &&synth) {
			if (n == nullptr) {
				return; // no insertion needed
			}
			if (before == nullptr) {
				if (_root) {
					before = max();
					before->right = n;
				} else { // empty tree
					_root = n;
				}
			} else if (before->left) {
				for (before = before->left; before->right; before = before->right) {
				}
				// before is now the predecessor of the previous before
				before->right = n;
			} else {
				before->left = n;
			}
			n->parent = before;
			refresh_synthesized_result(before, std::forward<MySynth>(synth));
		}
		/// \ref insert_before(node*, node*, MySynth&&) with the default synthesizer.
		void insert_before(node *before, node *n) {
			insert_before(before, n, _synth);
		}


		/// Inserts a range of nodes before the given node, with a custom synthesizer. The items are copied from the
		/// given objects.
		template <typename It, typename MySynth> void insert_range_before_copy(
			node *before, It &&beg, It &&end, MySynth &&synth
		) {
			insert_before(before, build_tree_copy(
				std::forward<It>(beg), std::forward<It>(end), std::forward<MySynth>(synth)
			), std::forward<MySynth>(synth));
		}
		/// \ref insert_range_before_copy(node*, It&&, It&&, MySynth&&) with the default synthesizer.
		template <typename It> void insert_range_before_copy(node *before, It &&beg, It &&end) {
			insert_range_before_copy(before, std::forward<It>(beg), std::forward<It>(end), _synth);
		}
		/// \overload
		template <typename ...Args> void insert_range_before_copy(const_iterator before, Args &&...args) {
			insert_range_before_copy(before.get_node(), std::forward<Args>(args)...);
		}
		/// Inserts a range of nodes before the given node, with a custom synthesizer. The items are moved from the
		/// given objects.
		template <typename It, typename MySynth> void insert_range_before_move(
			node *before, It &&beg, It &&end, MySynth &&synth
		) {
			insert_before(before, build_tree_move(
				std::forward<It>(beg), std::forward<It>(end), std::forward<MySynth>(synth)
			), std::forward<MySynth>(synth));
		}
		/// \ref insert_range_before_move(node*, It&&, It&&, MySynth&&) with the default synthesizer.
		template <typename It> void insert_range_before_move(node *before, It &&beg, It &&end) {
			insert_range_before_move(before, std::forward<It>(beg), std::forward<It>(end), _synth);
		}
		/// \overload
		template <typename ...Args> void insert_range_before_move(const_iterator before, Args &&...args) {
			insert_range_before_move(before.get_node(), std::forward<Args>(args)...);
		}

		/// Inserts a node before the given node, with a custom synthesizer. The value of the node (i.e.,
		/// \ref binary_tree_node::value) is constructed in-place.
		template <typename MySynth, typename ...Args> iterator emplace_before_custom_synth(
			node *before, MySynth &&synth, Args &&...args
		) {
			node *n = new node(std::forward<Args>(args)...);
			synth(*n);
			insert_before(before, n, std::forward<MySynth>(synth));
			return get_iterator_for(n);
		}
		/// \overload
		template <typename ...Args> iterator emplace_before_custom_synth(const_iterator before, Args &&...args) {
			return emplace_before_custom_synth(before.get_node(), std::forward<Args>(args)...);
		}
		/// \ref emplace_before_custom_synth() with the default synthesizer.
		template <typename ...Args> iterator emplace_before(node *before, Args &&...args) {
			return emplace_before_custom_synth(before, _synth, std::forward<Args>(args)...);
		}
		/// \overload
		template <typename ...Args> iterator emplace_before(const_iterator before, Args &&...args) {
			return emplace_before(before.get_node(), std::forward<Args>(args)...);
		}

		/// Inserts a node before the given node, with a custom synthesizer.
		template <typename MySynth> iterator insert_before(
			node *before, typename node::value_type val, MySynth &&synth
		) {
			emplace_before_custom_synth(before, std::forward<MySynth>(synth), std::move(val));
		}
		/// \ref insert_before(node*, typename node::value_type, MySynth&&) with the default synthesizer.
		iterator insert_before(node *before, typename node::value_type val) {
			return insert_before(before, std::move(val), _synth);
		}
		/// \overload
		template <typename ...Args> iterator insert_before(const_iterator before, Args &&...args) {
			return insert_before(before.get_node(), std::forward<Args>(args)...);
		}


		/// Finds a node using a branch selector and a reference.
		/// The reference may be non-const to provide additional feedback.
		///
		/// \param b A branch selector. See \ref bst_branch_selector for an example.
		/// \param ref The reference for finding the target node.
		/// \return The resulting iterator, or end() if none qualifies.
		template <typename BranchSelector, typename Ref> iterator find(BranchSelector &&b, Ref &&ref) {
			return get_iterator_for(find_custom(_root, std::forward<BranchSelector>(b), std::forward<Ref>(ref)));
		}
		/// Const version of find_custom(BranchSelector&&, Ref&&).
		template <typename BranchSelector, typename Ref> const_iterator find(
			BranchSelector &&b, Ref &&ref
		) const {
			return get_iterator_for(find_custom(_root, std::forward<BranchSelector>(b), std::forward<Ref>(ref)));
		}


		/// Update the synthesized values of all nodes on the path from \p n to the root with a custom synthesizer.
		template <typename MySynth> inline static void refresh_synthesized_result(node *n, MySynth &&synth) {
			for (; n; n = n->parent) {
				synth(*n);
			}
		}
		/// \ref refresh_synthesized_result(node*, MySynth&&) that uses the default synthesizer.
		void refresh_synthesized_result(node *n) {
			refresh_synthesized_result(n, _synth);
		}

	protected:
		/// Enumeration for recording node status when traversing the tree.
		enum class _traverse_status {
			not_visited, ///< The node has not been visited.
			visited_left, ///< The node's left subtree has been visited.
			visited_right ///< The node's right subtree has been visited.
		};
	public:
		/// Update the synthesized values of all nodes in the tree with a custom synthesizer.
		template <typename MySynth> void refresh_tree_synthesized_result(MySynth &&synth) const {
			std::stack<std::pair<node*, _traverse_status>> stk;
			if (_root) {
				stk.emplace(_root, _traverse_status::not_visited);
			}
			while (!stk.empty()) { // nodes are visited in a DFS-like fashion
				std::pair<node*, _traverse_status> &p = stk.top();
				switch (p.second) {
				case _traverse_status::not_visited:
					if (p.first->left) { // visit left subtree first if there is one
						p.second = _traverse_status::visited_left;
						stk.emplace(p.first->left, _traverse_status::not_visited);
						break;
					}
					[[fallthrough]]; // otherwise
				case _traverse_status::visited_left:
					if (p.first->right) { // visit right subtree first if there is one
						p.second = _traverse_status::visited_right;
						stk.emplace(p.first->right, _traverse_status::not_visited);
						break;
					}
					[[fallthrough]]; // otherwise
				case _traverse_status::visited_right:
					synth(*p.first);
					stk.pop();
					break;
				}
			}
		}
		/// \ref refresh_tree_synthesized_result(MySynth&&) with the default synthesizer.
		void refresh_tree_synthesized_result() {
			refresh_tree_synthesized_result(_synth);
		}

		/// Synthesize certain values from a node to the root.
		///
		/// \param n The node to start with.
		/// \param v An object whose %operator() is called at every node with the current node and its parent. Note
		///          that this is different from normal synthesizers used in trees.
		template <typename NewSynth> void synthesize_root_path(const node *n, NewSynth &&v) const {
			if (n) {
				for (node *p = n->parent; p; n = p, p = p->parent) {
					v(*p, *n);
				}
			}
		}

		/// Right rotation, with the synthesized data updated by a custom synthesizer.
		///
		/// \param n The root of the subtree before rotation. \p n->left will be in its place after.
		/// \param synth The custom synthesizer.
		/// \remark This method may be made protected when red-black tree is used.
		template <typename MySynth> void rotate_right(node *n, MySynth &&synth) {
			assert_true_logical(n->left != nullptr, "cannot perform rotation");
			if (n->parent) {
				(n == n->parent->left ? n->parent->left : n->parent->right) = n->left;
			} else {
				assert_true_logical(_root == n, "corrupted tree structure");
				_root = n->left;
			}
			n->left->parent = n->parent;
			n->parent = n->left;
			n->left = n->left->right;
			n->parent->right = n;
			if (n->left) {
				n->left->parent = n;
			}
			n->parent->synth_data = n->synth_data;
			synth(*n);
		}
		/// Left rotation, with the synthesized data updated by a custom synthesizer.
		///
		/// \param n The root of the subtree before rotation. \p n->right will be in its place after.
		/// \param synth The custom synthesizer.
		/// \remark This method may be made protected when red-black tree is used.
		template <typename MySynth> void rotate_left(node *n, MySynth &&synth) {
			assert_true_logical(n->right != nullptr, "cannot perform rotation");
			if (n->parent) {
				(n == n->parent->left ? n->parent->left : n->parent->right) = n->right;
			} else {
				assert_true_logical(_root == n, "corrupted tree structure");
				_root = n->right;
			}
			n->right->parent = n->parent;
			n->parent = n->right;
			n->right = n->right->left;
			n->parent->left = n;
			if (n->right) {
				n->right->parent = n;
			}
			n->parent->synth_data = n->synth_data;
			synth(*n);
		}
		/// Splays the node until its parent is another designated node, using a custom synthesizer.
		///
		/// \param n The node to bring up.
		/// \param targetroot The opration stops when it is the parent of \p n.
		/// \param synth The synthesizer used.
		/// \remark This method may be removed when red-black tree is used.
		template <typename MySynth> void splay(node *n, node *targetroot, MySynth &&synth) {
			while (n->parent != targetroot) {
				if (
					n->parent->parent != targetroot &&
					(n == n->parent->left) == (n->parent == n->parent->parent->left)
					) {
					if (n == n->parent->left) {
						rotate_right(n->parent->parent, std::forward<MySynth>(synth));
						rotate_right(n->parent, std::forward<MySynth>(synth));
					} else {
						rotate_left(n->parent->parent, std::forward<MySynth>(synth));
						rotate_left(n->parent, std::forward<MySynth>(synth));
					}
				} else {
					if (n == n->parent->left) {
						rotate_right(n->parent, std::forward<MySynth>(synth));
					} else {
						rotate_left(n->parent, std::forward<MySynth>(synth));
					}
				}
			}
		}

		/// Removes the given node from the tree and deletes it, using a custom synthesizer.
		///
		/// \return The node after the erased one.
		template <typename MySynth> node *erase(node *n, MySynth &&synth) {
			if (n == nullptr) {
				return nullptr;
			}
			node *oc, *next = n->next();
			if (n->left && n->right) {
				node *rmin = min(n->right);
				splay(rmin, n, std::forward<MySynth>(synth));
				rotate_left(n, std::forward<MySynth>(synth));
				oc = n->left;
			} else {
				oc = n->left ? n->left : n->right;
			}
			if (_root == n) {
				_root = oc;
			} else {
				(n == n->parent->left ? n->parent->left : n->parent->right) = oc;
			}
			node *f = n->parent;
			if (oc) {
				oc->parent = f;
			}
			delete n;
			refresh_synthesized_result(f, std::forward<MySynth>(synth));
			return next;
		}
		/// \ref erase(node*, MySynth&&) with the default synthesizer.
		node *erase(node *n) {
			return erase(n, _synth);
		}
		/// \overload
		template <typename ...Args> iterator erase(const_iterator it, Args &&...synth) {
			return get_iterator_for(erase(it.get_node(), std::forward<Args>(synth)...));
		}

		/// Removes a range of nodes from the tree, but doesn't delete the nodes. This function uses a custom
		/// synthesizer.
		///
		/// \param beg The first node to remove.
		/// \param end The node after the last node to remove. \p nullptr to indicate the last node.
		/// \param synth The synthesizer.
		/// \return The root of the removed subtree.
		/// \remark This method may be removed when red-black tree is used.
		template <typename MySynth> node *detach_tree(node *beg, node *end, MySynth &&synth) {
			if (beg == nullptr) {
				return nullptr;
			}
			beg = beg->prev();
			node *res = nullptr;
			if (beg && end) {
				splay(beg, nullptr, std::forward<MySynth>(synth));
				splay(end, beg, std::forward<MySynth>(synth));
				assert_true_logical(end == beg->right, "invalid range");
				res = end->left;
				end->left = nullptr;

				synth(*end);
				synth(*beg);
			} else if (beg) {
				splay(beg, nullptr, std::forward<MySynth>(synth));
				res = beg->right;
				beg->right = nullptr;

				synth(*beg);
			} else if (end) {
				splay(end, nullptr, std::forward<MySynth>(synth));
				res = end->left;
				end->left = nullptr;

				synth(*end);
			} else {
				res = _root;
				_root = nullptr;
			}
			if (res) {
				res->parent = nullptr;
			}
			return res;
		}

		/// Removes a range of nodes from the tree and deletes them, using a custom synthesizer.
		///
		/// \param beg The first node to remove.
		/// \param end The node after the last node to remove. \p nullptr to indicate the last node.
		/// \param synth The custom synthesizer.
		template <typename MySynth> void erase(node *beg, node *end, MySynth &&synth) {
			delete_tree(detach_tree(beg, end, std::forward<MySynth>(synth)));
		}
		/// \ref erase(node*, node*, MySynth&&) with the default synthesizer.
		void erase(node *beg, node *end) {
			erase(beg, end, _synth);
		}
		/// \overload
		template <typename ...Args> void erase(const_iterator beg, const_iterator end, Args &&...synth) {
			erase(beg.get_node(), end.get_node(), std::forward<Args>(synth)...);
		}

		/// Returns a \ref node_value_modifier corresponding to the given node, with the default synthesizer.
		node_value_modifier<Synth> get_modifier_for(node *n) {
			return node_value_modifier<Synth>(*n, _synth);
		}

		/// Checks the integrity of the given tree. Asserts if there are any incorrect pointers.
		inline static void check_integrity(const node *rt) {
			if (rt == nullptr) {
				return;
			}
			assert_true_logical(rt->parent == nullptr, "root should not have a parent");
			std::stack<const node*> ns;
			ns.emplace(rt);
			while (!ns.empty()) {
				const node *n = ns.top();
				ns.pop();
				if (n->left) {
					assert_true_logical(n->left->parent == n, "left child has incorrect parent");
					ns.emplace(n->left);
				}
				if (n->right) {
					assert_true_logical(n->right->parent == n, "left child has incorrect parent");
					ns.emplace(n->right);
				}
			}
		}
		/// Checks the integrity of this tree.
		void check_integrity() const {
			check_integrity(root());
		}

		/// Returns the root of the tree.
		node *root() const {
			return _root;
		}
		/// Returns a reference to the root pointer.
		node *&mutable_root() {
			return _root;
		}
		/// Returns the leftmost element of the tree, or \p nullptr if the tree's empty.
		node *min() const {
			return min(root());
		}
		/// Returns the rightmost element of the tree, or \p nullptr if the tree's empty.
		node *max() const {
			return max(root());
		}
		/// Returns whether the tree is empty.
		bool empty() const {
			return _root == nullptr;
		}
		/// Deletes all nodes in the tree, and resets \ref _root_t::ptr of \ref _root to \p nullptr.
		void clear() {
			delete_tree(_root);
			_root = nullptr;
		}

		/// Replaces \ref _root_t::synth with the given synthesizer.
		template <typename SynRef> void set_synthesizer(SynRef &&s) {
			_synth = std::forward<SynRef>(s);
		}
		/// Returns the current synthesizer.
		Synth &get_synthesizer() {
			return _synth;
		}
		/// Const version of \ref get_synthesizer().
		const Synth &get_synthesizer() const {
			return _synth;
		}


		/// Returns the leftmost element of a tree whose root is \p n, or \p nullptr.
		inline static node *min(node *n) {
			for (; n && n->left; n = n->left) {
			}
			return n;
		}
		/// Returns the rightmost element of a tree whose root is \p n, or \p nullptr.
		inline static node *max(node *n) {
			for (; n && n->right; n = n->right) {
			}
			return n;
		}


		/// Clones a tree.
		///
		/// \param n The root of the tree to clone.
		/// \return The root of the new tree.
		inline static node *clone_tree(const node *n) {
			if (n == nullptr) {
				return nullptr;
			}
			std::stack<_clone_node> stk;
			node *res = nullptr;
			stk.emplace(n, nullptr, &res);
			do {
				_clone_node curv = stk.top();
				stk.pop();
				auto *cn = new node(curv.src->value);
				// copy synth data directly because we're replicating the tree structure exactly
				cn->synth_data = curv.src->synth_data;
				cn->parent = curv.parent;
				*curv.assign = cn;
				if (curv.src->left) {
					stk.emplace(curv.src->left, cn, &cn->left);
				}
				if (curv.src->right) {
					stk.emplace(curv.src->right, cn, &cn->right);
				}
			} while (!stk.empty());
			return res;
		}
		/// Deletes all nodes in a tree.
		///
		/// \param n The root of the tree to delete.
		inline static void delete_tree(node *n) {
			if (n == nullptr) {
				return;
			}
			std::stack<node*> ns;
			ns.emplace(n);
			do {
				node *c = ns.top();
				ns.pop();
				if (c->left) {
					ns.emplace(c->left);
				}
				if (c->right) {
					ns.emplace(c->right);
				}
				delete c;
			} while (!ns.empty());
		}
		/// Builds a tree from an array of objects. Objects are copied from the container.
		///
		/// \param beg Iterator to the first element.
		/// \param end Iterator past the last element.
		/// \param synth The synthesizer used.
		/// \return The root of the newly built tree.
		template <typename It, typename SynRef> inline static node *build_tree_copy(It &&beg, It &&end, SynRef &&synth) {
			return _build_tree<_copy_value>(
				std::forward<It>(beg), std::forward<It>(end), std::forward<SynRef>(synth)
				);
		}
		/// Builds a tree from an array of objects. Objects are moved out of the container.
		///
		/// \param beg Iterator to the first element.
		/// \param end Iterator past the last element.
		/// \param synth The synthesizer used.
		/// \return The root of the newly built tree.
		template <typename It, typename SynRef> inline static node *build_tree_move(It &&beg, It &&end, SynRef &&synth) {
			return _build_tree<_move_value>(
				std::forward<It>(beg), std::forward<It>(end), std::forward<SynRef>(synth)
				);
		}
		/// Searches in a tree for a matching node using a branch selector.
		///
		/// \param root The root node of the tree.
		/// \param b The branch selector.
		/// \param ref The reference value.
		/// \return The resulting node.
		template <typename B, typename Ref> inline static node *find_custom(node *root, B &&b, Ref &&ref) {
			node *cur = root;
			while (cur) {
				switch (b.select_find(*cur, std::forward<Ref>(ref))) {
				case -1:
					cur = cur->left;
					break;
				case 0:
					return cur;
				case 1:
					cur = cur->right;
					break;
				}
			}
			return cur;
		}
	protected:
		/// The synthesizer. This is put before \ref _root because of the (very very slim) chance that the root
		/// pointer can fit in the tail padding of this object.
		[[no_unique_address]] Synth _synth;
		node *_root = nullptr; ///< The root pointer.


		/// Auxiliary struct used when cloning a tree.
		struct _clone_node {
			/// Default constructor.
			_clone_node() = default;
			/// Constructor that initializes all fields of this struct.
			_clone_node(const node *s, node *p, node **a) : src(s), parent(p), assign(a) {
			}

			const node *src = nullptr; ///< The source node; the corresponding node in the tree to be cloned.
			node
				*parent = nullptr, ///< The cloned node's parent.
				**assign = nullptr; ///< \ref parent's pointer to the cloned node, or a pointer to the root pointer.
		};


		/// Struct that copies values from iterators to initialize nodes.
		struct _copy_value {
			/// Interface for \ref _build_tree. Allocates a new node from the given iterator.
			///
			/// \param it The iterator to copy value from.
			/// \return A pointer to the newly created node.
			template <typename It> inline static node *get(It it) {
				return new node(*it);
			}
		};
		/// Struct that moves values from iterators to initialize nodes.
		struct _move_value {
			/// \sa _copy_value::get
			template <typename It> inline static node *get(It it) {
				return new node(std::move(*it));
			}
		};

		/// Builds a tree from a range of elements using the designated synthesizer and construct operation.
		///
		/// \tparam Op Either \ref _copy_value or \ref _move_value. Indicates whether values are copied or moved.
		/// \param beg Iterator to the first element.
		/// \param end Iterator past the last element.
		/// \param s The synthesizer used.
		template <typename Op, typename It, typename SynRef> inline static node *_build_tree(
			It beg, It end, SynRef &&synth
		) {
			if (beg == end) {
				return nullptr;
			}
			auto half = beg + (end - beg) / 2;
			// recursively call _build_tree
			node
				*left = _build_tree<Op, It, SynRef>(beg, half, std::forward<SynRef>(synth)),
				*right = _build_tree<Op, It, SynRef>(half + 1, end, std::forward<SynRef>(synth)),
				*cur = Op::get(half);
			cur->left = left;
			cur->right = right;
			if (cur->left) {
				cur->left->parent = cur;
			}
			if (cur->right) {
				cur->right->parent = cur;
			}
			synth(*cur);
			return cur;
		}
	};

	/// Helper structs to simplify the use of \ref binary_tree.
	namespace synthesization_helper {
		/// Indicates that the property is stored in a field of \ref binary_tree_node::value.
		///
		/// \tparam Prop Member pointer to the field.
		template <auto Prop> struct field_value_property {
			/// Getter interface for propeties.
			///
			/// \return The retrieved value.
			template <typename Node> inline static auto get(Node &&n) {
				return n.value.*Prop;
			}
		};
		/// Indicates that the property can be retrieved by calling a method of binary_tree_node::value.
		///
		/// \tparam Prop Member pointer to the function.
		template <auto Prop> struct func_value_property {
			/// Getter interface for properties.
			///
			/// \return The return value of the function.
			template <typename Node> inline static auto get(Node &&n) {
				return (n.value.*Prop)();
			}
		};

		/// Auxiliary property of nodes that always returns 1. Useful for obtaining tree sizes.
		struct identity {
			/// Returns 1.
			///
			/// \tparam The type of the node.
			template <typename Node> inline static std::size_t get(const Node&) {
				return 1;
			}
		};
	}

	/// Synthesizer related structs that treat the data as block sizes,
	/// i.e., each node represents an array of objects after the previous one.
	/// The representation can be the actual objects and/or certain statistics of them.
	namespace sum_synthesizer {
		/// Represents a property of binary_tree_node::synth_data,
		/// that records the statistics of a single node and the whole subtree.
		///
		/// \tparam GetForNode A property of binary_tree_node::value, similar to the ones in
		///                    \ref synthesization_helper.
		/// \tparam NodeVal Member pointer to the statistics for a single node.
		/// \tparam TreeVal Member pointer to the statistics for a subtree.
		template <typename GetForNode, auto NodeVal, auto TreeVal> struct property {
			static_assert(
				std::is_same_v<
				std::decay_t<typename member_pointer_traits<decltype(NodeVal)>::value_type>,
				std::decay_t<typename member_pointer_traits<decltype(TreeVal)>::value_type>
				>, "incoherent value types"
				);

			/// The data type of the statistics, deducted from the member pointers.
			using value_type = std::decay_t<typename member_pointer_traits<decltype(NodeVal)>::value_type>;

			/// Returns the statistics obtained directly from the node.
			///
			/// \return The statistics obtained directly from the node.
			template <typename Node> inline static value_type get_node_value(Node &&n) {
				return GetForNode::get(std::forward<Node>(n));
			}
			/// Returns the statistics of a single node obtained from \ref binary_tree_node::synth_data.
			///
			/// \return The statistics of a single node obtained from \ref binary_tree_node::synth_data.
			template <typename Node> inline static value_type get_node_synth_value(Node &&n) {
				return n.synth_data.*NodeVal;
			}
			/// Sets the statistics of the node in \ref binary_tree_node::synth_data.
			template <typename Node> inline static void set_node_synth_value(Node &&n, value_type v) {
				n.synth_data.*NodeVal = std::move(v);
			}
			/// Returns the statistics of a subtree obtained from \ref binary_tree_node::synth_data.
			///
			/// \return The statistics of a subtree obtained from \ref binary_tree_node::synth_data.
			template <typename Node> inline static value_type get_tree_synth_value(Node &&n) {
				return n.synth_data.*TreeVal;
			}
			/// Sets the statistics of the subtree in \ref binary_tree_node::synth_data.
			template <typename Node> inline static void set_tree_synth_value(Node &&n, value_type v) {
				n.synth_data.*TreeVal = std::move(v);
			}
		};
		/// Represents a property of \ref binary_tree_node::synth_data that only stores the statistics of a subtree.
		///
		/// \tparam GetForNode A property of binary_tree_node::value, similar to the ones in
		///                    \ref synthesization_helper.
		/// \tparam TreeVal Member pointer to the statistics for a subtree.
		template <typename GetForNode, auto TreeVal> struct compact_property {
			/// The data type of the statistics, deducted from \p TreeVal.
			using value_type = std::decay_t<typename member_pointer_traits<decltype(TreeVal)>::value_type>;

			/// Returns the statistics obtained directly from the node.
			template <typename Node> inline static value_type get_node_value(Node &&n) {
				return GetForNode::get(std::forward<Node>(n));
			}
			/// Returns the statistics of a single node obtained from \ref binary_tree_node::synth_data.
			template <typename Node> inline static value_type get_node_synth_value(Node &&n) {
				return get_node_value(std::forward<Node>(n));
			}
			/// Does nothing because there's no corresponding field.
			template <typename Node> inline static void set_node_synth_value(Node&&, value_type) {
			}
			/// Returns the statistics of a subtree obtained from \ref binary_tree_node::synth_data.
			template <typename Node> inline static value_type get_tree_synth_value(Node &&n) {
				return n.synth_data.*TreeVal;
			}
			/// Sets the statistics of the subtree in \ref binary_tree_node::synth_data.
			template <typename Node> inline static void set_tree_synth_value(Node &&n, value_type v) {
				n.synth_data.*TreeVal = std::move(v);
			}
		};

		/// Helper functions for setting node values.
		namespace _details {
			/// End of recursion.
			template <typename Node> inline void set_node_values(Node&&) {
			}
			/// Sets the node values and tree values of \ref binary_tree_node::synth_data to the value
			/// obtained from \p get_node_value, for all properties in the template argument list.
			///
			/// \tparam FirstProp The first property.
			/// \tparam OtherProps Other properties that follow.
			template <typename FirstProp, typename ...OtherProps, typename Node> inline void set_node_values(Node &&n) {
				auto v = FirstProp::get_node_value(std::forward<Node>(n));
				FirstProp::set_node_synth_value(std::forward<Node>(n), v);
				FirstProp::set_tree_synth_value(std::forward<Node>(n), v);
				set_node_values<OtherProps...>(std::forward<Node>(n));
			}

			/// End of recursion.
			template <
				typename ParentNode, typename ChildNode
			> inline void add_subtree_values(ParentNode&&, ChildNode&&) {
			}
			/// Adds the tree values of \ref binary_tree_node::synth_data of \p sub to \p n for all properties.
			///
			/// \tparam FirstProp The first property.
			/// \tparam OtherProps Other properties that follow.
			/// \param n The node whose tree values are to be updated.
			/// \param sub The node that contains the desired difference of tree values.
			template <
				typename FirstProp, typename ...OtherProps, typename ParentNode, typename ChildNode
			> inline void add_subtree_values(ParentNode &&n, ChildNode &&sub) {
				FirstProp::set_tree_synth_value(
					std::forward<ParentNode>(n),
					FirstProp::get_tree_synth_value(std::forward<ParentNode>(n)) +
					FirstProp::get_tree_synth_value(std::forward<ChildNode>(sub))
				);
				add_subtree_values<OtherProps...>(std::forward<ParentNode>(n), std::forward<ChildNode>(sub));
			}

			/// End of recursion.
			template <typename Node> inline void add_synth_node_values(Node&&) {
			}
			/// Adds the node values of the given node to the given values for all properties.
			///
			/// \tparam FirstProp The first property.
			/// \tparam OtherProps Other properties that follow.
			/// \param n The target node.
			/// \param fv The first object that receives the node value of the first property.
			/// \param otvals Other objects, similar to \p fv, that follow.
			template <
				typename FirstProp, typename ...OtherProps, typename FirstVal, typename ...OtherVals, typename Node
			> inline void add_synth_node_values(Node &&n, FirstVal &&fv, OtherVals &&...otvals) {
				fv += FirstProp::get_node_synth_value(std::forward<Node>(n));
				add_synth_node_values<OtherProps...>(std::forward<Node>(n), std::forward<OtherVals>(otvals)...);
			}

			/// End of recursion.
			template <typename Node> inline void add_synth_tree_values(Node&&) {
			}
			/// Adds the tree values of the given node to the given values for all properties.
			///
			/// \tparam FirstProp The first property.
			/// \tparam OtherProps Other properties that follow.
			/// \param n The target node.
			/// \param fv The first object that receives the tree value of the first property.
			/// \param otvals Other objects, similar to \p fv, that follow.
			template <
				typename FirstProp, typename ...OtherProps, typename FirstVal, typename ...OtherVals, typename Node
			> inline void add_synth_tree_values(Node &&n, FirstVal &&fv, OtherVals &&...otvals) {
				fv += FirstProp::get_tree_synth_value(std::forward<Node>(n));
				add_synth_tree_values<OtherProps...>(std::forward<Node>(n), std::forward<OtherVals>(otvals)...);
			}
		}

		/// Used in \p additional_data::synthesize that accumulates the corresponding values for all properties.
		///
		/// \tparam Props All properties.
		/// \param n The node whose synthesize data is to be updated.
		template <typename ...Props, typename Node> inline void synthesize(Node &&n) {
			_details::set_node_values<Props...>(std::forward<Node>(n));
			if (n.left) {
				_details::add_subtree_values<Props...>(std::forward<Node>(n), *n.left);
			}
			if (n.right) {
				_details::add_subtree_values<Props...>(std::forward<Node>(n), *n.right);
			}
		}
		/// Finder that can be used with \ref binary_tree::find_custom. Finds a node such that
		/// a property of all nodes to its left accumulates to or below a certain value.
		/// For instance, suppose that each node stores an array of objects,
		/// and that these arrays may not have the same length, this finder can be used
		/// to find the node holding the k-th element.
		///
		/// \tparam Property The property used to perform the lookup.
		/// \tparam PreventOverflow If \p true, the result won't exceed the last element.
		/// \tparam Comp Comparison function used to decide which node to go to.
		///              Typically \p std::less or \p std::less_equal.
		template <
			typename Property, bool PreventOverflow = false, typename Comp = std::less<typename Property::value_type>
		> struct index_finder {
			/// Interface for \ref binary_tree::find_custom(). This function can also collect additional data
			/// when looking for the node, as specified by \p Props and \p avals.
			///
			/// \tparam Props Additional properties whose values will be accumulated in
			///         corresponding entries in \p avals.
			/// \param n The current node, provided by binary_tree::find_custom.
			/// \param target The target value.
			/// \param avals Variables to collect additional statistics. The caller must initialize these values
			///              properly.
			template <
				typename ...Props, typename ...AddVals, typename Node, typename V
			> inline static int select_find(Node &&n, V &target, AddVals &&...avals) {
				static_assert(sizeof...(Props) == sizeof...(AddVals), "wrong number of additional accumulators");
				Comp cmp;
				if (n.left) {
					auto lval = Property::get_tree_synth_value(*n.left);
					if (cmp(target, lval)) { // left branch
						return -1;
					}
					target -= lval;
					_details::add_synth_tree_values<Props...>(*n.left, std::forward<AddVals>(avals)...);
				}
				auto nval = Property::get_node_synth_value(std::forward<Node>(n));
				// PreventOverflow is checked here
				if (cmp(target, nval) || (PreventOverflow && n.right == nullptr)) { // target node found
					return 0;
				}
				// right branch
				target -= nval;
				_details::add_synth_node_values<Props...>(std::forward<Node>(n), std::forward<AddVals>(avals)...);
				return 1;
			}
		};

		/// Obtains the sum of all nodes before (not including) the given node for all given properties.
		///
		/// \param it Iterator to the node.
		/// \param args Variables where the results are stored. The caller is responsible of initializing these
		///             variables properly.
		template <typename ...Props, typename Iter, typename ...Args> inline static void sum_before(
			Iter &&it, Args &&...args
		) {
			static_assert(sizeof...(Props) == sizeof...(Args), "incorrect number of arguments");
			if (it == it.get_container()->end()) {
				if (it.get_container()->root() != nullptr) {
					_details::add_synth_tree_values<Props...>(
						*it.get_container()->root(), std::forward<Args>(args)...
						);
				}
			} else {
				auto *node = it.get_node();
				if (node->left) {
					_details::add_synth_tree_values<Props...>(*node->left, std::forward<Args>(args)...);
				}
				it.get_container()->synthesize_root_path(node, [&](const auto &parent, const auto &child) {
					if (&child == parent.right) {
						_details::add_synth_node_values<Props...>(parent, std::forward<Args>(args)...);
						if (parent.left) { // left subtree
							_details::add_synth_tree_values<Props...>(*parent.left, std::forward<Args>(args)...);
						}
					}
				});
			}
		}
	};
}
