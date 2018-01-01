#pragma once

#include <functional>

#include "misc.h"

namespace codepad {
	template <typename T, typename Hash = std::hash<T>, typename Equal = std::equal_to<T>> class probing_hash_set {
	protected:
		constexpr static size_t _null = static_cast<size_t>(-1);

		struct _wrapper {
			T val;
			std::ptrdiff_t next_diff = 0;
			bool has_val = false;
		};
	public:
		probing_hash_set() = default;
		probing_hash_set(probing_hash_set &&src) : _arr(src._arr), _end(src._end), _used(src._used) {
			src._arr = src._end = nullptr;
			src._used = 0;
		}
		probing_hash_set(const probing_hash_set &src) : probing_hash_set() {
			// TODO
			std::abort();
		}
		probing_hash_set &operator=(probing_hash_set &&src) {
			std::swap(_arr, src._arr);
			std::swap(_end, src._end);
			std::swap(_used, src._used);
			return *this;
		}
		probing_hash_set &operator=(const probing_hash_set &&src) {
			// TODO
			std::abort();
			return *this;
		}
		virtual ~probing_hash_set() {
			clear();
			if (_arr) {
				std::free(_arr);
			}
		}

		template <bool Const> struct iterator_base {
			friend class probing_hash_set<T, Hash, Equal>;
		public:
			using dereferenced_type = std::conditional_t<Const, const T, T>;

			iterator_base() = default;

			friend bool operator==(const iterator_base &lhs, const iterator_base &rhs) {
				return lhs._ptr == rhs._ptr;
			}
			friend bool operator!=(const iterator_base &lhs, const iterator_base &rhs) {
				return !(lhs == rhs);
			}

			dereferenced_type &operator*() const {
				return _ptr->val;
			}
			dereferenced_type *operator->() const {
				return &operator*();
			}

			iterator_base &operator++() {
				_ptr += _ptr->next_diff;
				return *this;
			}
			iterator_base operator++(int) {
				iterator_base res = *this;
				++*this;
				return res;
			}
		protected:
			using _node_t = std::conditional_t<Const, const _wrapper, _wrapper>;

			explicit iterator_base(_node_t *p) : _ptr(p) {
			}

			_node_t *_ptr = nullptr;
		};
		using iterator = iterator_base<true>;

		iterator begin() const {
			return _make_iterator<iterator>(_end ? _end + _end->next_diff : nullptr);
		}
		iterator end() const {
			return _make_iterator<iterator>(_end);
		}

		iterator insert(T obj) {
			size_t nbk = _num_buckets();
			if (nbk == 0) {
				_resize(nbk = _min_buckets);
			} else if (_load_too_large(_used, nbk)) {
				_resize(nbk = _enlarge_bucket_num(nbk));
			}
			size_t hash = Hash()(obj);
			for (size_t i = 0; ; ++i) {
				_wrapper *buck = _arr + _hash_to_bucket(hash + i * i, nbk);
				if (!buck->has_val) {
					buck->has_val = true;
					new (&buck->val) T(std::move(obj));
					_wrapper *fst = _end + _end->next_diff;
					buck->next_diff = fst - buck;
					_end->next_diff = buck - _end;
					++_used;
					return _make_iterator<iterator>(buck);
				} else {
					++colp;
					if (i == 0) {
						++col;
					}
					logger::get().log_info(CP_HERE, "total ", _used, " colp ", colp, " col ", col);
				}
			}
		}
		iterator find(const T &obj) const {
			return _make_iterator<iterator>(_find(obj, Hash(), Equal()));
		}

		void clear() {
			if (_used > 0) {
				for (_wrapper *wp = _end + _end->next_diff; wp != _end; wp += wp->next_diff) {
					wp->val.~T();
					wp->has_val = false;
				}
			}
			_end->next_diff = 0;
			_used = 0;
		}

		size_t size() const {
			return _used;
		}
		size_t capacity() const {
			return _num_buckets();
		}

		size_t col = 0, colp = 0;
	protected:
		constexpr static size_t _min_buckets = 1 << 10;
		inline static size_t _enlarge_bucket_num(size_t b) {
			return b << 1;
		}
		inline static size_t _hash_to_bucket(size_t h, size_t b) {
			return h & (b - 1);
		}
		inline static bool _load_too_large(size_t h, size_t b) {
			return h > ((b >> 1) | (b >> 2)); // 0.75
		}

		template <typename Iter, typename Node> inline static Iter _make_iterator(Node *w) {
			return Iter(w);
		}

		size_t _num_buckets() const {
			return _end - _arr;
		}
		void _resize(size_t nbuck) {
			size_t oldbuknum = _num_buckets();
			assert_true_logical(nbuck > oldbuknum);
			size_t memsz = sizeof(_wrapper) * (nbuck + 1);
			_wrapper *narr = static_cast<_wrapper*>(std::malloc(memsz)), *nend = narr + nbuck;
			std::memset(narr, 0, memsz);
			if (oldbuknum > 0) {
				if (_used > 0) {
					Hash hash;
					_wrapper *prev = nend;
					for (_wrapper *cw = _end + _end->next_diff; cw != _end; cw += cw->next_diff) {
						size_t hashv = hash(cw->val);
						for (size_t i = 0; ; ++i) {
							_wrapper *newbuck = narr + _hash_to_bucket(hashv + i * i, oldbuknum);
							if (!newbuck->has_val) {
								newbuck->has_val = true;
								new (&newbuck->val) T(std::move(cw->val));
								cw->val.~T();
								prev->next_diff = newbuck - prev;
								prev = newbuck;
								break;
							}
						}
					}
					prev->next_diff = nend - prev;
				}
				std::free(_arr);
			}
			_arr = narr;
			_end = nend;
		}
		template <typename K, typename HashT, typename EqualT> _wrapper *_find(
			const K &obj, HashT &&hash, EqualT &&equal
		) const {
			if (_arr) {
				size_t hv = hash(obj), buckets = _num_buckets();
				for (size_t i = 0; ; ++i) {
					_wrapper *buck = _arr + _hash_to_bucket(hv + i * i, buckets);
					if (!buck->has_val) {
						break;
					}
					if (equal(obj, buck->val)) {
						return buck;
					}
				}
			}
			return _end;
		}

		_wrapper *_arr = nullptr, *_end = nullptr;
		size_t _used = 0;
	};

