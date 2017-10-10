#pragma once

#include <functional>

#include "misc.h"

namespace codepad {
	struct no_data {
		template <typename Node> void synthesize(const Node&) {
		}
	};
	template <typename T> struct default_synthesizer {
		template <typename N> void operator()(T &obj, const N &node) const {
			obj.synthesize(node);
		}
	};
	template <typename Comp> struct bst_branch_selector {
	public:
		template <typename Node> bool select_insert(const Node &cur, const Node &inserting) const {
			return _comp(inserting.value, cur.value);
		}
		template <typename Node, typename U> int select_find(const Node &cur, const U &v) const {
			if (_comp(v, cur.value)) {
				return -1;
			} else if (_comp(cur.value, v)) {
				return 1;
			}
			return 0;
		}
	protected:
		Comp _comp;
	};
	template <typename T, typename AdditionalData = no_data> struct binary_tree_node {
		template <typename ...Args> explicit binary_tree_node(Args &&...args) : value(std::forward<Args>(args)...) {
		}

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

		T value;
		AdditionalData synth_data;
		binary_tree_node *left = nullptr, *right = nullptr, *parent = nullptr;
	};
	template <
		typename T, typename AdditionalData = no_data, typename Synth = default_synthesizer<AdditionalData>
	> struct binary_tree {
	public:
		using node = binary_tree_node<T, AdditionalData>;
		template <typename Cont, typename Node> struct iterator_base {
			friend struct binary_tree<T, AdditionalData, Synth>;
		public:
			using value_type = decltype(Node::value);
			using dereferenced_type = std::conditional_t<
				std::is_const<Cont>::value, const value_type, value_type
			>;
			using reference = dereferenced_type&;
			using pointer = dereferenced_type*;
			using iterator_category = std::bidirectional_iterator_tag;

			iterator_base() = default;
			iterator_base(const iterator_base<binary_tree, node> &it) :
				_con(it._con), _n(it._n) {
			}

			friend bool operator==(const iterator_base &lhs, const iterator_base &rhs) {
				return lhs._con == rhs._con && lhs._n == rhs._n;
			}
			friend bool operator!=(const iterator_base &lhs, const iterator_base &rhs) {
				return !(lhs == rhs);
			}

			iterator_base &operator++() {
				assert_true_logical(_n != nullptr, "cannot increment iterator");
				_n = _n->next();
				return *this;
			}
			iterator_base operator++(int) {
				iterator_base ov = *this;
				++*this;
				return ov;
			}
			iterator_base &operator--() {
				if (_n) {
					_n = _n->prev();
					assert_true_logical(_n != nullptr, "cannot decrement iterator");
				} else {
					_n = _con->max();
				}
				return *this;
			}
			iterator_base operator--(int) {
				iterator_base ov = *this;
				--*this;
				return ov;
			}

			reference operator*() const {
				return _n->value;
			}
			pointer operator->() const {
				return &_n->value;
			}
			Node *get_node() const {
				return _n;
			}
			Cont *get_container() const {
				return _con;
			}
		protected:
			iterator_base(Cont *c, Node *n) : _con(c), _n(n) {
			}

			Cont *_con = nullptr;
			Node *_n = nullptr;
		};
		using iterator = iterator_base<binary_tree, node>;
		using const_iterator = iterator_base<const binary_tree, node>;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<iterator>;

		binary_tree() = default;
		template <typename ...Args> binary_tree(std::vector<T> v, Args &&...args) : _synth(std::forward<Args>(args)...) {
			_root = _build_tree(v.begin(), v.end(), _synth);
		}
		binary_tree(const binary_tree &tree) : _root(clone_tree(tree._root)) {
		}
		binary_tree(binary_tree &&tree) : _root(tree._root) {
			tree._root = nullptr;
		}
		binary_tree &operator=(const binary_tree &tree) {
			delete_tree(_root);
			_root = clone_tree(tree._root);
			return *this;
		}
		binary_tree &operator=(binary_tree &&tree) {
			std::swap(_root, tree._root);
			return *this;
		}
		~binary_tree() {
			delete_tree(_root);
		}

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
		template <typename Comp, typename ...Args> iterator insert(Args &&...args) {
			return insert_custom(bst_branch_selector<Comp>(), std::forward<Args>(args)...);
		}
		void insert_before(node *n, node *before) {
			if (before == nullptr) {
				before = max();
				if (before) {
					before->right = n;
				} else {
					_root = n;
				}
			} else if (before->left) {
				for (before = before->left; before->right; before = before->right) {
				}
				before->right = n;
			} else {
				before->left = n;
			}
			n->parent = before;
			refresh_synthesized_result(before);
		}

		template <typename BranchSelector, typename Ref> iterator find_custom(BranchSelector &&b, Ref &&ref) {
			return _find_custom_impl<iterator>(this, std::forward<BranchSelector>(b), std::forward<Ref>(ref));
		}
		template <typename BranchSelector, typename Ref> const_iterator find_custom(BranchSelector &&b, Ref &&ref) const {
			return _find_custom_impl<const_iterator>(this, std::forward<BranchSelector>(b), std::forward<Ref>(ref));
		}

		iterator get_iterator_for(node *n) {
			return iterator(this, n);
		}
		const_iterator get_iterator_for(node *n) const {
			return const_iterator(this, n);
		}

		iterator begin() {
			return iterator(this, min());
		}
		const_iterator begin() const {
			return cbegin();
		}
		const_iterator cbegin() const {
			return const_iterator(this, min());
		}
		iterator end() {
			return iterator(this, nullptr);
		}
		const_iterator end() const {
			return cend();
		}
		const_iterator cend() const {
			return const_iterator(this, nullptr);
		}
		reverse_iterator rbegin() {
			return reverse_iterator(end());
		}
		const_reverse_iterator rbegin() const {
			return crbegin();
		}
		const_reverse_iterator crbegin() const {
			return reverse_iterator(end());
		}
		reverse_iterator rend() {
			return reverse_iterator(begin());
		}
		const_reverse_iterator rend() const {
			return crend();
		}
		const_reverse_iterator crend() const {
			return reverse_iterator(begin());
		}

		void refresh_synthesized_result(node *n) {
			for (; n; n = n->parent) {
				_update_synth(n);
			}
		}
		template <typename NewSynth> void synthesize_root_path(const node *n, NewSynth &&v) const {
			if (n) {
				for (node *p = n->parent; p; n = p, p = p->parent) {
					v(*p, *n);
				}
			}
		}
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
			_update_synth(n);
		}
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
			_update_synth(n);
		}
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

		node *min() const {
			return min(_root);
		}
		node *min(node *n) const {
			for (; n && n->left; n = n->left) {
			}
			return n;
		}
		node *max() const {
			return max(_root);
		}
		node *max(node *n) const {
			for (; n && n->right; n = n->right) {
			}
			return n;
		}
		node *root() const {
			return _root;
		}

		void erase(node *n) {
			if (n == nullptr) {
				return;
			}
			node *oc;
			if (n->left && n->right) {
				node *rmin = min(n->right);
				n->value = std::move(rmin->value);
				n = rmin;
				oc = n->right;
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
		void erase(node *beg, node *end) {
			if (beg == nullptr) {
				return;
			}
			beg = beg->prev();
			if (beg && end) {
				splay(beg);
				splay(end, beg);
				assert_true_logical(end == beg->right, "invalid range");
				delete_tree(end->left);
				end->left = nullptr;
				_update_synth(end);
				_update_synth(beg);
			} else if (beg) {
				splay(beg);
				delete_tree(beg->right);
				beg->right = nullptr;
				_update_synth(beg);
			} else if (end) {
				splay(end);
				delete_tree(end->left);
				end->left = nullptr;
				_update_synth(end);
			} else {
				clear();
			}
		}
		void erase(iterator it) {
			erase(it.get_node());
		}
		void erase(iterator beg, iterator end) {
			erase(beg.get_node(), end.get_node());
		}

		void clear() {
			delete_tree(_root);
			_root = nullptr;
		}

		node *build_tree(std::vector<T> objs) const {
			return build_tree(std::move(objs), _synth);
		}

		template <typename SynRef> void set_synthesizer(SynRef &&s) {
			_synth = std::forward<SynRef>(s);
		}
		const Synth &get_synthesizer() const {
			return _synth;
		}

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
		template <typename SynRef> inline static node *build_tree(std::vector<T> objs, SynRef &&synth) {
			return _build_tree(objs.begin(), objs.end(), std::forward<SynRef>(synth));
		}
	protected:
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

		node *_root = nullptr;
		Synth _synth;

		template <typename SynRef> inline static void _update_synth(SynRef &&sy, node *n) {
			sy(n->synth_data, *n);
		}
		void _update_synth(node *n) {
			_update_synth(_synth, n);
		}

		struct _clone_node {
			_clone_node() = default;
			_clone_node(const node *s, node *p, node **a) : src(s), parent(p), assign(a) {
			}

			const node *src = nullptr;
			node *parent = nullptr, **assign = nullptr;
		};

		// items will be moved out of the container
		template <typename SynRef> inline static node *_build_tree(
			typename std::vector<T>::iterator beg, typename std::vector<T>::iterator end, SynRef &&s
		) {
			if (beg == end) {
				return nullptr;
			}
			auto half = beg + (end - beg) / 2;
			node
				*left = _build_tree(beg, half, s), *right = _build_tree(half + 1, end, s),
				*cur = new node(std::move(*half));
			cur->left = left;
			cur->right = right;
			if (cur->left) {
				cur->left->parent = cur;
			}
			if (cur->right) {
				cur->right->parent = cur;
			}
			_update_synth(std::forward<SynRef>(s), cur);
			return cur;
		}
	};
}
