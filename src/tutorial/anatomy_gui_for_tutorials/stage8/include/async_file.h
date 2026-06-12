#pragma once
/// @file async_file.h
/// @brief Simple async file I/O helpers using coroutines.

#include "task.h"

#include <string>
#include <vector>

namespace miniui {

/// Read a file asynchronously (schedules on the event loop).
Task<std::string> readFileAsync(const std::string& path);

/// Write a file asynchronously (schedules on the event loop).
Task<void> writeFileAsync(const std::string& path, const std::string& content);

} // namespace miniui
