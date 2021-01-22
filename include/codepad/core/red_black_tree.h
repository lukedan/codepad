#pragma once

/// \file
/// Implementation of red-black tree operations.

#include "binary_tree.h"

/// Implementation of red-black tree oprations.
namespace codepad::red_black_tree {
	/// The color of a node.
	enum class color : unsigned char {
		black, ///< Black.
		red ///< Red.
	};

	/// Access a red-black flag in a node's value through the given member pointer.
	template <auto MemberPtr> struct member_red_black_access {
		/// Returns the value.
		template <typename Node> color get(const Node &n) const {
			return n.value.*MemberPtr;
		}
		/// Sets the value.
		template <typename Node> void set(Node &n, color value) const {
			n.value.*MemberPtr = value;
		}
	};

	/// Checks the integrity of the given red-black tree. This function does not check if the binary tree structure
	/// is valid.
	template <typename Node, typename RedBlackAccess> void check_integrity(Node *root, RedBlackAccess &&access) {
		if (root == nullptr) {
			return;
		}

		binary_tree<
			typename Node::value_type, typename Node::synth_data_type, lacks_synthesizer
		>::check_integrity(root);

		std::optional<std::size_t> black_depth;
		std::stack<std::pair<Node*, std::size_t>> stack;
		assert_true_logical(access.get(*root) == color::black, "root must be black");
		stack.emplace(root, 0);
		while (!stack.empty()) {
			Node *n = stack.top().first;
			std::size_t depth = stack.top().second;
			stack.pop();

			if (n == nullptr) {
				if (black_depth.has_value()) {
					assert_true_logical(black_depth.value() == depth, "inconsistent black depth");
				} else {
					black_depth.emplace(depth);
				}
				continue;
			}

			if (access.get(*n) == color::red) {
				assert_true_logical(
					(n->left == nullptr || access.get(*n->left) == color::black) &&
					(n->right == nullptr || access.get(*n->right) == color::black),
					"both children of a red node must be black"
				);
			} else {
				++depth;
			}

			stack.emplace(n->left, depth);
			stack.emplace(n->right, depth);
		}
	}

	// insertion & deletion
	/// After a node has been inserted into the tree, performs post-insert fixup to re-balance the tree.
	template <
		typename Tree, typename RedBlackAccess, typename Synth
	> void post_insert_fixup(typename Tree::iterator iter, RedBlackAccess &&access, Synth &&synth) {
		using _node = typename Tree::node;

		Tree *tree = iter.get_container();
		_node *node = iter.get_node();
		while (true) {
			// case 1: root
			if (node->parent == nullptr) {
				access.set(*node, color::black);
				break;
			}

			access.set(*node, color::red); // paint it red first
			// case 2: parent is black
			if (access.get(*node->parent) == color::black) {
				break;
			}

			_node *grandparent = node->parent->parent;
			_node *uncle = node->parent == grandparent->left ? grandparent->right : grandparent->left;
			// case 3: uncle is red
			if (uncle && access.get(*uncle) == color::red) {
				access.set(*node->parent, color::black);
				access.set(*uncle, color::black);
				node = grandparent;
				continue; // continue the fixup for grandparent, this is the only loop point
			}

			// case 4: no uncle (black nil) or uncle is black
			if (node->parent == grandparent->left) {
				if (node == node->parent->right) {
					tree->rotate_left(node->parent, synth);
					node = node->left;
				}
				tree->rotate_right(grandparent, std::forward<Synth>(synth));
			} else {
				if (node == node->parent->left) {
					tree->rotate_right(node->parent, synth);
					node = node->right;
				}
				tree->rotate_left(grandparent, std::forward<Synth>(synth));
			}
			access.set(*node->parent, color::black);
			access.set(*grandparent, color::red);
			break;
		}
	}
	/// \ref post_insert_fixup() without requiring the node to be actually in a tree. The caller also has to pass in
	/// the root of the tree, and will get a new root pointer in return (this function doesn't actually create new
	/// nodes, only that it may rotate the root away).
	///
	/// \return The new root of the tree.
	template <typename Node, typename RedBlackAccess, typename Synth> Node *post_insert_fixup_notree(
		Node *n, Node *root, RedBlackAccess &&access, Synth &&synth
	) {
		using _tree = binary_tree<typename Node::value_type, typename Node::synth_data_type, lacks_synthesizer>;

		// FIXME this is kinda hacky but it's the only way to do it for now
		_tree temp_tree;
		temp_tree.mutable_root() = root;
		post_insert_fixup<_tree>(
			temp_tree.get_iterator_for(n), std::forward<RedBlackAccess>(access), std::forward<Synth>(synth)
		);
		root = temp_tree.root();
		temp_tree.mutable_root() = nullptr; // reset the root so that the tree doesn't delete all the nodes
		return root;
	}

