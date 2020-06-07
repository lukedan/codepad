// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Common definitions for plugins.

// define PLUGIN_EXPORT
// https://atomheartother.github.io/c++/2018/07/12/CPPDynLib.html
#if defined(_WIN32) || defined(__CYGWIN__)
#	ifdef __GNUC__
#		define PLUGIN_EXPORT __attribute__((dllexport))
#	else
#		define PLUGIN_EXPORT __declspec(dllexport)
#	endif
#else
#	define PLUGIN_EXPORT
#endif

#define PLUGIN_INITIALIZE(CONTEXT) PLUGIN_EXPORT void initialize(const ::codepad::plugin_context &CONTEXT)
#define PLUGIN_GET_NAME PLUGIN_EXPORT const char *get_name
#define PLUGIN_ENABLE PLUGIN_EXPORT void enable
#define PLUGIN_DISABLE PLUGIN_EXPORT void disable
#define PLUGIN_FINALIZE PLUGIN_EXPORT void finalize