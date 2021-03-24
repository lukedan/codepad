// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Stress-test for the red-black tree. Randomly inserts, erases, joins, and splits trees, and compares the result
/// with those obtained from `dumb' algorithms.

#include <csignal>
#include <random>
#include <atomic>

#include <codepad/core/logging.h>
#include <codepad/core/logger_sinks.h>
#include <codepad/core/red_black_tree.h>

using namespace codepad;

std::atomic_bool keep_running; ///< This is set to \p false if SIGINT is encountered.

/// Data stored in a tree node.
struct node_data {
	/// Default constructor.
	node_data() = default;
	/// Initializes \ref value.
	explicit node_data(int v) : value(v) {
	}

	int value = 0; ///< The value of this node.
	red_black_tree::color color = red_black_tree::color::black; ///< The color of this node.
};
/// Synthesized data for a subtree.
struct synth_data {
	/// A node of the tree.
	using node = binary_tree_node<node_data, synth_data>;

	std::size_t num_nodes = 0; ///< The number of nodes in this subtree.

	/// The property used to compute \ref num_nodes.
	using num_nodes_property = sum_synthesizer::compact_property<
		synthesization_helper::identity, &synth_data::num_nodes
	>;

	/// Refreshes the data of the given node.
	inline static void synthesize(node &n) {
		sum_synthesizer::synthesize<num_nodes_property>(n);
	}
};

/// The red black tree type.
using tree = red_black_tree::tree<node_data, red_black_tree::member_red_black_access<&node_data::color>, synth_data>;

