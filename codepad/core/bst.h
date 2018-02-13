#pragma once

/// \file bst.h
/// Implementation of a generic binary tree.

#include <vector>
#include <functional>

#include "misc.h"

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
		Comp _comp; ///< An instance of the comparison object.
	};
	/// Nodes of a \ref binary_tree.
	///
	/// \tparam T Type of \ref value.
	/// \tparam AdditionalData Type of \ref synth_data.
	template <typename T, typename AdditionalData = no_data> struct binary_tree_node {
		/// Constructs a binary tree node, forwarding all its arguments to the constructor of \ref value.
		template <typename ...Args> explicit binary_tree_node(Args &&...args) : value(std::forward<Args>(args)...) {
		}

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
		binary_tree_node * prev() const {
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
		AdditionalData synth_data; ///< Additional data that's synthesized based on \ref value
								   ///< (and possibly the values of the node's children)
		binary_tree_node
			*left = nullptr, ///< Pointer to the left subtree of the node.
			*right = nullptr, ///< Pointer to the right subtree of the node.
			*parent = nullptr; ///< Pointer to the parent of the node.
	};
	/// A binary tree. Note it may or may not be a binary search tree.
	///
	/// \tparam T Type of the data held by the tree's nodes.
	/// \tparam AdditionalData Type of additional data held by the nodes.
	/// \tparam Synth Provides operator() to calculate binary_tree_node::synth_data.
	template <
		typename T, typename AdditionalData = no_data, typename Synth = default_synthesizer<AdditionalData>
	> struct binary_tree {
	public:
		using node = binary_tree_node<T, AdditionalData>;
		template <bool> struct iterator_base;
		/// Helper class to ensure that binary_tree_node::synth_data
		/// of nodes with modified values are updated properly,
		/// by automatically calling refresh_synthesized_result(binary_tree_node*) in ~node_value_modifier().
		struct node_value_modifier {
			friend struct iterator_base<false>;
			friend struct binary_tree;
		public:
			/// Default constructor.
			node_value_modifier() = default;
			/// Default copy constructor.
			node_value_modifier(const node_value_modifier&) = default;
			/// Swaps the contents of two \ref node_value_modifier "node_value_modifiers"
			/// to eliminate one (redundant) call to refresh_synthesized_result(binary_tree_node*).
			node_value_modifier(node_value_modifier &&mod) : _n(mod._n), _cont(mod._cont) {
				mod._n = nullptr;
				mod._cont = nullptr;
			}
			/// Calls manual_refresh() before copying.
			node_value_modifier &operator=(const node_value_modifier &mod) {
				manual_refresh();
				_n = mod._n;
				_cont = mod._cont;
			}
			/// Calls manual_refresh() before moving the contents.
			node_value_modifier &operator=(node_value_modifier &&mod) {
				manual_refresh();
				_n = mod._n;
				_cont = mod._cont;
				mod._n = nullptr;
				mod._cont = nullptr;
			}
			/// Calls manual_refresh() to update the synthesized values in the tree.
			~node_value_modifier() {
				manual_refresh();
			}

			/// Returns a reference to the node's value for modification.
			///
			/// \return A reference to the value of \ref _n.
			T &operator*() const {
				return _n->value;
			}
			/// Returns a pointer to the node's value for modification.
			///
			/// \return A pointer to the value of \ref _n.
			T *operator->() const {
				return &operator*();
			}

			/// Calls refresh_synthesized_result(binary_tree_node*) to update the synthesized values if
			/// this \ref node_value_modifier is valid.
			void manual_refresh() {
				if (_cont) {
					// if _cont is nonempty, then _n should be nonempty too
					assert_true_logical(_n != nullptr, "invalid modifier");
					_cont->refresh_synthesized_result(_n);
				}
			}
		protected:
			/// Protected constructor that only \ref binary_tree and binary_tree::iterator_base can access.
			///
			/// \param n The node that's to be modified. Should not be nullptr if \p c isn't.
			/// \param c The container that \p n belongs to, or \p nullptr.
			node_value_modifier(node *n, binary_tree *c) : _n(n), _cont(c) {
			}

			node *_n = nullptr; ///< Pointer to the \ref binary_tree_node that the user intends to modify.
			binary_tree *_cont = nullptr; ///< Pointer to the tree that \ref _n belongs to.
		};
		/// Template of const and non-const iterators.
		///
		/// \tparam Const \p true for \ref const_iterator, \p false for \ref iterator.
		template <bool Const> struct iterator_base {
			friend struct binary_tree<T, AdditionalData, Synth>;
		public:
			using value_type = decltype(node::value);
			using reference = std::conditional_t<Const, const value_type&, value_type&>;
			using pointer = std::conditional_t<Const, const value_type*, value_type*>;
			using iterator_category = std::bidirectional_iterator_tag;

			using tree_type = binary_tree<T, AdditionalData, Synth>;
			using container_type = std::conditional_t<Const, const tree_type, tree_type>;

			/// Default constructor.
			iterator_base() = default;
			/// Converting constructor from non-const iterator to const iterator.
			///
			/// \param it A non-const iterator.
			iterator_base(const iterator_base<false> &it) :
				_con(it._con), _n(it._n) {
			}

			/// Equality of two \ref iterator_base instances.
			friend bool operator==(const iterator_base &lhs, const iterator_base &rhs) {
				return lhs._con == rhs._con && lhs._n == rhs._n;
			}
			/// Inequality of two \ref iterator_base instances.
			friend bool operator!=(const iterator_base &lhs, const iterator_base &rhs) {
				return !(lhs == rhs);
			}

			/// Pre-increment.
			iterator_base &operator++() {
				assert_true_logical(_n != nullptr, "cannot increment iterator");
				_n = _n->next();
				return *this;
			}
			/// Post-increment.
			iterator_base operator++(int) {
				iterator_base ov = *this;
				++*this;
				return ov;
			}
			/// Pre-decrement.
			iterator_base &operator--() {
				if (_n) {
					_n = _n->prev();
					assert_true_logical(_n != nullptr, "cannot decrement iterator");
				} else {
					_n = _con->max();
				}
				return *this;
			}
			/// Post-decrement.
			iterator_base operator--(int) {
				iterator_base ov = *this;
				--*this;
				return ov;
			}

			/// Used to bypass \ref node_value_modifier and modify the value of the node.
			/// The caller is responsible of calling refresh_synthesized_result(binary_tree_node*) afterwards.
			///
			/// \return A reference to binary_tree_node::value.
			T &get_value_rawmod() const {
				return _n->value;
			}
			/// Returns the readonly value of the node.
			///
			/// \return A const reference to binary_tree_node::value.
			const T &get_value() const {
				return _n->value;
			}
			/// Returns a corresponding \ref node_value_modifier. 
			///
			/// \return A corresponding \ref node_value_modifier.
			node_value_modifier get_modifier() const {
				return _con->get_modifier(_n);
			}
			/// Returns the readonly value of the node.
			///
			/// \return A const reference to binary_tree_node::value.
			const T &operator*() const {
				return get_value();
			}
			/// Returns the readonly value of the node.
			///
			/// \return A const pointer to binary_tree_node::value.
			const T *operator->() const {
				return &get_value();
			}

			/// Returns the underlying node.
			///
			///\return The underlying node.
			node *get_node() const {
				return _n;
			}
			/// Returns the tree that this iterator belongs to.
			///
			/// \return The tree that the iterator belongs to.
			container_type *get_container() const {
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
		using iterator = iterator_base<false>;
		using const_iterator = iterator_base<true>;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<iterator>;

		/// Default constructor.
		binary_tree() = default;
		/// Initializes \ref _synth with \p args, then builds the tree with the contents of \p cont.
		///
		/// \tparam Cont Container type.
		/// \param cont A container used to populate the tree.
		/// \param args Arguments for \ref _synth.
		template <typename Cont, typename ...Args> explicit binary_tree(Cont &&cont, Args &&...args) :
			_synth(std::forward<Args>(args)...) {
			_root = build_tree(std::forward<Cont>(cont), _synth);
		}
		/// Copy constructor. Clones the tree.
		binary_tree(const binary_tree &tree) : _root(clone_tree(tree._root)) {
		}
		/// Move constructor. Moves the root pointer.
		binary_tree(binary_tree &&tree) : _root(tree._root) {
			tree._root = nullptr;
		}
		/// Copy assignment. Clones the tree after deleting the old one.
		binary_tree &operator=(const binary_tree &tree) {
			delete_tree(_root);
			_root = clone_tree(tree._root);
			return *this;
		}
		/// Move assignment. Exchanges the root pointers.
		binary_tree &operator=(binary_tree &&tree) {
			std::swap(_root, tree._root);
			return *this;
		}
		/// Destructor. Calls delete_tree(binary_tree_node*) to dispose all nodes.
		~binary_tree() {
			delete_tree(_root);
		}

		/// Inserts a node into the tree. Creates a \ref binary_tree_node using the provided arguments,
		/// then travels from the root downwards, using a branch selector to determine where to go.
		///
		/// \param d The branch selector. See \ref bst_branch_selector for an example.
		/// \param args Arguments used to initialize the new node.
		template <typename BranchSelector, typename ...Args> iterator insert_custom(BranchSelector &&d, Args &&...args) {
			node *n = new node(std::forward<Args>(args)...), *prev = nullptr, **pptr = &_root;
			while (*pptr) {
				prev = *pptr;
				pptr = d.select_insert(*prev, *n) ? &prev->left : &prev->right;
			}
			*pptr = n;
			n->parent = prev;
			refresh_synthesized_result(n);
			return iterator(this, n);
		}
		/// Inserts a node into the tree as if it were a binary search tree.
		///
		/// \tparam Comp Comparison function used when selecting branches.
		/// \param args Arguments used to initialize the new node.
		template <typename Comp, typename ...Args> iterator insert(Args &&...args) {
			return insert_custom(bst_branch_selector<Comp>(), std::forward<Args>(args)...);
		}

		/// Inserts a node or a subtree before a node, that is, after the insertion,
		/// the predecesor of \p before will be the rightmost element of the subtree.
		/// If \p before is \p nullptr, \p n is inserted at the end of the tree.
		///
		/// \param before Indicates where the new subtree is to be inserted.
		/// \param n The root of the subtree.
		void insert_before_raw(node *before, node *n) {
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
			refresh_synthesized_result(before);
		}
		/// Builds a tree from an array of objects, then insert the tree before the designated node.
		///
		/// \param before Indicates where to insert the subtree.
		/// \param objs A container of values.
		template <typename Cont> void insert_tree_before(node *before, Cont &&objs) {
			insert_before_raw(before, build_tree(std::forward<Cont>(objs), _synth));
		}
		/// \overload
		template <typename Cont> void insert_tree_before(const_iterator it, Cont &&objs) {
			insert_tree_before(it.get_node(), std::forward<Cont>(objs));
		}
		/// \overload
		template <typename It> void insert_tree_before(node *before, It &&beg, It &&end) {
			insert_before_raw(before, build_tree(std::forward<It>(beg), std::forward<It>(end)));
		}
		/// \overload
		template <typename It> void insert_tree_before(const_iterator it, It &&beg, It &&end) {
			insert_tree_before(it.get_node(), std::forward<It>(beg), std::forward<It>(end));
		}
		/// Inserts a node into the tree before the designated node.
		///
		/// \param before Indicates where to insert the node.
		/// \param args Arguments used to initialize the new node.
		template <typename ...Args> node *insert_node_before(node *before, Args &&...args) {
			node *n = new node(std::forward<Args>(args)...);
			_refresh_synth(n);
			insert_before_raw(before, n);
			return n;
		}
		/// \overload
		template <typename ...Args> iterator insert_node_before(const_iterator it, Args &&...args) {
			return get_iterator_for(insert_node_before(it.get_node(), std::forward<Args>(args)...));
		}

		/// Finds a node using a branch selector and a reference.
		/// The reference may be non-const to provide additional feedback.
		///
		/// \param b A branch selector. See \ref bst_branch_selector for an example.
		/// \param ref The reference for finding the target node.
		/// \return The resulting iterator, or end() if none qualifies.
		template <typename BranchSelector, typename Ref> iterator find_custom(BranchSelector &&b, Ref &&ref) {
			return _find_custom_impl<iterator>(this, std::forward<BranchSelector>(b), std::forward<Ref>(ref));
		}
		/// Const version of find_custom(BranchSelector&&, Ref&&).
		template <typename BranchSelector, typename Ref> const_iterator find_custom(BranchSelector &&b, Ref &&ref) const {
			return _find_custom_impl<const_iterator>(this, std::forward<BranchSelector>(b), std::forward<Ref>(ref));
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

		/// Update the synthesized values of all nodes on the path from \p n to the root.
		///
		/// \param n One end of the path.
		void refresh_synthesized_result(node *n) {
			for (; n; n = n->parent) {
				_refresh_synth(n);
			}
		}
	protected:
		/// Enumeration for recording node status when traversing the tree.
		enum class _traverse_status {
			not_visited, ///< The node has not been visited.
			visited_left, ///< The node's left subtree has been visited.
			visited_right ///< The node's right subtree has been visited.
		};
	public:
		/// Update the synthesized values of all nodes in the tree.
		void refresh_tree_synthesized_result() {
			std::vector<std::pair<node*, _traverse_status>> stk;
			if (_root) {
				stk.push_back(std::make_pair(_root, _traverse_status::not_visited));
			}
			while (!stk.empty()) { // nodes are visited in a DFS-like fashion
				std::pair<node*, _traverse_status> &p = stk.back();
				switch (p.second) {
				case _traverse_status::not_visited:
					if (p.first->left) { // visit left subtree if there is one
						p.second = _traverse_status::visited_left;
						stk.push_back(std::make_pair(p.first->left, _traverse_status::not_visited));
						break;
					}
					[[fallthrough]]; // if there is none
				case _traverse_status::visited_left:
					if (p.first->right) { // visit right subtree if there is one
						p.second = _traverse_status::visited_right;
						stk.push_back(std::make_pair(p.first->right, _traverse_status::not_visited));
						break;
					}
					[[fallthrough]]; // if there is none
				case _traverse_status::visited_right:
					_refresh_synth(p.first);
					stk.pop_back();
					break;
				}
			}
		}
		/// Synthesize certain values from a node to the root.
		///
		/// \param n The node to start with.
		/// \param v An object whose %operator() is called at every node with the current node and its parent.
		template <typename NewSynth> void synthesize_root_path(const node *n, NewSynth &&v) const {
			if (n) {
				for (node *p = n->parent; p; n = p, p = p->parent) {
					v(*p, *n);
				}
			}
		}
		/// Right rotation.
		///
		/// \param n The root of the subtree before rotation. \p n->left will be in its place after.
		void rotate_right(node *n) {
			assert_true_logical(n->left != nullptr, "cannot perform rotation");
			node *left = n->left;
			n->left = n->left->right;
			left->right = n;
			left->parent = n->parent;
			n->parent = left;
			if (n->left) {
				n->left->parent = n;
			}
			if (left->parent) {
				(n == left->parent->left ? left->parent->left : left->parent->right) = left;
			} else {
				assert_true_logical(_root == n, "invalid node");
				_root = left;
			}
			left->synth_data = n->synth_data;
			_refresh_synth(n);
		}
		/// Left rotation.
		///
		/// \param n The root of the subtree before rotation. \p n->right will be in its place after.
		void rotate_left(node *n) {
			assert_true_logical(n->right != nullptr, "cannot perform rotation");
			node *right = n->right;
			n->right = n->right->left;
			right->left = n;
			right->parent = n->parent;
			n->parent = right;
			if (n->right) {
				n->right->parent = n;
			}
			if (right->parent) {
				(n == right->parent->left ? right->parent->left : right->parent->right) = right;
			} else {
				assert_true_logical(_root == n, "invalid node");
				_root = right;
			}
			right->synth_data = n->synth_data;
			_refresh_synth(n);
		}
		/// Splays the node until its parent is another designated node.
		///
		/// \param n The node to bring up.
		/// \param targetroot The opration stops when it is the parent of \p n.
		void splay(node *n, node *targetroot = nullptr) {
			while (n->parent != targetroot) {
				if (
					n->parent->parent != targetroot &&
					(n == n->parent->left) == (n->parent == n->parent->parent->left)
					) {
					if (n == n->parent->left) {
						rotate_right(n->parent->parent);
						rotate_right(n->parent);
					} else {
						rotate_left(n->parent->parent);
						rotate_left(n->parent);
					}
				} else {
					if (n == n->parent->left) {
						rotate_right(n->parent);
					} else {
						rotate_left(n->parent);
					}
				}
			}
		}

		/// Returns the leftmost element of the tree, or \p nullptr if the tree's empty.
		///
		/// \return The leftmost element of the tree, or \p nullptr if the tree's empty.
		node *min() const {
			return min(_root);
		}
		/// Returns the leftmost element of a tree whose root is \p n, or \p nullptr.
		///
		/// \return The leftmost element of a tree whose root is \p n, or \p nullptr.
		inline static node *min(node *n) {
			for (; n && n->left; n = n->left) {
			}
			return n;
		}
		/// Returns the rightmost element of the tree, or \p nullptr if the tree's empty.
		///
		/// \return The rightmost element of the tree, or \p nullptr if the tree's empty.
		node *max() const {
			return max(_root);
		}
		/// Returns the rightmost element of a tree whose root is \p n, or \p nullptr.
		///
		/// \return The rightmost element of a tree whose root is \p n, or \p nullptr.
		inline static node *max(node *n) {
			for (; n && n->right; n = n->right) {
			}
			return n;
		}
		/// Returns the root of the tree.
		///
		/// \return The root of the tree.
		node *root() const {
			return _root;
		}
		/// Checks if the tree is empty.
		///
		/// \return \p true if the tree has no nodes, \p false otherwise.
		bool empty() const {
			return _root == nullptr;
		}

		/// Removes a certain node from the tree and deletes it.
		///
		/// \param n The node to remove.
		void erase(node *n) {
			if (n == nullptr) {
				return;
			}
			node *oc;
			if (n->left && n->right) {
				node *rmin = min(n->right);
				splay(rmin, n);
				rotate_left(n);
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
			refresh_synthesized_result(f);
		}
		/// Removes a range of nodes from the tree and deletes them.
		///
		/// \param beg The first node to remove.
		/// \param end The node after the last node to remove. \p nullptr to indicate the last node.
		void erase(node *beg, node *end) {
			delete_tree(detach_tree(beg, end));
		}
		/// Removes a range of nodes from the tree.
		///
		/// \param beg The first node to remove.
		/// \param end The node after the last node to remove. \p nullptr to indicate the last node.
		/// \return The root of the removed subtree.
		node *detach_tree(node *beg, node *end) {
			if (beg == nullptr) {
				return nullptr;
			}
			beg = beg->prev();
			node *res = nullptr;
			if (beg && end) {
				splay(beg);
				splay(end, beg);
				assert_true_logical(end == beg->right, "invalid range");
				res = end->left;
				end->left = nullptr;
				_refresh_synth(end);
				_refresh_synth(beg);
			} else if (beg) {
				splay(beg);
				res = beg->right;
				beg->right = nullptr;
				_refresh_synth(beg);
			} else if (end) {
				splay(end);
				res = end->left;
				end->left = nullptr;
				_refresh_synth(end);
			} else {
				res = _root;
				_root = nullptr;
			}
			if (res) {
				res->parent = nullptr;
			}
			return res;
		}
		/// \overload
		void erase(const_iterator it) {
			erase(it.get_node());
		}
		/// \overload
		void erase(const_iterator beg, const_iterator end) {
			erase(beg.get_node(), end.get_node());
		}

		/// Returns a \ref node_value_modifier of a given node.
		///
		/// \param n The node to get a \ref node_value_modifier for.
		/// \return A \ref node_value_modifier of a given node.
		node_value_modifier get_modifier(node *n) {
			return node_value_modifier(n, this);
		}

		/// Deletes all nodes in the tree, and resets \ref _root to \p nullptr.
		void clear() {
			delete_tree(_root);
			_root = nullptr;
		}

		/// Builds a tree from an array of objects.
		/// Objects are moved or copied out of the container according to the type of the argument.
		///
		/// \param objs The objects to build a tree out of.
		/// \return The root of the newly built tree.
		template <typename Cont> node *build_tree(Cont &&objs) const {
			return build_tree(std::forward<Cont>(objs), _synth);
		}
		/// \overload
		template <typename It> node *build_tree(It &&beg, It &&end) const {
			return build_tree(std::forward<It>(beg), std::forward<It>(end), _synth);
		}

		/// Replaces \ref _synth with the given synthesizer.
		///
		/// \param s The new synthesizer.
		template <typename SynRef> void set_synthesizer(SynRef &&s) {
			_synth = std::forward<SynRef>(s);
		}
		/// Returns a const reference to the current synthesizer.
		///
		/// \return A const reference to the current synthesizer.
		const Synth &get_synthesizer() const {
			return _synth;
		}

		/// Clones a tree.
		///
		/// \param n The root of the tree to clone.
		/// \return The root of the new tree.
		inline static node *clone_tree(const node *n) {
			if (n == nullptr) {
				return nullptr;
			}
			std::vector<_clone_node> stk;
			node *res = nullptr;
			stk.push_back(_clone_node(n, nullptr, &res));
			do {
				_clone_node curv = stk.back();
				stk.pop_back();
				node *cn = new node(*curv.src);
				cn->parent = curv.parent;
				*curv.assign = cn;
				if (curv.src->left) {
					stk.push_back(_clone_node(curv.src->left, cn, &cn->left));
				}
				if (curv.src->right) {
					stk.push_back(_clone_node(curv.src->right, cn, &cn->right));
				}
			} while (!stk.empty());
			return res;
		}
		/// Deletes all nodes in a tree.
		///
		/// \param n The root of the tree to delete.
		inline static void delete_tree(node *n) {
			if (!n) {
				return;
			}
			std::vector<node*> ns;
			ns.push_back(n);
			do {
				node *c = ns.back();
				ns.pop_back();
				if (c->left) {
					ns.push_back(c->left);
				}
				if (c->right) {
					ns.push_back(c->right);
				}
				delete c;
			} while (!ns.empty());
		}
		/// Builds a tree from an array of objects.
		/// Objects are copied from the container.
		///
		/// \param objs The objects to build a tree out of.
		/// \param synth The synthesizer used.
		/// \return The root of the newly built tree.
		template <typename Cont, typename SynRef> inline static node *build_tree(const Cont &objs, SynRef &&synth) {
			return _build_tree<_copy_value>(objs.begin(), objs.end(), std::forward<SynRef>(synth));
		}
		/// Builds a tree from an array of objects.
		/// Objects are moved from the container.
		///
		/// \param objs The objects to build a tree out of.
		/// \param synth The synthesizer used.
		/// \return The root of the newly built tree.
		template <typename Cont, typename SynRef> inline static std::enable_if_t<
			!std::is_lvalue_reference_v<Cont>, node*
		> build_tree(Cont &&objs, SynRef &&synth) {
			return _build_tree<_move_value>(objs.begin(), objs.end(), std::forward<SynRef>(synth));
		}
		/// Builds a tree from an array of objects.
		/// Objects are copied from the container.
		///
		/// \param beg Iterator to the first element.
		/// \param end Iterator past the last element.
		/// \param synth The synthesizer used.
		/// \return The root of the newly built tree.
		template <typename It, typename SynRef> inline static node *build_tree(It &&beg, It &&end, SynRef &&synth) {
			return _build_tree<_copy_value>(
				std::forward<It>(beg), std::forward<It>(end), std::forward<SynRef>(synth)
				);
		}
	protected:
		/// Implementation of the \ref find_custom function,
		/// in order to support const and non-const iterators with the same code.
		///
		/// \tparam It Desired type of the iterator (const / non-const).
		/// \param c The container (tree).
		/// \param b The branch selector.
		/// \param ref The reference value.
		/// \return The resulting iterator.
		template <
			typename It, typename Cont, typename B, typename Ref
		> inline static It _find_custom_impl(Cont *c, B &&b, Ref &&ref) {
			node *cur = c->_root;
			while (cur) {
				switch (b.select_find(*cur, std::forward<Ref>(ref))) {
				case -1:
					cur = cur->left;
					break;
				case 0:
					return It(c, cur);
				case 1:
					cur = cur->right;
					break;
				}
			}
			return It(c, cur);
		}

		node *_root = nullptr; ///< Pointer to the root node.
		Synth _synth; ///< The synthesizer object used.

		/// Re-calculate the synthesized value of the given node, using the designated synthesizer,
		/// by simply calling the %operator() of the synthesizer object.
		///
		/// \param sy The designated synthesizer object.
		/// \param n The target node.
		template <typename SynRef> inline static void _refresh_synth(SynRef &&sy, node *n) {
			sy(*n);
		}
		/// Re-calculate the synthesized value of the given node, using the current synthesizer of the tree.
		///
		/// \param n The target node.
		void _refresh_synth(node *n) {
			_refresh_synth(_synth, n);
		}

		/// Utility struct used when cloning a tree.
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
			/// Interface for \ref _build_tree.
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
			It beg, It end, SynRef &&s
		) {
			if (beg == end) {
				return nullptr;
			}
			auto half = beg + (end - beg) / 2;
			// recursively call _build_tree
			node
				*left = _build_tree<Op, It, SynRef>(beg, half, std::forward<SynRef>(s)),
				*right = _build_tree<Op, It, SynRef>(half + 1, end, std::forward<SynRef>(s)),
				*cur = Op::get(half);
			cur->left = left;
			cur->right = right;
			if (cur->left) {
				cur->left->parent = cur;
			}
			if (cur->right) {
				cur->right->parent = cur;
			}
			_refresh_synth(std::forward<SynRef>(s), cur);
			return cur;
		}
	};

	/// Helper structs to simplify the use of \ref binary_tree.
	namespace synthesization_helper {
		/// Indicates that the property is stored in a field of binary_tree_node::value.
		///
		/// \tparam Ptr Type of the member pointer.
		/// \tparam Prop Member pointer to the field.
		/// \todo Change \p Ptr to \p auto when VS supports it
		template <typename Ptr, Ptr Prop> struct field_value_property {
			/// Getter interface for propeties.
			///
			/// \return The retrieved value.
			template <typename Node> inline static auto get(Node &&n) {
				return n.value.*Prop;
			}
		};
		/// Indicates that the property can be retrieved by calling a method of binary_tree_node::value.
		///
		/// \tparam Func Type of the function.
		/// \tparam Prop Member pointer to the function.
		/// \todo Change \p Ptr to \p auto when VS supports it
		template <typename Func, Func Prop> struct func_value_property {
			/// Getter interface for properties.
			///
			/// \return The return value of the function.
			template <typename Node> inline static auto get(Node &&n) {
				return (n.value.*Prop)();
			}
		};
	}

	/// Synthesizer related structs that treat the data as block sizes,
	/// i.e., each node represents an array of objects after the previous one.
	/// The representation can be the actual objects and / or certain statistics of them.
	namespace sum_synthesizer {
		/// Represents a property of binary_tree_node::synth_data,
		/// that records the statistics of a single node and the whole subtree.
		///
		/// \tparam T Type of the statistics.
		/// \tparam Synth Type of binary_tree_node::synth_data.
		/// \tparam GetForNode A property of binary_tree_node::value, similar to the ones in \ref synthesization_helper.
		/// \tparam NodeVal Member pointer to the statistics for a single node.
		/// \tparam TreeVal Member pointer to the statistics for a subtree.
		template <typename T, typename Synth, typename GetForNode, T Synth::*NodeVal, T Synth::*TreeVal> struct property {
			using value_type = T;

			/// Returns the statistics obtained directly from the node.
			///
			/// \return The statistics obtained directly from the node.
			template <typename Node> inline static auto get_node_value(Node &&n) {
				return GetForNode::get(std::forward<Node>(n));
			}
			/// Returns the statistics of a single node obtained from binary_tree_node::synth_data.
			///
			/// \return The statistics of a single node obtained from binary_tree_node::synth_data.
			template <typename Node> inline static T get_node_synth_value(Node &&n) {
				return n.synth_data.*NodeVal;
			}
			/// Sets the statistics of the node in binary_tree_node::synth_data.
			template <typename Node> inline static void set_node_synth_value(Node &&n, T v) {
				n.synth_data.*NodeVal = std::move(v);
			}
			/// Returns the statistics of a subtree obtained from binary_tree_node::synth_data.
			///
			/// \return The statistics of a subtree obtained from binary_tree_node::synth_data.
			template <typename Node> inline static T get_tree_synth_value(Node &&n) {
				return n.synth_data.*TreeVal;
			}
			/// Sets the statistics of the subtree in binary_tree_node::synth_data.
			template <typename Node> inline static void set_tree_synth_value(Node &&n, T v) {
				n.synth_data.*TreeVal = std::move(v);
			}
		};
		/// Represents a property of binary_tree_node::synth_data that only stores the statistics of a subtree.
		///
		/// \tparam T Type of the statistics.
		/// \tparam Synth Type of binary_tree_node::synth_data.
		/// \tparam GetForNode A property of binary_tree_node::value, similar to the ones in \ref synthesization_helper.
		/// \tparam TreeVal Member pointer to the statistics for a subtree.
		template <typename T, typename Synth, typename GetForNode, T Synth::*TreeVal> struct compact_property {
			using value_type = T;

			/// Returns the statistics obtained directly from the node.
			///
			/// \return The statistics obtained directly from the node.
			template <typename Node> inline static T get_node_value(Node &&n) {
				return GetForNode::get(std::forward<Node>(n));
			}
			/// Returns the statistics of a single node obtained from binary_tree_node::synth_data.
			///
			/// \return The statistics of a single node obtained from binary_tree_node::synth_data.
			template <typename Node> inline static T get_node_synth_value(Node &&n) {
				return get_node_value(std::forward<Node>(n));
			}
			/// Does nothing because there's no corresponding field.
			template <typename Node> inline static void set_node_synth_value(Node&&, T) {
			}
			/// Returns the statistics of a subtree obtained from binary_tree_node::synth_data.
			///
			/// \return The statistics of a subtree obtained from binary_tree_node::synth_data.
			template <typename Node> inline static T get_tree_synth_value(Node &&n) {
				return n.synth_data.*TreeVal;
			}
			/// Sets the statistics of the subtree in binary_tree_node::synth_data.
			template <typename Node> inline static void set_tree_synth_value(Node &&n, T v) {
				n.synth_data.*TreeVal = std::move(v);
			}
		};

		/// Helper functions for setting node values.
		namespace _details {
			/// End of recursion.
			template <typename Node> inline void set_node_values(Node&&) {
			}
			/// Sets the node values and tree values of binary_tree_node::synth_data to the value
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
			/// Adds the tree values of binary_tree_node::synth_data of \p sub to \p n for all properties.
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
				add_synth_node_values<OtherProps...>(std::forward<Node>(n), std::forward<OtherVals>(otvals)...);
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
		/// Finder that can be used with binary_tree::find_custom. Finds a node such that
		/// a property of all nodes to its left accumulates to or below a certain value.
		/// For instance, suppose that each node stores an array of objects,
		/// and that these arrays may not have the same length, this finder can be used
		/// to find the node holding the k-th element.
		///
		/// \tparam Property The property used to perform the lookup.
		/// \tparam PreventOverflow If \p true, the result won't exceed the last element.
		/// \tparam Comp Comparison function used to decide which node to go to.
		///              Typically \p std::less or \p std::less_equal.
		template <typename Property, bool PreventOverflow = false, typename Comp = std::less<typename Property::value_type>> struct index_finder {
			/// Interface for binary_tree::find_custom. This function can also collect additional data
			/// when looking for the node, as specified by \p Props and \p avals.
			///
			/// \tparam Props Additional properties whose values will be accumulated in
			///         corresponding entries in \p avals.
			/// \param n The current node, provided by binary_tree::find_custom.
			/// \param target The target value.
			/// \param avals Variables to collect additional statistics.
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
	};
}
