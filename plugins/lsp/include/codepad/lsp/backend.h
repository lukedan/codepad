// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Declaration of a general LSP backend used for sending and receiving messages.

#include <sstream>
#include <deque>

#include <rapidjson/stringbuffer.h>

#include "types/serialization.h"

namespace codepad::lsp {
	/// A backend used to communicate with LSP servers.
	class backend {
	public:
		/// Default virtual destructor.
		virtual ~backend() = default;

		/// Prepends a header to the JSON message and sends it to the server.
		void send_message(std::u8string_view json_data) {
			std::ostringstream ss;
			ss << "Content-Length: " << json_data.size() << "\r\n\r\n";
			std::string s = ss.str();

			_send_bytes(s.data(), s.size());
			_send_bytes(json_data.data(), json_data.size());
		}
		/// Receives a message. This function blocks until a message has been received.
		[[nodiscard]] std::u8string receive_message() {
			std::size_t message_length = 0;

			std::string accum;
			while (true) {
				accum.clear();
				bool last_return = false;
				while (true) {
					char new_char = _read_byte();
					if (last_return && new_char == '\n') {
						accum.pop_back();
						break;
					} else {
						last_return = (new_char == '\r');
					}
					accum.push_back(new_char);
				}
				if (accum.empty()) { // end of header
					break;
				}

				std::size_t split = 1;
				for (; split < accum.size(); ++split) {
					if (accum[split - 1] == ':' && accum[split] == ' ') {
						break;
					}
				}
				std::string_view header(accum.begin(), accum.begin() + split - 1);
				if (header == "Content-Length") {
					// TODO handle errors?
					std::from_chars(accum.data() + split + 1, accum.data() + accum.size(), message_length);
				}
			}

			return _read_bulk(message_length);
		}


		/// Sends a request or a notification. Whether it's a request or a notification depends on the \p id
		/// parameter.
		template <typename Id, typename Send> void send_message(
			std::u8string_view method, Send &send, Id &&id
		) {
			// compose message
			rapidjson::StringBuffer stream;
			rapidjson::Writer<rapidjson::StringBuffer> writer(stream);
			writer.StartObject();
			{
				writer.Key("jsonrpc");
				writer.String("2.0");

				if constexpr (!std::is_same_v<std::decay_t<Id>, std::nullopt_t>) {
					writer.Key("id");
					if constexpr (std::is_integral_v<std::decay_t<Id>>) {
						writer.Int(id);
					} else {
						writer.String(reinterpret_cast<const char*>(id.data()), id.size());
					}
				}

				writer.Key("method");
				writer.String(reinterpret_cast<const char*>(method.data()), method.size());

				if constexpr (!std::is_same_v<std::remove_cv_t<Send>, std::nullopt_t>) {
					writer.Key("params");
					types::serializer<rapidjson::StringBuffer> sr(writer);
					sr.visit(send);
				}
			}
			writer.EndObject();
			// send message
			send_message(std::u8string_view(reinterpret_cast<const char8_t*>(stream.GetString()), stream.GetSize()));
		}
	protected:
		std::vector<char> _read_buffer; ///< Caches bytes that have been read.
		std::size_t _offset = 0; /// Offset of the current position in \ref _read_buffer.

		/// Reads another byte from the server.
		[[nodiscard]] char _read_byte() {
			constexpr static std::size_t _buffer_size = 8192;
			
			if (_offset == _read_buffer.size()) {
				_read_buffer.resize(_buffer_size);
				std::size_t count = 0;
				do {
					count = _receive_bytes(_read_buffer.data(), _read_buffer.size());
				} while (count == 0);
				_read_buffer.resize(count);
				_offset = 0;
			}
			return _read_buffer[_offset++];
		}
		/// Reads the specified amount of bytes.
		[[nodiscard]] std::u8string _read_bulk(std::size_t len) {
			std::u8string result(len, '\0');
			char8_t *target = result.data();
			if (_offset < _read_buffer.size()) {
				std::size_t num_bytes = _read_buffer.size() - _offset;
				if (len <= num_bytes) {
					// special case: already have the data needed
					std::memcpy(result.data(), _read_buffer.data() + _offset, len);
					_offset += len;
					return result;
				}

				std::memcpy(target, _read_buffer.data() + _offset, num_bytes);
				_offset = _read_buffer.size();
				len -= num_bytes;
				target += num_bytes;
			}
			// receive the rest of the data
			while (len > 0) {
				std::size_t num_bytes = _receive_bytes(target, len);
				target += num_bytes;
				len -= num_bytes;
			}
			return result;
		}

		/// Sends the given bytes to the server.
		virtual void _send_bytes(const void*, std::size_t) = 0;
		/// Receives bytes from the server. Blocks until any bytes are received. This function may be called from
		/// a thread other than the one this object is created on.
		///
		/// \return The actual number of bytes received.
		virtual std::size_t _receive_bytes(void*, std::size_t) = 0;
	};
}
