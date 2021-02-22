// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Uses the RapidJSON engine by default.

#include "rapidjson.h"

namespace codepad::json {
	using document_t = json::rapidjson::document_t; ///< Documents that contain raw JSON data.
	using value_t = json::rapidjson::value_t; ///< Pointers to generic JSON values in a \ref document_t.
	using array_t = json::rapidjson::array_t; ///< Pointers to JSON arrays in a \ref document_t.
	using object_t = json::rapidjson::object_t; ///< Pointers to JSON objects in a \ref document_t.
}
