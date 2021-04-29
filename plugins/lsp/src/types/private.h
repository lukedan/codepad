// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Private macros for LSP structures.

#define CP_LSP_VISIT_FUNC(VISITOR, TYPE) void TYPE::visit_fields(visitor_base &VISITOR)
#define CP_LSP_VISIT_BASE(VISITOR, BASE) BASE::visit_fields(VISITOR)
#define CP_LSP_VISIT_FIELD(VISITOR, FIELD) v.visit_field(u8 ## #FIELD, FIELD)
