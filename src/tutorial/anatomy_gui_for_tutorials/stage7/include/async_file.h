#pragma once
/// @file async_file.h — Asynchronous file reading via thread + postTask.

#include "application.h"
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace miniui {

class AsyncFile {
public:
    using ReadCallback = std::function<void(std::vector<uint8_t> data)>;

    /// Read entire file @p path on a background thread, deliver result via
    /// Application::postTask to the GUI thread.
    static void readAll(const std::string& path, ReadCallback callback) {
        std::thread([path, callback = std::move(callback)]() {
            FILE* f = fopen(path.c_str(), "rb");
            if (!f) {
                Application::instance().postTask([callback = std::move(callback)]() {
                    callback({});
                });
                return;
            }
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            std::vector<uint8_t> data(sz);
            fread(data.data(), 1, sz, f);
            fclose(f);

            Application::instance().postTask(
                [callback = std::move(callback), data = std::move(data)]() mutable {
                    callback(std::move(data));
                });
        }).detach();
    }
};

} // namespace miniui
