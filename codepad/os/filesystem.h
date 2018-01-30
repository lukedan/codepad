#pragma once

#include "../utilities/encodings.h"

namespace codepad {
	namespace os {
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
