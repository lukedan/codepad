// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/lsp/uri.h"

/// \file
/// Implementation of URI utilities.

#include <string>

#include <uriparser/Uri.h>

#include <codepad/core/logging.h>
#include <codepad/core/assert.h>

namespace codepad::lsp::uri {
	/// Converts a path to a URI using the given conversion function.
	void _uri_from_path(const std::u8string &p, std::u8string &res, int (*convert)(const char*, char*)) {
		auto *res_data = reinterpret_cast<char*>(res.data());
		int error = uriWindowsFilenameToUriStringA(reinterpret_cast<const char*>(p.c_str()), res_data);
		if (error != 0) {
			logger::get().log_error(CP_HERE) << "failed to convert path: " << p << ", error: " << error;
			assert_true_sys(false, "failed to convert path to uri");
		}
		res.resize(std::strlen(res_data));
	}

	std::u8string from_windows_path(const std::u8string &p) {
		std::u8string uri;
		uri.resize(9 + 3 * p.size());
		_uri_from_path(p, uri, uriWindowsFilenameToUriStringA);
		return uri;
	}

	std::u8string from_unix_path(const std::u8string &p) {
		std::u8string uri;
		uri.resize(8 + 3 * p.size());
		_uri_from_path(p, uri, uriUnixFilenameToUriStringA);
		return uri;
	}

	std::u8string from_current_os_path(const std::u8string &p) {
#ifdef CP_PLATFORM_WINDOWS
		return from_windows_path(p);
#else
		return from_unix_path(p);
#endif
	}
}
