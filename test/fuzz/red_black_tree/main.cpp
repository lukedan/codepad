// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Stress-test for the red-black tree. Randomly inserts, erases, joins, and splits trees, and compares the result
/// with those obtained from `dumb' algorithms.

#include <csignal>
#include <random>
#include <atomic>

#include <codepad/core/fuzz_test.h>
#include <codepad/core/red_black_tree.h>

namespace cp = codepad;

/// Data stored in a tree node.
struct node_data {
	/// Default constructor.
	node_data() = default;
	/// Initializes \ref value.
	explicit node_data(int v) : value(v) {
	}

	int value = 0; ///< The value of this node.
	cp::red_black_tree::color color = cp::red_black_tree::color::black; ///< The color of this node.
};
/// Synthesized data for a subtree.
struct synth_data {
	/// A node of the tree.
	using node = cp::binary_tree_node<node_data, synth_data>;

	std::size_t num_nodes = 0; ///< The number of nodes in this subtree.

	/// The property used to compute \ref num_nodes.
	using num_nodes_property = cp::sum_synthesizer::compact_property<
		cp::synthesization_helper::identity, &synth_data::num_nodes
	>;

	/// Refreshes the data of the given node.
	inline static void synthesize(node &n) {
		cp::sum_synthesizer::synthesize<num_nodes_property>(n);
	}
};

/// The red black tree type.
using tree = cp::red_black_tree::tree<
	node_data, cp::red_black_tree::member_red_black_access<&node_data::color>, synth_data
>;

/// Builds a tree from the given integers.
[[nodiscard]] tree build_tree(const std::vector<int> &values) {
	tree result;
	for (int x : values) {
		result.emplace_before(result.end(), x);
	}
	return result;
}
/// Returns an iterator to the node in the tree at the given index.
[[nodiscard]] tree::const_iterator at(const tree &t, std::size_t index) {
	return t.find(cp::sum_synthesizer::index_finder<synth_data::num_nodes_property>(), index);
}

/// Various testing operations.
enum class operations : unsigned char {
	insert, ///< Insertion of one element into a tree.
	insert_subtree, ///< Insertion of a range of elements into a tree.
	insert_tree, ///< Insertion of a new tree into the array of trees.
	erase, ///< Removal of one element from a tree.
	erase_subtree, ///< Removal of a range of elements from a tree.
	split, ///< Splitting a tree between a pivot which is discarded.
	join, ///< Joining two trees using a new element.

	max_index ///< Total number of operations.
};

/// Possible range of test operations.
std::pair<int, int> op_range{ 0, static_cast<int>(operations::max_index) - 1 };
/// The range of the number of inserted elements for each operation.
std::pair<std::size_t, std::size_t> insert_count_range{1, 1000};

/// The fuzz test class.
class red_black_tree_test : public cp::fuzz_test {
public:
	/// Returns the name of this test.
	std::u8string_view get_name() const override {
		return u8"red_black_tree_test";
	}

