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
	/// Converts between a path and a URI using the given conversion function.
	void _convert(const std::u8string &p, std::u8string &res, int (*convert)(const char*, char*)) {
		auto *res_data = reinterpret_cast<char*>(res.data());
		int error = convert(reinterpret_cast<const char*>(p.c_str()), res_data);
		if (error != 0) {
			logger::get().log_error(CP_HERE) << "failed to convert: " << p << ", error: " << error;
			assert_true_sys(false, "failed to convert between path and uri");
		}
		res.resize(std::strlen(res_data));
	}


	std::u8string from_windows_path(const std::u8string &p) {
		std::u8string uri;
		uri.resize(9 + 3 * p.size());
		_convert(p, uri, uriWindowsFilenameToUriStringA);
		return uri;
	}

	std::u8string from_unix_path(const std::u8string &p) {
		std::u8string uri;
		uri.resize(8 + 3 * p.size());
		_convert(p, uri, uriUnixFilenameToUriStringA);
		return uri;
	}

	std::u8string from_current_os_path(const std::u8string &p) {
#ifdef CP_PLATFORM_WINDOWS
		return from_windows_path(p);
#else
		return from_unix_path(p);
#endif
	}


	std::u8string to_windows_path(const std::u8string &u) {
		std::u8string path;
		path.resize(u.size() + 1);
		_convert(u, path, uriUriStringToWindowsFilenameA);
		return path;
	}

	std::u8string to_unix_path(const std::u8string &u) {
		std::u8string path;
		path.resize(u.size() + 1);
		_convert(u, path, uriUriStringToUnixFilenameA);
		return path;
	}

	std::u8string to_current_os_path(const std::u8string &u) {
#ifdef CP_PLATFORM_WINDOWS
		return to_windows_path(u);
#else
		return to_unix_path(u);
#endif
	}
}
