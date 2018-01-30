#include "context.h"
#include "../../os/current/filesystem.h"

using namespace std;
using namespace codepad::os;

namespace codepad {
	namespace editor {
		namespace code {
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


			bool text_context::load_from_file(const std::filesystem::path &fn) {
				clear();
				auto begt = chrono::high_resolution_clock::now();
				file fil(fn, access_rights::read, open_mode::open_or_create);
				if (!fil.valid()) {
					return false;
				}
				file_mapping mapping(fil, access_rights::read);
				if (!mapping.valid()) {
					return false;
				}
				unsigned char *cs = static_cast<unsigned char*>(mapping.get_mapped_pointer());
				insert_text(0, cs, cs + static_cast<int>(fil.get_size()));
				auto now = chrono::high_resolution_clock::now();
				logger::get().log_info(
					CP_HERE, "file loaded in ",
					chrono::duration<double, milli>(now - begt).count(),
					"ms"
				);
				return true;
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
	}
}
