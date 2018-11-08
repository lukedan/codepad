// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include <random>

#include "editors/code/interpretation.h"

using namespace std;

using namespace codepad;
using namespace codepad::editor;
using namespace codepad::editor::code;

template <typename Rnd> byte_string generate_random_utf8_string(size_t length, Rnd &random) {
	uniform_int_distribution<codepoint> dist(0, 0x10FFFF - 0x800);
	basic_string<codepoint> str;
	for (size_t i = 0; i < length; ++i) {
		codepoint cp = dist(random);
		if (cp >= 0xD800) {
			cp += 0x800;
		}
		str.push_back(cp);
	}
	byte_string res;
	for (codepoint cp : str) {
		res.append(encodings::utf8::encode_codepoint(cp));
	}
	return res;
}

template <typename Rnd> byte_string generate_random_string(size_t length, Rnd &random) {
	byte_string res;
	uniform_int_distribution<int> dist(0, 255);
	for (size_t i = 0; i < length; ++i) {
		res.push_back(static_cast<std::byte>(dist(random)));
	}
	return res;
}

template <typename Rnd> vector<pair<size_t, size_t>> get_modify_positions_boundary(
	size_t count, const buffer&, const interpretation &interp, Rnd &random
) {
	uniform_int_distribution<size_t> caretpos_dist(0, interp.get_linebreaks().num_chars());
	// create carets
	std::vector<size_t> carets;
	for (size_t i = 0; i < count; ++i) {
		carets.push_back(caretpos_dist(random));
		carets.push_back(caretpos_dist(random));
	}
	sort(carets.begin(), carets.end());
	caret_set cset;
	for (size_t i = 0; i < carets.size(); i += 2) {
		cset.add(caret_set::entry({carets[i], carets[i + 1]}, caret_data()));
	}

	vector<pair<size_t, size_t>> cs;
	interpretation::character_position_converter cvt(interp);
	for (const auto &pair : cset.carets) {
		assert_true_logical(pair.first.first <= pair.first.second);
		size_t p1 = cvt.character_to_byte(pair.first.first), p2 = cvt.character_to_byte(pair.first.second);
		cs.emplace_back(p1, p2);
	}
	return cs;
}
template <typename Rnd> vector<pair<size_t, size_t>> get_modify_positions_random(
	size_t count, const buffer &buf, const interpretation&, Rnd &random
) {
	uniform_int_distribution<size_t> caretpos_dist(0, buf.length());
	// create carets
	std::vector<size_t> carets;
	for (size_t i = 0; i < count; ++i) {
		carets.push_back(caretpos_dist(random));
		carets.push_back(caretpos_dist(random));
	}
	sort(carets.begin(), carets.end());
	vector<pair<size_t, size_t>> cs;
	for (size_t i = 0; i < carets.size(); i += 2) {
		cs.emplace_back(carets[i], carets[i + 1]);
	}
	return cs;
}
byte_string convert_to_byte_string(const string &str) {
	return byte_string(reinterpret_cast<const std::byte*>(str.c_str()));
}

int main(int argc, char **argv) {
	initialize(argc, argv);
	default_random_engine eng(123456);
	auto buf = make_shared<buffer>(0);
	interpretation interp(buf, encoding_manager::get().get_default());

	caret_set cset;
	cset.reset();
	interp.on_insert(cset, generate_random_string(1000000, eng), nullptr);
	assert_true_logical(interp.check_integrity());

	uniform_int_distribution<size_t>
		ncarets_dist(1, 100), insertlen_dist(0, 3000);
	size_t idx = 0;
	while (true) {
		logger::get().log_info(
			CP_HERE, "document length: ", buf->length(), " bytes, ", interp.get_linebreaks().num_chars(), " chars"
		);
		vector<pair<size_t, size_t>> positions = get_modify_positions_random(ncarets_dist(eng), *buf, interp, eng);
		vector<byte_string> inserts;
		for (size_t i = 0; i < positions.size(); ++i) {
			inserts.emplace_back(generate_random_string(insertlen_dist(eng), eng));
		}
		{
			buffer::modifier mod;
			mod.begin(*buf, nullptr);
			for (size_t i = 0; i < positions.size(); ++i) {
				mod.modify(positions[i].first, positions[i].second - positions[i].first, inserts[i]);
			}
			buffer::edit dummy;
			mod.end_custom(dummy); // no history; otherwise all memory will be eaten
		}
		logger::get().log_info(CP_HERE, "checking edit ", idx);
		assert_true_logical(interp.check_integrity());
		++idx;
	}

	return 0;
}
