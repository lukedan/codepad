// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Declaration of the LSP client.

#include <thread>

#include "codepad/ui/scheduler.h"

#include "codepad/editors/code/interpretation.h"

#include "types/serialization.h"
#include "types/common.h"
#include "types/general.h"
#include "types/workspace.h"
#include "backend.h"
#include "manager.h"
#include "uri.h"

namespace codepad::lsp {
	/// A LSP client.
	class client {
	public:
		using id_t = std::variant<types::integer, std::u8string_view>; ///< Identifiers for requests.
		/// The state of this client.
		enum class state : unsigned char {
			not_initialized, ///< Created but not initialized.
			initializing, ///< The server is initializing.
			ready, ///< Ready to communicate with the server.
			shutting_down, ///< The server is being shut down.
			exited ///< The receiver thread has exited and the \p exit notification has been sent.
		};
		/// Function type for error callbacks.
		using on_error_callback = std::function<void(types::integer, std::u8string_view, const json::value_t&)>;
		/// Handles a reply message.
		struct reply_handler {
			/// Function invoked when the response does not indicate an error.
			std::function<void(const json::value_t&)> on_return;
			/// Function invoked when the response indicates an error.
			on_error_callback on_error;

			/// Handles the reply.
			void handle_reply(const json::object_t&);
		};
		/// Handler for a request.
		struct request_handler {
			/// The callback that will be called on the main thread upon this request. The \ref json::object_t that
			/// is passed in will contain the raw complete JSON response, and it's up to the handler to determine
			/// whether this is a request or a notifcation and the request id, and deserialize the parameters. It's
			/// also the handler's responsibility to send any response messages.
			std::function<void(const json::object_t&, client&)> callback;

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

		/// A token for a request that has been sent out. This can be used to modify the callback functions. Note
		/// that this is invalidated but not automatically cleared when the reply is executed.
		struct request_token {
			friend client;
		public:
			/// Default constructor.
			request_token() = default;

			/// Returns the registered \ref reply_handler.
			[[nodiscard]] reply_handler &handler() const {
				assert_true_usage(!empty(), "accessing an empty token");
				return _iter->second;
			}

			/// Resets \ref reply_handler::on_return to \p nullptr and \ref reply_handler::on_error to
			/// \ref default_error_handler.
			void cancel_handler() {
				assert_true_usage(!empty(), "accessing an empty token");
				handler().on_return = nullptr;
				handler().on_error = default_error_handler;
			}

			/// Returns whether this is an empty token.
			[[nodiscard]] bool empty() const {
				return _client == nullptr;
			}
		protected:
			/// Iterator to the reply handler entry.
			std::unordered_map<types::integer, reply_handler>::iterator _iter;
			client *_client = nullptr; ///< The associated client. This will be \p nullptr for empty tokens.

			/// Initializes all fields of this token.
			request_token(client &c, std::unordered_map<types::integer, reply_handler>::iterator it) :
				_iter(it), _client(&c) {
			}
		};


		/// Initializes \ref _backend. Also creates a new thread to receive messages from the server.
		client(std::unique_ptr<backend> back, manager &man) :
			_backend(std::move(back)), _manager(man) {

			_receiver_thread_obj = std::thread(
				[](client &c, ui::scheduler &sched) {
					_receiver_thread(c, sched);
				},
				std::ref(*this), std::ref(_manager.get_plugin_context().ui_man->get_scheduler())
			);
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
					_initialize_result = std::move(res);
					state expected = state::initializing;
					assert_true_logical(
						_state.compare_exchange_strong(expected, state::ready),
						"incorrect state for initialize reply"
					);
					types::InitializedParams initialized;
					_send_notification_impl(u8"initialized", initialized);
					callback(_initialize_result);
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
			_receiver_thread_obj.join();
			exit();
			_state = state::exited;
		}


		/// Sends a request and registers the given response handler. The handler will be executed on the main
		/// thread.
		template <typename ReturnStruct, typename SendStruct, typename Callback> request_token send_request(
			std::u8string_view name, SendStruct &send, Callback &&cb, on_error_callback err = default_error_handler
		) {
			assert_true_usage(_state == state::ready, "client is not ready");
			return _send_request_impl<ReturnStruct>(name, send, std::forward<Callback>(cb), std::move(err));
		}
		/// Sends a notification.
		template <typename SendStruct> void send_notification(std::u8string_view name, SendStruct &send) {
			assert_true_usage(_state == state::ready, "client is not ready");
			_send_notification_impl(name, send);
		}