	namespace _details {
		template <typename Hash> struct key_hasher {
			template <typename PairT> size_t operator()(const PairT &p) {
				return Hash()(p.first);
			}
		};
		template <typename Equal> struct key_equal {
			template <typename Pair1, typename Pair2> size_t operator()(const Pair1 &lhs, const Pair2 &rhs) {
				return Equal()(lhs.first, rhs.first);
			}
		};
	}
	template <
		typename Key, typename Value, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>
	> class probing_hash_map : public probing_hash_set<
		std::pair<const Key, Value>, _details::key_hasher<Hash>, _details::key_equal<Equal>
	> {
	protected:
		struct _find_equals {
			bool operator()(const Key &k, const std::pair<Key, Value> &p) const {
				return actual_equal(k, p.first);
			}
			Equal actual_equal;
		};
		using _base = probing_hash_set<
			std::pair<const Key, Value>, _details::key_hasher<Hash>, _details::key_equal<Equal>
		>;
	public:
		using iterator = typename _base::template iterator_base<false>;
		using const_iterator = typename _base::template iterator_base<true>;

		iterator begin() {
			return _base::template _make_iterator<iterator>(
				_base::_end ? _base::_end + _base::_end->next_diff : nullptr
			);
		}
		iterator end() {
			return _base::template _make_iterator<iterator>(_base::_end);
		}

		iterator find(const Key &k) {
			return _base::template _make_iterator<iterator>(_find(k, Hash(), _find_equals()));
		}
		const_iterator find(const Key &k) const {
			return _base::template _make_iterator<const_iterator>(_find(k, Hash(), _find_equals()));
		}
	};
}