	/// Detaches the given node from the tree **without deleting it**.
	///
	/// \return Successor of the detached node.
	template <typename Tree, typename RedBlackAccess, typename Synth> typename Tree::node *detach(
		Tree &tree, typename Tree::node *node, RedBlackAccess &&access, Synth &&synth
	) {
		using _node = typename Tree::node;

		_node *return_value = nullptr; // the return value - the successor of the input node
		if (node->right) {
			_node *next = node->right;
			for (; next->left; next = next->left) {
			}
			return_value = next;

			// node has two children, swap node and next
			if (node->left) {
				// first swap pointers within node and next...
				next->left = node->left;
				node->left = nullptr;
				if (next->parent == node) {
					// node and next are adjacent
					node->right = next->right;
					next->parent = node->parent;
					node->parent = next;
					next->right = node;
				} else {
					// node and next are not adjacent
					std::swap(next->right, node->right);
					std::swap(next->parent, node->parent);

					// ...and then adjust pointers that point to them
					next->right->parent = next;
					node->parent->left = node;
				}
				next->left->parent = next;
				if (node->right) {
					node->right->parent = node;
				}
				if (next->parent) {
					(node == next->parent->left ? next->parent->left : next->parent->right) = next;
				}
				if (tree.root() == node) {
					tree.mutable_root() = next;
				}

				// refresh synth
				synth(*node);
				synth(*next);
				{
					color tmp_color = access.get(*node);
					access.set(*node, access.get(*next));
					access.set(*next, tmp_color);
				}
			}
		} else {
			for (
				return_value = node;
				return_value->parent && return_value == return_value->parent->right;
				return_value = return_value->parent
			) {
			}
			return_value = return_value->parent;
		}

		// simple case: node is red, must have no children
		if (access.get(*node) == color::red) {
			assert_true_logical(node->left == nullptr && node->right == nullptr, "corrupted red-black tree");
			if (node->parent) {
				(node == node->parent->left ? node->parent->left : node->parent->right) = nullptr;
				tree.refresh_synthesized_result(node->parent, std::forward<Synth>(synth));
				node->parent = nullptr;
			} else { // the last node of the tree
				assert_true_logical(tree.root() == node, "corrupted red-black tree");
				tree.mutable_root() = nullptr;
			}
			return return_value;
		}

		// otherwise the node is black
		_node *only_child = node->left ? node->left : node->right;
		// if it has a child, it must be red
		if (only_child) {
			assert_true_logical(access.get(*only_child) == color::red, "corrupted red-black tree");
			access.set(*only_child, color::black);
			only_child->parent = node->parent;
			if (node->parent) {
				(node->parent->left == node ? node->parent->left : node->parent->right) = only_child;
				tree.refresh_synthesized_result(node->parent, std::forward<Synth>(synth));
				node->parent = nullptr;
			} else {
				tree.mutable_root() = only_child;
			}
			node->left = node->right = nullptr;
			return return_value;
		}

		// back up the node pointer because it's treated as a virtual leaf and must not be removed until all
		// adjustments are finished
		_node *node_to_remove = node;
		// otherwise delete the black node
		while (true) {
			// case 1: root, just break
			if (node->parent == nullptr) {
				break;
			}

			// sibling must be non-null
			_node *sibling = node == node->parent->left ? node->parent->right : node->parent->left;
			color sibling_left = color::black, sibling_right = color::black;
			// case 2: sibling is red
			if (access.get(*sibling) == color::red) {
				access.set(*node->parent, color::red);
				access.set(*sibling, color::black);
				if (node == node->parent->left) {
					tree.rotate_left(node->parent, synth);
					sibling = node->parent->right;
				} else {
					tree.rotate_right(node->parent, synth);
					sibling = node->parent->left;
				}
				// this will color parent red so case 3 is impossible
				// the new sibling must also be non-null and black

				// retrieve sibling colors from the new sibling
				sibling_left = sibling->left ? access.get(*sibling->left) : color::black;
				sibling_right = sibling->right ? access.get(*sibling->right) : color::black;
			} else {
				// retrieve sibling colors from the new sibling
				sibling_left = sibling->left ? access.get(*sibling->left) : color::black;
				sibling_right = sibling->right ? access.get(*sibling->right) : color::black;

				// case 3: parent, sibling, and sibling's children are black
				// we already know that the sibling is black
				if (
					access.get(*node->parent) == color::black &&
					sibling_left == color::black && sibling_right == color::black
				) {
					access.set(*sibling, color::red);
					node = node->parent;
					continue; // loop back
				}
			}

			// case 4: sibling and children are black but parent is red
			// again, we already know that the sibling is black
			if (
				access.get(*node->parent) == color::red &&
				sibling_left == color::black && sibling_right == color::black
			) {
				access.set(*sibling, color::red);
				access.set(*node->parent, color::black);
				break;
			}

			// case 5: s is black, and the inner child of s is red
			if (node == node->parent->left) {
				// if sibling right is black, then sibling left must be red or we'll never reach this
				if (sibling_right == color::black) {
					access.set(*sibling->left, color::black);
					access.set(*sibling, color::red);
					tree.rotate_right(sibling, synth);

					sibling = sibling->parent;
					// sibling_left is black
					// sibling_right is red
				}

				// case 6: sibling is black, sibling right is red
				access.set(*sibling, access.get(*node->parent));
				access.set(*node->parent, color::black);
				access.set(*sibling->right, color::black);
				tree.rotate_left(node->parent, synth);
			} else {
				if (sibling_left == color::black) {
					access.set(*sibling->right, color::black);
					access.set(*sibling, color::red);
					tree.rotate_left(sibling, synth);

					sibling = sibling->parent;
					// sibling_right is black
					// sibling_left is red
				}

				// case 6
				access.set(*sibling, access.get(*node->parent));
				access.set(*node->parent, color::black);
				access.set(*sibling->left, color::black);
				tree.rotate_right(node->parent, synth);
			}
			break; // do not loop
		}

		// finally detach node from the tree
		if (node_to_remove->parent) {
			if (node_to_remove == node_to_remove->parent->left) {
				node_to_remove->parent->left = nullptr;
			} else {
				node_to_remove->parent->right = nullptr;
			}
			// refresh synth
			tree.refresh_synthesized_result(node_to_remove->parent, std::forward<Synth>(synth));
			node_to_remove->parent = nullptr;
		} else {
			tree.mutable_root() = nullptr;
		}

		return return_value;
	}

