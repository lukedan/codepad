// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Declaration of the LSP client.

#include <thread>

#include "codepad/ui/scheduler.h"

#include "codepad/editors/code/interpretation.h"

#include "backend.h"
#include "types/serialization.h"
#include "types/common.h"
#include "types/general.h"
#include "types/workspace.h"
#include "uri.h"

namespace codepad::lsp {
	/// A LSP client.
	class client {
	public:
		using id_t = std::variant<types::integer, std::u8string_view>; ///< Identifiers for requests.
		/// Handler for a request.
		struct request_handler {
			/// The callback that will be called on the main thread upon this request. The \p rapidjson::Value that
			/// is passed in will contain the raw complete JSON response, and it's up to the handler to determine
			/// whether this is a request or a notifcation and the request id, and deserialize the parameters. It's
			/// also the handler's responsibility to send any response messages.
			std::function<void(const rapidjson::Value&, client&)> callback;

			/// Creates a new request handler. The callback accepts two or three parameters: the \ref client, a
			/// \ref id_t that is the id of the request, and, if \p Param is not \p std::nullopt_t, the deserialized
			/// parameters.
			template <
				typename Param, typename Callback
			> [[nodiscard]] static request_handler create_request_handler(Callback&&);
			/// Creates a new notification handler. The callback accepts one or two parameters, similar to
			/// \ref create_request_handler(). The only difference is that the \p id parameter is absent.
			template <
				typename Param, typename Callback
			> [[nodiscard]] static request_handler create_notification_handler(Callback&&);
		};
		/// The state of this client.
		enum class state : unsigned char {
			not_initialized, ///< Created but not initialized.
			initializing, ///< The server is initializing.
			ready, ///< Ready to communicate with the server.
			shutting_down, ///< The server is being shut down.
			receiver_shutdown, ///< The receiver thread has received the reply for \p shutdown request.
			exited ///< The receiver thread has exited and the \p exit notification has been sent.
		};
		/// Function type for error callbacks.
		using on_error_callback = std::function<void(types::integer, std::u8string_view, const rapidjson::Value*)>;

		/// Initializes \ref _backend. Also creates a new thread to receive messages from the server.
		client(std::unique_ptr<backend> back, ui::scheduler &sched) : _backend(std::move(back)) {
			std::thread t(
				[](client &c, ui::scheduler &sched) {
					_receiver_thread(c, sched);
				},
				std::ref(*this), std::ref(sched)
			);
			t.detach();
		}


		/// Sends the \p initialize message and updates \ref _state. Use this instead of sending the message
		/// manually.
		template <typename Callback> void initialize(types::InitializeParams init, Callback &&cb) {
			state expected = state::not_initialized;
			assert_true_usage(
				_state.compare_exchange_strong(expected, state::initializing),
				"initializing a client that has already been initialized"
			);
			_send_request_impl<types::InitializeResult>(
				u8"initialize", init,
				[this, callback = std::forward<Callback>(cb)](types::InitializeResult res) {
					state expected = state::initializing;
					assert_true_logical(
						_state.compare_exchange_strong(expected, state::ready),
						"incorrect state for initialize reply"
					);
					types::InitializedParams initialized;
					_send_notification_impl(u8"initialized", initialized);
					callback();
				},
				default_error_handler
			);
		}
		/// Shuts down the server.
		void shutdown() {
			state expected = state::ready;
			assert_true_usage(
				_state.compare_exchange_strong(expected, state::shutting_down),
				"incorrect state for shutting down"
			);
			_send_shutdown_request();
		}
		/// Sends the \ref exit notification. This function does not check nor alter the state of this client, but
		/// it's advised that this is only called when \ref get_state() returns \ref state::receiver_shutdown.
		/// Normally it is sufficient to use \ref shutdown_and_exit() directly.
		void exit() {
			_send_notification_impl(u8"exit", std::nullopt);
		}
		/// Invokes \ref shutdown(), waits for the receiver process to shut down, and sends the \p exit notification.
		/// Note that this function blocks until a reply for the \p shutdown request is received.
		void shutdown_and_exit() {
			shutdown();
			// wait
			while (_state != state::receiver_shutdown) {
			}
			exit();
			_state = state::exited;
		}

		/// Sends the \p didOpen notification.
		void didOpen(editors::code::interpretation &interp) {
			auto &encoding = *interp.get_encoding();
			auto &buffer = *interp.get_buffer();
			auto &id = buffer.get_id();
			if (!std::holds_alternative<std::filesystem::path>(id)) {
				return;
			}

			// encode document as utf8
			types::string text;
			text.reserve(buffer.length());
			for (editors::buffer::const_iterator it = buffer.begin(); it != buffer.end(); ) {
				codepoint cp;
				if (!encoding.next_codepoint(it, buffer.end(), cp)) {
					cp = encodings::replacement_character;
				}
				auto str = encodings::utf8::encode_codepoint(cp);
				text += std::u8string_view(reinterpret_cast<const char8_t*>(str.data()), str.size());
			}

			types::DidOpenTextDocumentParams didopen;
			didopen.textDocument.version = 0;
			didopen.textDocument.languageId = u8"cpp"; // TODO
			didopen.textDocument.uri = uri::from_current_os_path(std::get<std::filesystem::path>(id));
			didopen.textDocument.text = std::move(text);
			send_notification(u8"textDocument/didOpen", didopen);
		}


