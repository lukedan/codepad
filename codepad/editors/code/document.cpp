#include "document.h"

/// \file
/// Implementation of certain methods related to \ref codepad::editor::code::document.

#include "document_manager.h"
#include "view.h"
#include "editor.h"
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
		// carets without selections
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
		if (p1mmv.second <= p2mmv.first || p1mmv.first >= p2mmv.second) { // no need to merge
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


	document::~document() {
		document_manager::get()._on_deleting_document(*this);
		_tags.clear();
	}

	void document::save_new(const filesystem::path &path) {
		assert_true_usage(holds_alternative<size_t>(_fileid), "file already associated with a path");
		save();
		document_manager::get()._on_saved_new_document(get<size_t>(_fileid), path);
		_fileid.emplace<filesystem::path>(path);
	}

	void document::undo(editor *source) {
		assert_true_usage(can_undo(), "cannot undo");
		const edit &ce = _edithist[--_curedit];
		document_modifier mod(*this);
		for (const modification &cm : ce) {
			mod.undo_modification(cm);
		}
		mod.finish_edit_nohistory(source);
	}

	void document::redo(editor *source) {
		assert_true_usage(can_redo(), "cannot redo");
		const edit &ce = _edithist[_curedit];
		++_curedit;
		document_modifier mod(*this);
		for (const modification &cm : ce) {
			mod.redo_modification(cm);
		}
		mod.finish_edit_nohistory(source);
	}

	view_formatting document::create_view_formatting() {
		return view_formatting(_lbr);
	}


	void document_modifier::finish_edit_nohistory(editor *source) {
		assert_true_logical(_cfixup.mods.size() == _removedclips.size(), "forgot to add item to _removedclips");
		source->set_carets(std::move(_newcarets));
		_doc->modified.invoke_noret(source, std::move(_cfixup), std::move(_removedclips));
		_doc = nullptr;
	}
}