	/// Executes one test operation.
	void iterate() override {
		auto op = static_cast<operations>(random_int(op_range));
		if (_reference_data.empty()) {
			op = operations::insert_tree;
		}
		switch (op) {
		case operations::insert:
			{
				std::size_t major_index = random_index(_reference_data);
				std::size_t minor_index = random_insertion_index(_reference_data[major_index]);

				int test_value = rng();
				// test
				auto &test_tree = _test_data[major_index];
				test_tree.emplace_before(at(test_tree, minor_index), test_value);
				// reference
				auto &ref_arr = _reference_data[major_index];
				ref_arr.insert(ref_arr.begin() + minor_index, test_value);
			}
			break;
		case operations::insert_subtree:
			{
				std::size_t major_index = random_index(_reference_data);
				std::size_t minor_index = random_insertion_index(_reference_data[major_index]);
				std::vector<int> test_values(random_int(insert_count_range));
				for (int &v : test_values) {
					v = rng();
				}
				// test
				tree &test_tree = _test_data[major_index];
				test_tree.insert_range(build_tree(test_values), at(test_tree, minor_index));
				// reference
				auto &ref_arr = _reference_data[major_index];
				ref_arr.insert(ref_arr.begin() + minor_index, test_values.begin(), test_values.end());
			}
			break;
		case operations::insert_tree:
			{
				std::size_t major_index = random_insertion_index(_reference_data);
				std::vector<int> test_values(random_int(insert_count_range));
				for (int &v : test_values) {
					v = rng();
				}
				// test
				_test_data.insert(_test_data.begin() + major_index, build_tree(test_values));
				// reference
				_reference_data.insert(_reference_data.begin() + major_index, std::move(test_values));
			}
			break;

		case operations::erase:
			{
				std::size_t major_index = random_index(_reference_data);
				std::size_t minor_index = random_index(_reference_data[major_index]);
				// test
				auto &test_tree = _test_data[major_index];
				test_tree.erase(at(test_tree, minor_index));
				if (test_tree.empty()) {
					_test_data.erase(_test_data.begin() + major_index);
				}
				// reference
				auto &ref_arr = _reference_data[major_index];
				ref_arr.erase(ref_arr.begin() + minor_index);
				if (ref_arr.empty()) {
					_reference_data.erase(_reference_data.begin() + major_index);
				}
			}
			break;
		case operations::erase_subtree:
			{
				std::size_t major_index = random_index(_reference_data);
				std::size_t minor_index_beg = random_index(_reference_data[major_index]);
				std::size_t minor_index_end = random_index(_reference_data[major_index]);
				if (minor_index_beg > minor_index_end) {
					std::swap(minor_index_beg, minor_index_end);
				}
				++minor_index_end; // move past the end
				// test
				auto &test_tree = _test_data[major_index];
				auto beg_it = at(test_tree, minor_index_beg), end_it = at(test_tree, minor_index_end);
				test_tree.erase(beg_it, end_it);
				if (test_tree.empty()) {
					_test_data.erase(_test_data.begin() + major_index);
				}
				// reference
				auto &ref_arr = _reference_data[major_index];
				ref_arr.erase(ref_arr.begin() + minor_index_beg, ref_arr.begin() + minor_index_end);
				if (ref_arr.empty()) {
					_reference_data.erase(_reference_data.begin() + major_index);
				}
			}
			break;

		case operations::split:
			{
				std::size_t major_index = random_index(_reference_data);
				std::size_t minor_index = random_index(_reference_data[major_index]);
				// test
				auto &test_tree = _test_data[major_index];
				auto iter = at(test_tree, minor_index);
				auto [left_tree, mid, right_tree] = std::move(test_tree).split_at(iter);
				if (right_tree.empty()) {
					_test_data.erase(_test_data.begin() + major_index);
				} else {
					_test_data[major_index] = std::move(right_tree);
				}
				if (!left_tree.empty()) {
					_test_data.insert(_test_data.begin() + major_index, std::move(left_tree));
				}
				delete mid;
				// reference
				auto &ref_arr = _reference_data[major_index];
				std::vector<int> second(ref_arr.begin() + (minor_index + 1), ref_arr.end());
				ref_arr.erase(ref_arr.begin() + minor_index, ref_arr.end());
				if (!second.empty()) {
					_reference_data.insert(_reference_data.begin() + major_index + 1, std::move(second));
				}
				if (_reference_data[major_index].empty()) {
					_reference_data.erase(_reference_data.begin() + major_index);
				}
			}
			break;
		case operations::join:
			{
				if (_reference_data.size() < 2) {
					return;
				}

				auto index = random_int<std::size_t>(0, _reference_data.size() - 2);
				int merge_value = rng();
				// test
				tree new_tree = tree::join_trees(
					std::move(_test_data[index]), std::move(_test_data[index + 1]), merge_value
				);
				_test_data[index] = std::move(new_tree);
				_test_data.erase(_test_data.begin() + (index + 1));
				// reference
				_reference_data[index].emplace_back(merge_value);
				_reference_data[index].insert(
					_reference_data[index].end(), _reference_data[index + 1].begin(), _reference_data[index + 1].end()
				);
				_reference_data.erase(_reference_data.begin() + (index + 1));
			}
			break;

		default:
			cp::assert_true_logical(false, "invalid operation");
		}

		// check that all data are consistent
		for (tree &t : _test_data) {
			t.check_integrity();
		}
		cp::assert_true_logical(_test_data.size() == _reference_data.size(), "number of trees different from ref");
		for (std::size_t i = 0; i < _test_data.size(); ++i) {
			const tree &t = _test_data[i];
			const auto &ref = _reference_data[i];

			cp::assert_true_logical(t.root() != nullptr, "empty array in list");
			cp::assert_true_logical(t.root()->synth_data.num_nodes == ref.size(), "different number of elements");

			{ // check synth data
				std::stack<const tree::node*> nodes;
				nodes.push(t.root());
				while (!nodes.empty()) {
					const tree::node *n = nodes.top();
					nodes.pop();

					std::size_t size = 1;
					if (n->left) {
						size += n->left->synth_data.num_nodes;
						nodes.emplace(n->left);
					}
					if (n->right) {
						size += n->right->synth_data.num_nodes;
						nodes.emplace(n->right);
					}
					cp::assert_true_logical(size == n->synth_data.num_nodes, "incorrect synth data");
				}
			}

			{ // check actual data
				auto it = t.begin();
				for (int val : ref) {
					cp::assert_true_logical(it->value == val, "incorrect value");
					++it;
				}
			}
		}
	}


	/// Generates a random index within the given \p std::vector.
	template <typename T> [[nodiscard]] std::size_t random_index(const std::vector<T> &vec) {
		return random_int<std::size_t>(0, vec.size() - 1);
	}
	/// Generates a random index within or at the end of the given \p std::vector.
	template <typename T> [[nodiscard]] std::size_t random_insertion_index(const std::vector<T> &vec) {
		return random_int<std::size_t>(0, vec.size());
	}
protected:
	std::vector<tree> _test_data; ///< Trees used for testing.
	std::vector<std::vector<int>> _reference_data; ///< Reference data.
};

/// Entry point of the game.
int main(int argc, char **argv) {
	return cp::fuzz_test::main(argc, argv, std::make_unique<red_black_tree_test>());
}
