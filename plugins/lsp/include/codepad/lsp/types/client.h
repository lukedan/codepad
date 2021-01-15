// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Client-related messages of LSP.

#include "common.h"

namespace codepad::lsp::types {
	struct Registration : public virtual object {
		string id;
		string method;
		optional<any> registerOptions;

		void visit_fields(visitor_base&) override;
	};

	struct RegistrationParams : public virtual object {
		array<Registration> registrations;

		void visit_fields(visitor_base&) override;
	};

	struct Unregistration : public virtual object {
		string id;
		string method;

		void visit_fields(visitor_base&) override;
	};

	struct UnregistrationParams : public virtual object {
		array<Unregistration> unregisterations;

		void visit_fields(visitor_base&) override;
	};
}
