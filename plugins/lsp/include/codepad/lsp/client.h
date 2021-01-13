// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Declaration of the LSP client.

#include <thread>

#include "backend.h"
#include "types/serialization.h"
#include "types/common.h"

namespace codepad::lsp {
	/// A LSP client.
	class client {
	public:
		/// Initializes \ref _backend. Also creates a new thread to receive messages from the server.
		explicit client(std::unique_ptr<backend> back) : _backend(std::move(back)) {
			std::thread t(
				[this]() {
					while (_continue) {
						std::u8string msg = _backend->receive_message();
						logger::get().log_debug(CP_HERE) << "received message: " << msg;
						rapidjson::Document doc;
						doc.Parse<
							rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag
						>(reinterpret_cast<const char*>(msg.data()), msg.size());
						if (doc.IsObject()) {
							if (auto it = doc.FindMember("result"); it != doc.MemberEnd()) {
								types::deserializer des(it->value);
								types::InitializeResult result;
								des.visit(result);
								if (result.serverInfo.value.has_value()) {
									auto entry = logger::get().log_info(CP_HERE);
									entry << "LSP server: " << result.serverInfo.value->name;
									if (result.serverInfo.value->version.value.has_value()) {
										entry << "\nversion: " << result.serverInfo.value->version.value.value();
									}
								}
								types::logger_serializer log(logger::get().log_debug(CP_HERE));
								log.visit(result);
							} else {
								if (auto err_it = doc.FindMember("error"); err_it != doc.MemberEnd()) {
									rapidjson::StringBuffer sb;
									rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
									err_it->value.Accept(writer);
									logger::get().log_debug(CP_HERE) << "LSP server error: " << sb.GetString();
								}
							}
						}
					}
				}
			);
			t.detach();

			types::InitializeParams init;
			init.processId.value.emplace<types::integer>(GetCurrentProcessId()); // TODO winapi
			types::logger_serializer log(logger::get().log_debug(CP_HERE));
			log.visit(init);
			_backend->send_message(u8"initialize", init, 0);
		}
	protected:
		std::unique_ptr<backend> _backend; ///< The backend used by this client.
		std::atomic_bool _continue = true; ///< Used to signal the receiver thread to stop.
	};
}
