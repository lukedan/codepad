#pragma once

#include "../utilities/encodings.h"

namespace codepad {
	namespace os {
		namespace nt_path {
			constexpr char_t sys_default_separator = '\\';
		}
		namespace posix_path {
			constexpr char_t sys_default_separator = '/';
		}

		namespace path {
#ifdef CP_PLATFORM_WINDOWS
			using namespace nt_path;
#else
			using namespace posix_path;
#endif
			inline bool is_separator(char_t c) {
				return c == '/' || c == '\\';
			}
			inline str_t join(const str_t &beg, const str_t &end, char_t sep = sys_default_separator) {
				if (beg.length() == 0) {
					return end;
				}
				str_t res;
				res.reserve(beg.length() + end.length());
				res.append(beg);
				while (res.length() > 0 && is_separator(res.back())) {
					res.pop_back();
				}
				res.push_back(sep);
				auto it = end.begin();
				for (; it != end.end() && is_separator(*it); ++it) {
				}
				res.append(it, end.end());
				return res;
			}
		}

		enum class access_rights {
			read = 1,
			write = 2,
			read_write = read | write
		};
		enum class open_mode {
			open = 1,
			create = 2,
			open_and_truncate = 4,
			open_or_create = open | create,
			create_or_truncate = create | open_and_truncate
		};
	}
}