/// Builds a tree from the given integers.
tree build_tree(const std::vector<int> &values) {
	tree result;
	for (int x : values) {
		result.emplace_before(result.end(), x);
	}
	return result;
}
/// Returns an iterator to the node in the tree at the given index.
tree::const_iterator at(const tree &t, std::size_t index) {
	return t.find(sum_synthesizer::index_finder<synth_data::num_nodes_property>(), index);
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

/// Generates a random number in the given *closed* interval.
template <typename T, typename Random> T random_between(T min, T max, Random &&r) {
	using _type = std::decay_t<T>;
	if constexpr (std::is_floating_point_v<_type>) {
		std::uniform_real_distribution<_type> dist(min, max);
		return dist(std::forward<Random>(r));
	} else {
		std::uniform_int_distribution<_type> dist(min, max);
		return dist(std::forward<Random>(r));
	}
}
/// Generates a random index within the given \p std::vector.
template <typename T, typename Random> std::size_t random_index(const std::vector<T> &vec, Random &&r) {
	return random_between<std::size_t>(0, vec.size() - 1, std::forward<Random>(r));
}
/// Generates a random index within or at the end of the given \p std::vector.
template <typename T, typename Random> std::size_t random_insertion_index(const std::vector<T> &vec, Random &&r) {
	return random_between<std::size_t>(0, vec.size(), std::forward<Random>(r));
}

/// Entry point of the game.
int main(int argc, char **argv) {
	// this enables us to terminate normally and check if there are any memory leaks
	keep_running = true;
	std::signal(SIGINT, [](int) {
		keep_running = false;
	});

	auto global_log = std::make_unique<logger>();
	global_log->sinks.emplace_back(std::make_unique<logger_sinks::console_sink>());
	global_log->sinks.emplace_back(std::make_unique<logger_sinks::file_sink>("buffer_test.log"));
	logger::set_current(std::move(global_log));

	initialize(argc, argv);

	std::uniform_int_distribution<int> op_distribution(0, static_cast<int>(operations::max_index) - 1);
	std::default_random_engine random(123456);

	std::vector<tree> test_data;
	std::vector<std::vector<int>> reference_data;

	std::size_t op_index = 0;
	while (keep_running) {
		auto op = static_cast<operations>(op_distribution(random));
		if (reference_data.empty()) {
			op = operations::insert_tree;
		}
		logger::get().log_info(CP_HERE) << "op index " << op_index;
		switch (op) {
		case operations::insert:
			{
				logger::get().log_info(CP_HERE) << "op insert";
				std::size_t major_index = random_index(reference_data, random);
				std::size_t minor_index = random_insertion_index(reference_data[major_index], random);

				int test_value = random();
				// test
				auto &test_tree = test_data[major_index];
				test_tree.emplace_before(at(test_tree, minor_index), test_value);
				// reference
				auto &ref_arr = reference_data[major_index];
				ref_arr.insert(ref_arr.begin() + minor_index, test_value);
			}
			break;
		case operations::insert_subtree:
			{
				logger::get().log_info(CP_HERE) << "op insert_subtree";
				std::size_t major_index = random_index(reference_data, random);
				std::size_t minor_index = random_insertion_index(reference_data[major_index], random);
				std::vector<int> test_values(random_between<std::size_t>(1, 1000, random));
				for (int &v : test_values) {
					v = random();
				}
				// test
				tree &test_tree = test_data[major_index];
				test_tree.insert_range(build_tree(test_values), at(test_tree, minor_index));
				// reference
				auto &ref_arr = reference_data[major_index];
				ref_arr.insert(ref_arr.begin() + minor_index, test_values.begin(), test_values.end());
			}
			break;
		case operations::insert_tree:
			{
				logger::get().log_info(CP_HERE) << "op insert_tree";
				std::size_t major_index = random_insertion_index(reference_data, random);
				std::vector<int> test_values(random_between<std::size_t>(1, 1000, random));
				for (int &v : test_values) {
					v = random();
				}
				// test
				test_data.insert(test_data.begin() + major_index, build_tree(test_values));
				// reference
				reference_data.insert(reference_data.begin() + major_index, std::move(test_values));
			}
			break;

		case operations::erase:
			{
				logger::get().log_info(CP_HERE) << "op erase";
				std::size_t major_index = random_index(reference_data, random);
				std::size_t minor_index = random_index(reference_data[major_index], random);
				// test
				auto &test_tree = test_data[major_index];
				test_tree.erase(at(test_tree, minor_index));
				if (test_tree.empty()) {
					test_data.erase(test_data.begin() + major_index);
				}
				// reference
				auto &ref_arr = reference_data[major_index];
				ref_arr.erase(ref_arr.begin() + minor_index);
				if (ref_arr.empty()) {
					reference_data.erase(reference_data.begin() + major_index);
				}
			}
			break;
		case operations::erase_subtree:
			{
				logger::get().log_info(CP_HERE) << "op erase_subtree";
				std::size_t major_index = random_index(reference_data, random);
				std::size_t minor_index_beg = random_index(reference_data[major_index], random);
				std::size_t minor_index_end = random_index(reference_data[major_index], random);
				if (minor_index_beg > minor_index_end) {
					std::swap(minor_index_beg, minor_index_end);
				}
				++minor_index_end; // move past the end
				// test
				auto &test_tree = test_data[major_index];
				auto beg_it = at(test_tree, minor_index_beg), end_it = at(test_tree, minor_index_end);
				test_tree.erase(beg_it, end_it);
				if (test_tree.empty()) {
					test_data.erase(test_data.begin() + major_index);
				}
				// reference
				auto &ref_arr = reference_data[major_index];
				ref_arr.erase(ref_arr.begin() + minor_index_beg, ref_arr.begin() + minor_index_end);
				if (ref_arr.empty()) {
					reference_data.erase(reference_data.begin() + major_index);
				}
			}
			break;

		case operations::split:
			{
				logger::get().log_info(CP_HERE) << "op split";
				std::size_t major_index = random_index(reference_data, random);
				std::size_t minor_index = random_index(reference_data[major_index], random);
				// test
				auto &test_tree = test_data[major_index];
				auto iter = at(test_tree, minor_index);
				auto [left_tree, mid, right_tree] = std::move(test_tree).split_at(iter);
				if (right_tree.empty()) {
					test_data.erase(test_data.begin() + major_index);
				} else {
					test_data[major_index] = std::move(right_tree);
				}
				if (!left_tree.empty()) {
					test_data.insert(test_data.begin() + major_index, std::move(left_tree));
				}
				delete mid;
				// reference
				auto &ref_arr = reference_data[major_index];
				std::vector<int> second(ref_arr.begin() + (minor_index + 1), ref_arr.end());
				ref_arr.erase(ref_arr.begin() + minor_index, ref_arr.end());
				if (!second.empty()) {
					reference_data.insert(reference_data.begin() + major_index + 1, std::move(second));
				}
				if (reference_data[major_index].empty()) {
					reference_data.erase(reference_data.begin() + major_index);
				}
			}
			break;
		case operations::join:
			{
				logger::get().log_info(CP_HERE) << "op join";
				if (reference_data.size() < 2) {
					logger::get().log_info(CP_HERE) << "too few trees, cannot join";
					continue;
				}

				auto index = random_between<std::size_t>(0, reference_data.size() - 2, random);
				int merge_value = random();
				// test
				tree new_tree = tree::join_trees(
					std::move(test_data[index]), std::move(test_data[index + 1]), merge_value
				);
				test_data[index] = std::move(new_tree);
				test_data.erase(test_data.begin() + (index + 1));
				// reference
				reference_data[index].emplace_back(merge_value);
				reference_data[index].insert(
					reference_data[index].end(), reference_data[index + 1].begin(), reference_data[index + 1].end()
				);
				reference_data.erase(reference_data.begin() + (index + 1));
			}
			break;

		default:
			assert_true_logical(false, "invalid operation");
		}

		// check that all data are consistent
		for (tree &t : test_data) {
			t.check_integrity();
		}
		assert_true_logical(test_data.size() == reference_data.size(), "number of trees different from ref");
		for (std::size_t i = 0; i < test_data.size(); ++i) {
			const tree &t = test_data[i];
			const auto &ref = reference_data[i];

			assert_true_logical(t.root() != nullptr, "empty array in list");
			assert_true_logical(t.root()->synth_data.num_nodes == ref.size(), "different number of elements");

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
					assert_true_logical(size == n->synth_data.num_nodes, "incorrect synth data");
				}
			}

			{ // check actual data
				auto it = t.begin();
				for (int val : ref) {
					assert_true_logical(it->value == val, "incorrect value");
					++it;
				}
			}
		}

		++op_index;
	}

	logger::get().log_info(CP_HERE) << "exiting normally";
	return 0;
}
