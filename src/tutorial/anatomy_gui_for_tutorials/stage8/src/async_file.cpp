#include "async_file.h"
#include "application.h"

#include <fstream>
#include <sstream>
#include <cstdio>

namespace miniui {

Task<std::string> readFileAsync(const std::string& path) {
    // Simulate async: yield once to allow event loop to process other events
    co_await std::suspend_always{};

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    co_return ss.str();
}

Task<void> writeFileAsync(const std::string& path, const std::string& content) {
    // Simulate async: yield once
    co_await std::suspend_always{};

    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot write file: " + path);
    }

    file << content;
    co_return;
}

} // namespace miniui
