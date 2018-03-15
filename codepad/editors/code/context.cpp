#include "context.h"
#include "context_manager.h"
#include "../../os/filesystem.h"

using namespace std;
using namespace codepad::os;

namespace codepad::editor::code {
	caret_set::container::iterator caret_set::add_caret(container &mp, entry c, bool &merged) {
		merged = false;
		auto minmaxv = minmax({c.first.first, c.first.second});
		auto beg = mp.lower_bound(caret_selection(minmaxv.first, minmaxv.first));
		if (beg != mp.begin()) {
			--beg;
		}
		while (beg != mp.end() && min(beg->first.first, beg->first.second) <= minmaxv.second) {
			if (try_merge_selection(
				c.first.first, c.first.second,
				beg->first.first, beg->first.second,
				c.first.first, c.first.second
			)) {
				beg = mp.erase(beg);
				merged = true;
			} else {
				++beg;
			}
		}
		return mp.insert(c).first;
	}

	bool caret_set::try_merge_selection(
		size_t mm, size_t ms, size_t sm, size_t ss, size_t &rm, size_t &rs
	) {
		auto p1mmv = minmax(mm, ms), p2mmv = minmax(sm, ss);
		if (mm == ms && mm >= p2mmv.first && mm <= p2mmv.second) {
			rm = sm;
			rs = ss;
			return true;
		}
		if (sm == ss && sm >= p1mmv.first && sm <= p1mmv.second) {
			rm = mm;
			rs = ms;
			return true;
		}
		if (p1mmv.second <= p2mmv.first || p1mmv.first >= p2mmv.second) {
			return false;
		}
		size_t gmin = min(p1mmv.first, p2mmv.first), gmax = max(p1mmv.second, p2mmv.second);
		assert_true_logical(!((mm == gmin && sm == gmax) || (mm == gmax && sm == gmin)), "caret layout shouldn't occur");
		if (mm < ms) {
			rm = gmin;
			rs = gmax;
		} else {
			rm = gmax;
			rs = gmin;
		}
		return true;
	}


	text_context::text_context(const filesystem::path &fn) : _fileid(in_place_type<filesystem::path>, fn) {
		performance_monitor monitor(CP_STRLIT("load file"), 0.1);
		file fil(fn, access_rights::read, open_mode::open);
		if (fil.valid()) {
			file_mapping mapping(fil, access_rights::read);
			if (mapping.valid()) {
				char *cs = static_cast<char*>(mapping.get_mapped_pointer());
				insert_text(0, cs, cs + static_cast<int>(fil.get_size()));
				set_default_line_ending(detect_most_used_line_ending());
				return;
			}
		}
		// TODO error handling
	}

	text_context::~text_context() {
		context_manager::get()._on_deleting_context(_fileid);
	}

	void text_context::save_new(const filesystem::path &path) {
		assert_true_usage(holds_alternative<size_t>(_fileid), "file already associated with a path");
		context_manager::get()._on_saved_new_context(get<size_t>(_fileid), path);
		_fileid.emplace<filesystem::path>(path);
		save();
	}

	caret_set text_context::undo(editor *source) {
		assert_true_usage(can_undo(), "cannot undo");
		const edit &ce = _edithist[--_curedit];
		text_context_modifier mod(*this);
		for (const modification &cm : ce) {
			mod.undo_modification(cm);
		}
		return mod.finish_edit_nohistory(source);
	}

	caret_set text_context::redo(editor *source) {
		assert_true_usage(can_redo(), "cannot redo");
		const edit &ce = _edithist[_curedit];
		++_curedit;
		text_context_modifier mod(*this);
		for (const modification &cm : ce) {
			mod.redo_modification(cm);
		}
		return mod.finish_edit_nohistory(source);
	}
}