		/// Sends a request and registers the given response handler. The handler will be executed on the main
		/// thread.
		template <typename ReturnStruct, typename SendStruct, typename Callback> void send_request(
			std::u8string_view name, SendStruct &send, Callback &&cb, on_error_callback err = default_error_handler
		) {
			assert_true_usage(_state == state::ready, "client is not ready");
			_send_request_impl<ReturnStruct>(name, send, std::forward<Callback>(cb), std::move(err));
		}
		/// Sends a notification.
		template <typename SendStruct> void send_notification(std::u8string_view name, SendStruct &send) {
			assert_true_usage(_state == state::ready, "client is not ready");
			_send_notification_impl(name, send);
		}


		/// Returns the state of this \ref client. Check that this is \ref state::ready before sending is reliable
		/// because transitioning out of \ref state::ready is always manual.
		state get_state() const {
			return _state;
		}

		/// Returns a reference to \ref _request_handlers.
		[[nodiscard]] std::unordered_map<std::u8string_view, request_handler> &request_handlers() {
			return _request_handlers;
		}
		/// \overload
		[[nodiscard]] const std::unordered_map<std::u8string_view, request_handler> &request_handlers() const {
			return _request_handlers;
		}


		/// The default error handler that simply prints the error code and message. Simly logs the error.
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
			void handle_reply(const rapidjson::Value&);
		};


		std::unique_ptr<backend> _backend; ///< The backend used by this client.
		std::unordered_map<types::integer, _reply_handler> _reply_handlers; ///< Handlers for reply messages.
		std::unordered_map<std::u8string_view, request_handler> _request_handlers; ///< Handlers for requests.
		types::integer _next_message_id = 0; ///< Used to generate message IDs.

		std::atomic<state> _state = state::not_initialized; ///< Used to signal the receiver thread to stop.
		/// Request index of the \p shutdown message, used by the receiver thread to determine when to exit.
		std::atomic<types::integer> _shutdown_message_id = -1;


		/// Implementation of \ref send_request().
		template <typename ReturnStruct, typename SendStruct, typename Callback> void _send_request_impl(
			std::u8string_view name, SendStruct &send, Callback &&cb, on_error_callback err
		) {
			logger::get().log_debug(CP_HERE) << "LSP: sending request " << name;
			types::integer id = _next_message_id;
			++_next_message_id;
			// register handler before sending the message
			[[maybe_unused]] auto [it, inserted] = _reply_handlers.try_emplace(
				id, _create_handler<ReturnStruct>(std::forward<Callback>(cb), std::move(err))
			);
			// if it failed to register, something must've gone very wrong
			assert_true_logical(inserted, "repeating unhandled LSP message id???");
			// send message
			_backend->send_message(name, send, id);
		}
		/// Sets \ref _shutdown_message_id, then sends the \p shutdown request without registering a handler.
		void _send_shutdown_request() {
			logger::get().log_debug(CP_HERE) << "LSP: sending shutdown request";
			types::integer id = _next_message_id;
			_shutdown_message_id = id;
			++_next_message_id;
			// send message
			_backend->send_message(u8"shutdown", std::nullopt, id);
		}
		/// Implementation of \ref send_notification().
		template <typename SendStruct> void _send_notification_impl(std::u8string_view name, SendStruct &send) {
			logger::get().log_debug(CP_HERE) << "LSP: sending notification " << name;
			_backend->send_message(name, send, std::nullopt);
		}

		/// Create a handler that converts the result JSON object to an object of the specified type, with the
		/// specified return value handler and error handler.
		template <typename ResponseType, typename Handler> [[nodiscard]] _reply_handler _create_handler(
			Handler &&handler, on_error_callback error_handler
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


		/// The function called by the receiver thread.
		static void _receiver_thread(client&, ui::scheduler&);
	};


	template <
		typename Param, typename Callback
	> inline static client::request_handler client::request_handler::create_request_handler(Callback &&cb) {
		request_handler handler;
		handler.callback = [callback = std::forward<Callback>(cb)](const rapidjson::Value &v, client &c) {
			auto id_it = v.FindMember("id");
			if (id_it == v.MemberEnd()) {
				logger::get().log_error(CP_HERE) << "invalid LSP request: id field missing";
				return;
			}
			id_t id;
			if (id_it->value.IsInt()) {
				id.emplace<types::integer>(id_it->value.GetInt());
			} else if (id_it->value.IsString()) {
				id.emplace<std::u8string_view>(json::rapidjson::value_t::get_string_view(id_it->value));
			} else {
				logger::get().log_error(CP_HERE) << "invalid LSP request: invalid id type";
				return;
			}
			if constexpr (std::is_same_v<Param, std::nullopt_t>) {
				callback(id, c);
			} else {
				Param args;
				types::deserializer des(v);
				static_cast<types::visitor_base*>(&des)->visit_field(u8"params", args);
				callback(id, c, std::move(args));
			}
		};
		return handler;
	}

	template <
		typename Param, typename Callback
	> inline static client::request_handler client::request_handler::create_notification_handler(Callback &&cb) {
		request_handler handler;
		handler.callback = [callback = std::forward<Callback>(cb)](const rapidjson::Value &v, client &c) {
			if constexpr (std::is_same_v<Param, std::nullopt_t>) {
				callback(c);
			} else {
				Param args;
				types::deserializer des(v);
				static_cast<types::visitor_base*>(&des)->visit_field(u8"params", args);
				callback(c, std::move(args));
			}
		};
		return handler;
	}
}