	// bulk operations
	/// Returns the black depth of the given tree - the number of black nodes on the left spine including the root
	/// and excluding the conceptual \p nil leaves.
	template <typename Node, typename RedBlackAccess> std::size_t black_depth(Node *n, RedBlackAccess &&access) {
		std::size_t result = 0;
		for (; n; n = n->left) {
			if (access.get(*n) == color::black) {
				++result;
			}
		}
		return result;
	}
	/// Joins the two given trees and the node. Both trees must be non-empty.
	///
	/// \param left The tree whose nodes are ordered before \p mid.
	/// \param right The tree whose nodes are ordered after \p mid.
	/// \param mid A single node between \p left and \p right.
	/// \return The new root.
	template <typename Node, typename RedBlackAccess, typename Synth> Node *join(
		Node *left, Node *right, Node *mid, RedBlackAccess &&access, Synth &&synth
	) {
		using _tree = binary_tree<typename Node::value_type, typename Node::synth_data_type, lacks_synthesizer>;

		assert_true_logical(mid != nullptr, "null node used for join operation");
		assert_true_logical(
			mid->parent == nullptr && mid->left == nullptr && mid->right == nullptr,
			"mid must be an independent node"
		);
		assert_true_logical(
			(left == nullptr || left->parent == nullptr) && (right == nullptr || right->parent == nullptr),
			"left and right must be full trees, not subtrees"
		);

		// handle special cases
		if (left == nullptr) {
			if (right == nullptr) { // mid is the only node
				access.set(*mid, color::black);
				synth(*mid);
				return mid;
			}
			// inert at leftmost of right
			Node *insert = right;
			for (; insert->left; insert = insert->left) {
			}
			insert->left = mid;
			mid->parent = insert;
			_tree::refresh_synthesized_result(mid, synth);
			return post_insert_fixup_notree(
				mid, right, std::forward<RedBlackAccess>(access), std::forward<Synth>(synth)
			);
		} else if (right == nullptr) {
			// insert at rightmost of left
			Node *insert = left;
			for (; insert->right; insert = insert->right) {
			}
			insert->right = mid;
			mid->parent = insert;
			_tree::refresh_synthesized_result(mid, synth);
			return post_insert_fixup_notree(
				mid, left, std::forward<RedBlackAccess>(access), std::forward<Synth>(synth)
			);
		}

		// both trees are non-empty
		std::size_t
			left_depth = black_depth(left, access),
			right_depth = black_depth(right, access);
		if (left_depth == right_depth) {
			access.set(*mid, color::black);
			mid->left = left;
			mid->right = right;
			left->parent = right->parent = mid;
			synth(*mid);
			return mid;
		}

		Node *new_root = nullptr;
		if (left_depth < right_depth) {
			std::size_t diff = right_depth - left_depth;
			Node *pivot = right;
			for (std::size_t i = 0; i < diff; ++i) {
				pivot = pivot->left;
				while (access.get(*pivot) == color::red) {
					pivot = pivot->left;
				}
			}

			// insert the node and subtree
			mid->left = left;
			mid->right = pivot;
			mid->parent = pivot->parent;
			mid->parent->left = mid;
			left->parent = pivot->parent = mid;
			new_root = right;
		} else {
			std::size_t diff = left_depth - right_depth;
			Node *pivot = left;
			for (std::size_t i = 0; i < diff; ++i) {
				pivot = pivot->right;
				while (access.get(*pivot) == color::red) {
					pivot = pivot->right;
				}
			}

			// insert the node and subtree
			mid->left = pivot;
			mid->right = right;
			mid->parent = pivot->parent;
			mid->parent->right = mid;
			right->parent = pivot->parent = mid;
			new_root = left;
		}

		_tree::refresh_synthesized_result(mid, synth);
		return post_insert_fixup_notree(mid, new_root, std::forward<RedBlackAccess>(access), std::forward<Synth>(synth));
	}

