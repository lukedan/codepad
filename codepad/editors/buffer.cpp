#include "buffer.h"

/// \file
/// Implementation of certain functions related to the \ref buffer class.

#include "buffer_manager.h"

namespace codepad::editor {
	buffer::~buffer() {
		buffer_manager::get()._on_deleting_buffer(*this);
	}
}
