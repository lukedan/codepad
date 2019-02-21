// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "buffer.h"

/// \file
/// Implementation of certain functions related to the \ref buffer class.

#include "buffer_manager.h"

namespace codepad::editors {
	buffer::~buffer() {
		buffer_manager::get()._on_deleting_buffer(*this);
	}
}
