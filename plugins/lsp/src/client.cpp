// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/lsp/client.h"

/// \file
/// Implementation of the LSP client.

namespace codepad::lsp {
	void client::reply_handler::handle_reply(const json::object_t &reply) {
		if (auto it = reply.find_member(u8"result"); it != reply.member_end()) {
			if (on_return) {
				on_return(it.value());
			}
			return;
		}
		// otherwise handle error
		auto err_it = reply.find_member(u8"error");
		if (err_it == reply.member_end()) {
			logger::get().log_error(CP_HERE) << "LSP response has neither result nor error; skipping error handler";
		}
		if (on_error) {
			if (auto err_obj = err_it.value().try_cast<json::object_t>()) {
				json::value_t data;
				if (auto it = err_obj->find_member(u8"data"); it != err_obj->member_end()) {
					data = it.value();
				}
				// invoke handler
				on_error(
					err_obj->parse_member<types::integer>(u8"code").value_or(0),
					err_obj->parse_member<std::u8string_view>(u8"message").value_or(u8""),
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
			auto doc = json::document_t::parse(msg);

			// check for errors first
			auto root_obj = doc.root().try_cast<json::object_t>();
			if (!root_obj.has_value()) {
				logger::get().log_error(CP_HERE) << "invalid LSP response: not an object";
				continue;
			}
			// check for the jsonrpc field
			// this is optional and non-fatal
			if (auto jsonrpc_it = root_obj->find_member(u8"jsonrpc"); jsonrpc_it != root_obj->member_end()) {
				if (auto jsonrpc_str = jsonrpc_it.value().try_cast<std::u8string_view>()) {
					if (jsonrpc_str.value() != u8"2.0") {
						logger::get().log_error(CP_HERE) <<
							"LSP response without invalid version: expected 2.0, got " << jsonrpc_str.value();
					}
				} else {
					logger::get().log_error(CP_HERE) << "LSP response without invalid type for jsonrpc version";
				}
			} else {
				logger::get().log_error(CP_HERE) << "LSP response without jsonrpc version";
			}

			// handle requests and notifications
			if (auto method_it = root_obj->find_member(u8"method"); method_it != root_obj->member_end()) {
				if (auto method_str = method_it.value().try_cast<std::u8string_view>()) {
					// TODO using a json::document_t (which is not copy constructible) causes std::function to fail
					//      to instantiate
					auto ptr = std::make_shared<json::document_t>(std::move(doc));
					sched.execute_callback(
						[&c, doc_ptr = std::move(ptr), method = method_str.value()]() {
							auto handler_it = c.request_handlers().find(method);
							if (handler_it == c.request_handlers().end()) {
								logger::get().log_warning(CP_HERE) <<
									"unhandled LSP request/notification: " << method;
								return;
							}
							handler_it->second.callback(doc_ptr->root().get<json::object_t>(), c);
						}
					);
				} else {
					logger::get().log_error(CP_HERE) << "invalid LSP response: method field is not a string";
				}
				continue;
			}

			// handle response
			if (auto id_it = root_obj->find_member(u8"id"); id_it != root_obj->member_end()) {
				// if it's a response, the id must be an int
				if (auto id = id_it.value().try_cast<int>()) {
					// if this is the response to shutdown, do not handle the reply on the main thread; instead,
					// log any potential errors here and exit
					// the exit message will be sent by the main thread once this thread exits
					if (id.value() == c._shutdown_message_id) {
						reply_handler handler;
						// no need for success handler
						handler.on_error = default_error_handler;
						handler.handle_reply(root_obj.value());
						break;
					}
					// finally, handle message on the main thread
					// TODO using a json::document_t (which is not copy constructible) causes std::function to fail
					//      to instantiate
					auto ptr = std::make_shared<json::document_t>(std::move(doc));
					sched.execute_callback(
						[&c, id = id.value(), doc_ptr = std::move(ptr)]() {
							auto handler_it = c._reply_handlers.find(id);
							if (handler_it == c._reply_handlers.end()) {
								logger::get().log_error(CP_HERE) <<
									"no handler registered for the given LSP response";
								return;
							}
							reply_handler handler = std::move(handler_it->second);
							c._reply_handlers.erase(handler_it);
							handler.handle_reply(doc_ptr->root().get<json::object_t>());
						}
					);
				} else {
					if (id_it.value().is<std::u8string_view>()) {
						logger::get().log_error(CP_HERE) <<
							"the codepad LSP client does not use string IDs by default. " <<
							"who could've sent this message?";
					} else {
						logger::get().log_error(CP_HERE) << "invalid LSP response: invalid id type";
					}
				}
				continue;
			}

			logger::get().log_error(CP_HERE) << "invalid LSP message received: no valid id or method specified";
			continue;
		}
	}
}