		/// Returns \ref _initialize_result.
		const types::InitializeResult &get_initialize_result() const {
			return _initialize_result;
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

		/// Returns \ref _manager.
		[[nodiscard]] manager &get_manager() const {
			return _manager;
		}


		/// The default error handler that simply prints the error code and message. Simly logs the error.
		inline static void default_error_handler(
			types::integer code, std::u8string_view msg, const json::value_t&
		) {
			logger::get().log_error(CP_HERE) << "LSP server returned error " << code << ": " << msg;
		}
	protected:
		types::InitializeResult _initialize_result; ///< Initialization result with server capabilities.

		std::unique_ptr<backend> _backend; ///< The backend used by this client.
		std::unordered_map<types::integer, reply_handler> _reply_handlers; ///< Handlers for reply messages.
		std::unordered_map<std::u8string_view, request_handler> _request_handlers; ///< Handlers for requests.
		types::integer _next_message_id = 0; ///< Used to generate message IDs.

		std::thread _receiver_thread_obj; ///< The receiver thread object.
		std::atomic<state> _state = state::not_initialized; ///< Used to signal the receiver thread to stop.
		/// Request index of the \p shutdown message, used by the receiver thread to determine when to exit.
		std::atomic<types::integer> _shutdown_message_id = -1;

		manager &_manager; ///< The \ref manager that contains settings for the LSP plugin.


		/// Implementation of \ref send_request().
		template <typename ReturnStruct, typename SendStruct, typename Callback> request_token _send_request_impl(
			std::u8string_view name, SendStruct &send, Callback &&cb, on_error_callback err
		) {
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
			return request_token(*this, it);
		}
		/// Sets \ref _shutdown_message_id, then sends the \p shutdown request without registering a handler.
		void _send_shutdown_request() {
			types::integer id = _next_message_id;
			_shutdown_message_id = id;
			++_next_message_id;
			// send message
			_backend->send_message(u8"shutdown", std::nullopt, id);
		}
		/// Implementation of \ref send_notification().
		template <typename SendStruct> void _send_notification_impl(std::u8string_view name, SendStruct &send) {
			_backend->send_message(name, send, std::nullopt);
		}

		/// Create a handler that converts the result JSON object to an object of the specified type, with the
		/// specified return value handler and error handler.
		template <typename ResponseType, typename Handler> [[nodiscard]] reply_handler _create_handler(
			Handler &&handler, on_error_callback error_handler
		) {
			reply_handler result;
			result.on_return = [h = std::forward<Handler>(handler)](const json::value_t &val) {
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


	namespace _details {
		/// Used to determine if a type is a \ref types::optional.
		template <typename T> struct is_optional {
			constexpr static bool value = false; ///< \p false.
		};
		/// True for \ref types::optional.
		template <typename T> struct is_optional<types::optional<T>> {
			constexpr static bool value = true; ///< \p true.
		};
		///< Shorthand for \ref is_optional::value.
		template <typename T> constexpr bool is_optional_v = is_optional<T>::value;
	}


	template <
		typename Param, typename Callback
	> inline client::request_handler client::request_handler::create_request_handler(Callback &&cb) {
		request_handler handler;
		handler.callback = [callback = std::forward<Callback>(cb)](const json::object_t &v, client &c) {
			auto id_it = v.find_member(u8"id");
			if (id_it == v.member_end()) {
				logger::get().log_error(CP_HERE) << "invalid LSP request: id field missing";
				return;
			}
			id_t id;
			if (auto id_int = id_it.value().cast<types::integer>()) {
				id.emplace<types::integer>(id_int.value());
			} else if (auto id_str = id_it.value().cast<std::u8string_view>()) {
				id.emplace<std::u8string_view>(id_str.value());
			} else {
				logger::get().log_error(CP_HERE) << "invalid LSP request: invalid id type";
				return;
			}
			if constexpr (std::is_same_v<Param, std::nullopt_t>) {
				callback(id, c);
			} else {
				Param args;
				if (auto iter = v.find_member(u8"params"); iter != v.member_end()) {
					types::deserializer des(iter.value());
					static_cast<types::visitor_base*>(&des)->visit(args);
				} else {
					if constexpr (!_details::is_optional_v<Param>) {
						logger::get().log_error(CP_HERE) << "request expects parameters but none is found";
						return;
					}
				}
				callback(id, c, std::move(args));
			}
		};
		return handler;
	}

	template <
		typename Param, typename Callback
	> inline client::request_handler client::request_handler::create_notification_handler(Callback &&cb) {
		request_handler handler;
		handler.callback = [callback = std::forward<Callback>(cb)](const json::object_t &v, client &c) {
			if constexpr (std::is_same_v<Param, std::nullopt_t>) {
				callback(c);
			} else {
				Param args;
				if (auto iter = v.find_member(u8"params"); iter != v.member_end()) {
					types::deserializer des(iter.value());
					static_cast<types::visitor_base*>(&des)->visit(args);
				} else {
					if constexpr (!_details::is_optional_v<Param>) {
						logger::get().log_error(CP_HERE) << "notification expects parameters but none is found";
						return;
					}
				}
				callback(c, std::move(args));
			}
		};
		return handler;
	}
}