	/// Splits the tree into two subtrees and an extra node. The caller should keep references to the input node
	/// because it will be isolated from the two resulting trees and will be lost otherwise.
	template <typename Node, typename RedBlackAccess, typename Synth> std::pair<Node*, Node*> split(
		Node *n, RedBlackAccess &&access, Synth &&synth
	) {
		using _tree = binary_tree<typename Node::value_type, typename Node::synth_data_type, lacks_synthesizer>;

		// resulting left & right trees, assumed to have black roots
		Node *left = n->left, *right = n->right;
		if (left) {
			n->left = nullptr;
			left->parent = nullptr;
			access.set(*left, color::black);
		}
		if (right) {
			n->right = nullptr;
			right->parent = nullptr;
			access.set(*right, color::black);
		}
		if (n->parent == nullptr) {
			return { left, right };
		}

		Node *pivot = n->parent;
		n->parent = nullptr;
		bool parent_left = n == pivot->left;
		(parent_left ? pivot->left : pivot->right) = nullptr; // n is dead after this point
		while (true) {
			Node *join_left = nullptr, *join_right = nullptr, **join_target = nullptr;
			if (parent_left) { // join right
				join_left = right;
				join_right = pivot->right;
				join_target = &right;

				if (join_right) {
					pivot->right = nullptr;
					join_right->parent = nullptr;
					access.set(*join_right, color::black);
				}
			} else { // join left
				join_left = pivot->left;
				join_right = left;
				join_target = &left;

				if (join_left) {
					pivot->left = nullptr;
					join_left->parent = nullptr;
					access.set(*join_left, color::black);
				}
			}

			// update loop variables before merging
			Node *next_pivot = pivot->parent;
			if (next_pivot) {
				parent_left = pivot == next_pivot->left;
				pivot->parent = nullptr;
				(parent_left ? next_pivot->left : next_pivot->right) = nullptr;
			}

			// merge the two trees
			*join_target = join(join_left, join_right, pivot, access, synth);
			check_integrity(*join_target, access);

			if (next_pivot == nullptr) {
				break;
			}
			pivot = next_pivot;
		}
		return { left, right };
	}


