// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/lsp/client.h"

/// \file
/// Implementation of the LSP client.

namespace codepad::lsp {
	void client::_reply_handler::handle_reply(const rapidjson::Value &reply) {
		if (auto it = reply.FindMember("result"); it != reply.MemberEnd()) {
			if (on_return) {
				on_return(it->value);
			}
			return;
		}
		// otherwise handle error
		auto err_it = reply.FindMember("error");
		if (err_it == reply.MemberEnd()) {
			logger::get().log_error(CP_HERE) << "LSP response has neither result nor error";
		}
		if (on_error) {
			json::rapidjson::value_t val(&err_it->value);
			if (auto obj = val.try_cast<json::rapidjson::object_t>()) {
				const rapidjson::Value *data = nullptr;
				if (auto it = err_it->value.FindMember("data"); it != err_it->value.MemberEnd()) {
					data = &it->value;
				}
				// invoke handler
				on_error(
					obj->parse_member<types::integer>(u8"code").value_or(0),
					obj->parse_member<std::u8string_view>(u8"message").value_or(u8""),
					data
				);
			} else {
				logger::get().log_error(CP_HERE) << "LSP response with invalid error";
			}
		}
	}

	void client::_receiver_thread(client &c, ui::scheduler &sched) {
		while (true) {
			// receive & parse message
			std::u8string msg = c._backend->receive_message();
			rapidjson::Document doc;
			doc.Parse<
				rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag
			>(reinterpret_cast<const char*>(msg.data()), msg.size());

			// check for errors first
			if (!doc.IsObject()) {
				logger::get().log_error(CP_HERE) << "invalid LSP response: not an object";
				continue;
			}
			// check for the jsonrpc field
			// this is optional and non-fatal
			if (auto jsonrpc_it = doc.FindMember("jsonrpc"); jsonrpc_it != doc.MemberEnd()) {
				if (!jsonrpc_it->value.IsString()) {
					logger::get().log_error(CP_HERE) << "LSP response without invalid type for jsonrpc version";
				} else {
					auto version = json::rapidjson::value_t::get_string_view(jsonrpc_it->value);
					if (version != u8"2.0") {
						logger::get().log_error(CP_HERE) <<
							"LSP response without invalid version: expected 2.0, got " << version;
					}
				}
			} else {
				logger::get().log_error(CP_HERE) << "LSP response without jsonrpc version";
			}

			// handle requests and notifications
			if (auto method_it = doc.FindMember("method"); method_it != doc.MemberEnd()) {
				if (!method_it->value.IsString()) {
					logger::get().log_error(CP_HERE) << "invalid LSP response: method field is not a string";
					continue;
				}
				// TODO using a rapidjson::Document (which is not copy constructible) causes std::function to
				//      fail to instantiate
				auto ptr = std::make_shared<rapidjson::Document>(std::move(doc));
				sched.execute_callback(
					[&c, doc_ptr = std::move(ptr)]() {
						auto method = json::rapidjson::value_t::get_string_view(
							doc_ptr->FindMember("method")->value
						);
						auto handler_it = c.request_handlers().find(method);
						if (handler_it == c.request_handlers().end()) {
							logger::get().log_warning(CP_HERE) << "unhandled LSP request/notification: " << method;
							return;
						}
						handler_it->second.callback(*doc_ptr, c);
					}
				);
				continue;
			}

			// handle response
			if (auto id_it = doc.FindMember("id"); id_it != doc.MemberEnd()) {
				// if it's a response, the id must be an int
				if (!id_it->value.IsInt()) {
					if (id_it->value.IsString()) {
						logger::get().log_error(CP_HERE) <<
							"the codepad LSP client does not use string IDs by default. " <<
							"who could've sent this message?";
					} else {
						logger::get().log_error(CP_HERE) << "invalid LSP response: invalid id type";
					}
					continue;
				}
				// if this is the response to shutdown, do not handle the reply on the main thread; instead,
				// log any potential errors here, update _state, and exit
				// the exit message will be sent by the main thread once it detects the change to _state
				if (id_it->value.GetInt() == c._shutdown_message_id) {
					_reply_handler handler;
					// no need for success handler
					handler.on_error = default_error_handler;
					handler.handle_reply(doc);
					break;
				}
				// finally, handle message on the main thread
				// TODO using a rapidjson::Document (which is not copy constructible) causes std::function to
				//      fail to instantiate
				auto ptr = std::make_shared<rapidjson::Document>(std::move(doc));
				sched.execute_callback(
					[&c, id = id_it->value.GetInt(), doc_ptr = std::move(ptr)]() {
						auto handler_it = c._reply_handlers.find(id);
						if (handler_it == c._reply_handlers.end()) {
							logger::get().log_error(CP_HERE) <<
								"no handler registered for the given LSP response";
							return;
						}
						_reply_handler handler = std::move(handler_it->second);
						c._reply_handlers.erase(handler_it);
						handler.handle_reply(*doc_ptr);
					}
				);
				continue;
			}

			logger::get().log_error(CP_HERE) << "invalid LSP message received: no valid id or method specified";
			continue;
		}
	}
}
