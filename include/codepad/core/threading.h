// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Threading utilities.

#include <thread>
#include <unordered_set>
#include <shared_mutex>

namespace codepad {
	/// A \p std::shared_mutex with additional run-time checks to prevent re-entrance. Use only for debugging.
	struct checked_shared_mutex {
	public:
		/// Locks this mutex in exlcusive mode.
		void lock() {
			auto id = std::this_thread::get_id();
			{
				std::lock_guard<std::mutex> guard(_record_mutex);
				assert_true_logical(_writer != id, "writer re-entrance");
			}
			_mutex.lock();
			{
				std::lock_guard<std::mutex> guard(_record_mutex);
				_writer.emplace(id);
			}
		}
		/// Unlocks this mutex in exlcusive mode.
		void unlock() {
			{
				std::lock_guard<std::mutex> guard(_record_mutex);
				assert_true_logical(_writer == std::this_thread::get_id(), "writer not locked");
				_writer.reset();
			}
			_mutex.unlock();
		}

		/// Locks this mutex in shared mode.
		void lock_shared() {
			auto id = std::this_thread::get_id();
			{
				std::lock_guard<std::mutex> guard(_record_mutex);
				assert_true_logical(_writer != id, "writer-reader re-entrance");
				auto iter = _readers.find(id);
				assert_true_logical(iter == _readers.end(), "reader re-entrance");
			}
			_mutex.lock_shared();
			{
				std::lock_guard<std::mutex> guard(_record_mutex);
				_readers.emplace(id);
			}
		}
		/// Unlocks this mutex in shared mode.
		void unlock_shared() {
			{
				std::lock_guard<std::mutex> guard(_record_mutex);
				auto iter = _readers.find(std::this_thread::get_id());
				assert_true_logical(iter != _readers.end(), "reader not locked");
				_readers.erase(iter);
			}
			_mutex.unlock_shared();
		}
	protected:
		std::shared_mutex _mutex; ///< The mutex.

		std::mutex _record_mutex; ///< Used to protect \ref _readers and \ref _writer.
		std::unordered_set<std::thread::id> _readers; ///< Readers.
		std::optional<std::thread::id> _writer; ///< The current writer, or \p std::nullopt.
	};
}