	// convenience wrapper
	/// A red-black tree. Although it's possible to directly access the underlying binary tree, modifying the data
	/// may corrupt the red-black tree structure and is not recommended.
	template <
		typename T, typename RedBlackAccess,
		typename AdditionalData = no_data, typename Synth = default_synthesizer<AdditionalData>
	> class tree : protected binary_tree<T, AdditionalData, Synth> {
	public:
		using binary_tree_t = binary_tree<T, AdditionalData, Synth>; ///< The base binary tree type.

		using typename binary_tree_t::node;
		using typename binary_tree_t::iterator;
		using typename binary_tree_t::const_iterator;
		using typename binary_tree_t::reverse_iterator;
		using typename binary_tree_t::const_reverse_iterator;

		using binary_tree_t::begin;
		using binary_tree_t::end;
		using binary_tree_t::rbegin;
		using binary_tree_t::rend;
		using binary_tree_t::cbegin;
		using binary_tree_t::cend;
		using binary_tree_t::crbegin;
		using binary_tree_t::crend;


		/// Default constructor.
		tree() = default;
		/// Initializes this tree with the given synthesizer and red black accessor.
		explicit tree(Synth synth, RedBlackAccess access = RedBlackAccess()) :
			binary_tree_t(std::move(synth)), _rb_access(std::move(access)) {
		}


		/// Constructs a node in-place before the given position, using a custom synthesizer.
		template <typename MySynth, typename ...Args> iterator emplace_before_custom_synth(
			const_iterator it, MySynth &&synth, Args &&...args
		) {
			iterator res = binary_tree_t::emplace_before_custom_synth(it, synth, std::forward<Args>(args)...);
			post_insert_fixup<binary_tree_t>(res, _rb_access, std::forward<MySynth>(synth));
			return res;
		}
		/// Constructs a node in-place before the given position, using the default synthesizer.
		template <typename ...Args> iterator emplace_before(const_iterator it, Args &&...args) {
			return emplace_before_custom_synth(it, this->get_synthesizer(), std::forward<Args>(args)...);
		}

		/// Erases the given node using a custom synthesizer.
		template <typename MySynth> iterator erase_custom_synth(const_iterator it, MySynth &&synth) {
			assert_true_usage(it.get_container() == this, "iterator is for another tree");
			node *n = it.get_node();
			assert_true_usage(n != nullptr, "trying to erase empty iterator");
			node *next = detach(
				*static_cast<binary_tree_t*>(this), it.get_node(), _rb_access, std::forward<MySynth>(synth)
			);
			delete n;
			return this->get_iterator_for(next);
		}
		/// Erases the given node using the default synthesizer.
		iterator erase(const_iterator it) {
			return erase_custom_synth(it, this->get_synthesizer());
		}
		/// Erases a range of elements in the tree, performing fixup using the default synthesizer.
		void erase(const_iterator begin, const_iterator end) {
			[[maybe_unused]] tree discarded = split_range(begin, end);
		}


		using binary_tree_t::find;

		using binary_tree_t::refresh_synthesized_result;
		using binary_tree_t::refresh_tree_synthesized_result;

		using binary_tree_t::clear;
		using binary_tree_t::empty;

		using binary_tree_t::get_iterator_for;
		using binary_tree_t::get_const_iterator_for;
		using binary_tree_t::get_modifier_for;

		using binary_tree_t::root;


		/// Merges two trees using the middle node (which must be isolated), and returns the new tree. The red black
		/// access and synthesizer of the left tree are used for this operation and the new tree.
		[[nodiscard]] inline static tree join_trees(tree left, tree right, node *center) {
			node *left_root = left.mutable_root(), *right_root = right.mutable_root();
			left.mutable_root() = nullptr;
			right.mutable_root() = nullptr;

			node *new_root = join(left_root, right_root, center, left._rb_access, left._synth);
			tree result(std::move(left._synth), std::move(left._rb_access));
			result.mutable_root() = new_root;

			return result;
		}
		/// Constructs a new node using the given arguments, then merges two trees using that node.
		template <typename ...Args> [[nodiscard]] inline static tree join_trees(
			tree left, tree right, Args &&...center
		) {
			return join_trees(std::move(left), std::move(right), new node(std::forward<Args>(center)...));
		}

		/// Splits a tree into one tree containing all nodes before the given node, one tree containing all nodes
		/// after the given node, and the node itself.
		[[nodiscard]] inline static std::pair<tree, tree> split_tree(tree t, node &n) {
			t.mutable_root() = nullptr;
			auto [root_left, root_right] = red_black_tree::split(&n, t._rb_access, t._synth);

			tree left(t._synth, t._rb_access);
			tree right(std::move(t._synth), std::move(t._rb_access));
			left.mutable_root() = root_left;
			right.mutable_root() = root_right;

			return std::make_pair(std::move(left), std::move(right));
		}
		/// Convenience wrapper around \ref split_tree().
		[[nodiscard]] std::tuple<tree, node*, tree> split_at(const_iterator n) && {
			assert_true_logical(n.get_container() == this);
			if (n.get_node() == nullptr) { // n == end
				tree right(this->_synth, _rb_access);
				return std::make_tuple(std::move(*this), nullptr, std::move(right));
			}
			std::pair<tree, tree> split = split_tree(std::move(*this), *n.get_node());
			return std::make_tuple(std::move(split.first), n.get_node(), std::move(split.second));
		}

		/// Inserts the tree before the given position.
		void insert_range(tree t, const_iterator it) {
			assert_true_logical(it.get_container() == this, "iterator does not belong to this tree");

			node *split_point = it.get_node(), *detached = t.mutable_root();
			if (detached == nullptr) {
				return;
			}
			for (; detached->left; detached = detached->left) {
			}
			detach(t.raw_tree(), detached, t._rb_access, t.get_synthesizer());
			if (split_point == nullptr) {
				this->mutable_root() = join(
					this->mutable_root(), t.mutable_root(), detached, _rb_access, this->get_synthesizer()
				);
			} else {
				auto [left, right] = split(split_point, _rb_access, this->get_synthesizer());
				node *n = join(t.mutable_root(), right, split_point, _rb_access, this->get_synthesizer());
				n = join(left, n, detached, _rb_access, this->get_synthesizer());
				this->mutable_root() = n;
			}
			t.mutable_root() = nullptr;
		}

		/// Detaches the given range of nodes from the tree as a new tree.
		[[nodiscard]] tree split_range(const_iterator beg, const_iterator end) {
			assert_true_logical(
				beg.get_container() == this && end.get_container() == this,
				"iterators belong to other trees"
			);

			node *beg_node = beg.get_node(), *end_node = end.get_node();
			if (beg_node == end_node) { // empty range
				return tree(this->_synth, _rb_access);
			}

			// TODO check if beg_node is the first node and skip the following steps
			// split at beg_node
			auto [leftmost, midright] = split(beg_node, _rb_access, this->get_synthesizer());

			// insert beg_node back into midright
			if (midright) {
				node *insert = midright;
				for (; insert->left; insert = insert->left) {
				}
				insert->left = beg_node;
				beg_node->parent = insert;
				this->refresh_synthesized_result(beg_node);
				midright = post_insert_fixup_notree(beg_node, midright, _rb_access, this->get_synthesizer());
			} else {
				midright = beg_node;
				this->get_synthesizer()(*midright);
			}

			node *result_root = nullptr;
			if (end_node != nullptr) {
				// split at end_node
				auto [mid, rightmost] = split(end_node, _rb_access, this->get_synthesizer());
				this->mutable_root() = join(leftmost, rightmost, end_node, _rb_access, this->get_synthesizer());
				result_root = mid;
			} else {
				this->mutable_root() = leftmost;
				result_root = midright;
			}
			tree result(this->_synth, _rb_access);
			result.mutable_root() = result_root;
			return result;
		}


		/// Checks the integrity of this red-black tree.
		void check_integrity() const {
			red_black_tree::check_integrity(root(), _rb_access);
		}

		/// Returns a reference to the underlying tree that allows for direct structural modifications. Use with
		/// caution.
		[[nodiscard]] binary_tree_t &raw_tree() {
			return *this;
		}
	protected:
		[[no_unique_address]] RedBlackAccess _rb_access; ///< Used to access the red- and black-ness of a node.
	};
}
