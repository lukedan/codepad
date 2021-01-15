// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Window-related messages of LSP.

#include "common.h"

namespace codepad::lsp::types {
	enum class MessageTypeEnum : unsigned char {
		Error = 1,
		Warning = 2,
		Info = 3,
		Log = 4
	};
	using MessageType = numerical_enum<MessageTypeEnum>;

	struct ShowMessageParams : public virtual object {
		MessageType type;
		string message;

		void visit_fields(visitor_base&) override;
	};

	/// Used by ShowMessageRequestClientCapabilities.
	struct MessageActionItemClientCapabilities : public virtual object {
		optional<boolean> additionalPropertiesSupport;

		void visit_fields(visitor_base&) override;
	};
	struct ShowMessageRequestClientCapabilities : public virtual object {
		optional<MessageActionItemClientCapabilities> messageActionItem;

		void visit_fields(visitor_base&) override;
	};

	struct MessageActionItem : public virtual object {
		string title;

		void visit_fields(visitor_base&) override;
	};

	struct ShowMessageRequestParams : public virtual object {
		MessageType type;
		string message;
		optional<array<MessageActionItem>> actions;

		void visit_fields(visitor_base&) override;
	};

	struct ShowDocumentClientCapabilities : public virtual object {
		boolean support = false;

		void visit_fields(visitor_base&) override;
	};

	struct ShowDocumentParams : public virtual object {
		URI uri;
		optional<boolean> external;
		optional<boolean> takeFocus;
		optional<Range> selection;

		void visit_fields(visitor_base&) override;
	};

	struct ShowDocumentResult : public virtual object {
		boolean success = false;

		void visit_fields(visitor_base&) override;
	};

	struct LogMessageParams : public virtual object {
		MessageType type;
		string message;

		void visit_fields(visitor_base&) override;
	};

	struct WorkDoneProgressCreateParams : public virtual object {
		ProgressToken token;

		void visit_fields(visitor_base&) override;
	};

	struct WorkDoneProgressCancelParams : public virtual object {
		ProgressToken token;

		void visit_fields(visitor_base&) override;
	};
}
