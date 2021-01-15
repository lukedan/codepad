// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Declaration of the LSP client.

#include <thread>

#include "codepad/ui/scheduler.h"

#include "backend.h"
#include "types/serialization.h"
#include "types/common.h"
#include "types/general.h"
#include "types/workspace.h"

namespace codepad::lsp {
	/// A LSP client.
	class client {
	public:
		/// The state of this client.
		enum class state : unsigned char {
			not_initialized, ///< Created but not initialized.
			ready, ///< Ready to communicate with the server.
			shutting_down, ///< The server is being shut down.
			exiting, ///< The server has shut down, and the receiver thread is shutting down.
			exited ///< Used by the receiver thread to indicate that it has shut down.
		};

		/// Initializes \ref _backend. Also creates a new thread to receive messages from the server.
		client(std::unique_ptr<backend> back, ui::scheduler &sched) : _backend(std::move(back)) {
			std::thread t(
				[&sched](client &c) {
					while (c._state != state::exiting) {
						// receive & parse message
						std::u8string msg = c._backend->receive_message();
						logger::get().log_debug(CP_HERE) << "received message:\n" << msg;
						rapidjson::Document doc;
						doc.Parse<
							rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag
						>(reinterpret_cast<const char*>(msg.data()), msg.size());
						// check for errors first
						if (!doc.IsObject()) {
							logger::get().log_error(CP_HERE) << "invalid LSP response: not an object";
							continue;
						}
						auto id_it = doc.FindMember("id");
						if (id_it == doc.MemberEnd()) {
							logger::get().log_error(CP_HERE) << "invalid LSP response: no valid id specified";
							continue;
						}
						if (!id_it->value.IsInt()) {
							if (id_it->value.IsNull()) {
								// TODO what does this mean? when would id be null?
								logger::get().log_info(CP_HERE) << "LSP response with null id";
							} else if (id_it->value.IsString()) {
								logger::get().log_error(CP_HERE) <<
									"Codepad LSP client does not use string IDs by default. " <<
									"who could've sent this message?";
							} else {
								logger::get().log_error(CP_HERE) << "invalid LSP response: invalid id type";
							}
							continue;
						}
						// finally, handle message on the main thread
						// TODO using a rapidjson::Document (which is not copy constructible) causes std::function to
						//      fail to instantiate
						auto ptr = std::make_shared<rapidjson::Document>(std::move(doc));
						sched.execute_callback(
							[&c, id = id_it->value.GetInt(), ptr]() {
								auto handler_it = c._reply_handlers.find(id);
								if (handler_it == c._reply_handlers.end()) {
									logger::get().log_error(CP_HERE) <<
										"no handler registered for the given LSP response";
									return;
								}
								_reply_handler handler = std::move(handler_it->second);
								c._reply_handlers.erase(handler_it);
								handler.handle_reply(*ptr);
							}
						);
					}
					c._state = state::exited;
				},
				std::ref(*this)
			);
			t.detach();
		}


		/// Sends a request and registers the given response handler.
		template <typename ReturnStruct, typename SendStruct, typename Callback> void send_request(
			std::u8string_view name, SendStruct &send, Callback &&cb,
			std::function<
				void(types::integer, std::u8string_view, const rapidjson::Value*)
			> err = default_error_handler
		) {
			types::integer id = ++_timestamp;
			// send message
			_backend->send_message(name, send, id);
			// register handler
			[[maybe_unused]] auto [it, inserted] = _reply_handlers.try_emplace(
				id, _create_handler<ReturnStruct>(std::forward<Callback>(cb), std::move(err))
			);
			// if it failed to register, something must've gone very wrong
			assert_true_logical(inserted, "repeating unhandled LSP message id???");
		}
		/// Sends a notification.
		template <typename SendStruct> void send_notification(std::u8string_view name, SendStruct &send) {
			_backend->send_message(name, send, std::nullopt);
		}


		/// The default error handler that simply prints the error code and message.
		inline static void default_error_handler(
			types::integer code, std::u8string_view msg, const rapidjson::Value*
		) {
			logger::get().log_error(CP_HERE) << "LSP server returned error " << code << ": " << msg;
		}
	protected:
		/// Handles a reply message.
		struct _reply_handler {
			/// Function invoked when the response does not indicate an error.
			std::function<void(const rapidjson::Value&)> on_return;
			/// Function invoked when the response indicates an error.
			std::function<void(types::integer, std::u8string_view, const rapidjson::Value*)> on_error;

			/// Handles the reply. Assumes that the given \p rapidjson::Document is an object.
			void handle_reply(const rapidjson::Value &reply) {
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
		};

		std::unique_ptr<backend> _backend; ///< The backend used by this client.
		std::atomic<state> _state = state::not_initialized; ///< Used to signal the receiver thread to stop.
		std::unordered_map<types::integer, _reply_handler> _reply_handlers; ///< Handlers for reply messages.
		types::integer _timestamp = 0; ///< Used to generate message IDs.


		/// Create a handler that converts the result JSON object to an object of the specified type, with the
		/// specified return value handler and error handler.
		template <typename ResponseType, typename Handler> [[nodiscard]] _reply_handler _create_handler(
			Handler &&handler,
			std::function<void(types::integer, std::u8string_view, const rapidjson::Value*)> error_handler
		) {
			_reply_handler result;
			result.on_return = [h = std::forward<Handler>(handler)](const rapidjson::Value &val) {
				types::deserializer des(val);
				ResponseType response;
				des.visit(response);
				h(std::move(response));
			};
			result.on_error = std::move(error_handler);
			return result;
		}
	};
}
