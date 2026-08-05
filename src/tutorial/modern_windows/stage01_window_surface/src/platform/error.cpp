#include "mw/platform/error.h"

#include <format> // std::format

namespace mw {
namespace {

// Resolve a numeric status into the localized sentence the OS stores for it.
// Covers the common Win32 + standard HRESULT codes; vendor HRESULTs (e.g.
// many DXGI_* values) have no registered message and come back empty.
std::wstring system_message(unsigned long code) {
    constexpr DWORD kBufCch = 512; // cch = count of chars (wide)
    wchar_t buf[kBufCch] = {};

    const DWORD len = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                     /*lpSource*/ nullptr, code,
                                     MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), buf, kBufCch,
                                     /*Arguments*/ nullptr);

    if (len == 0) {
        return {}; // no description registered for this code
    }

    // System messages ship with a trailing "\r\n"; peel it off so the result
    // composes into a tidy one-liner inside error_message().
    std::wstring text(buf, len);
    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' ')) {
        text.pop_back();
    }
    return text;
}

} // namespace

std::wstring Error::error_message() const {
    const wchar_t* k = kind == Kind::HResult ? L"HRESULT"
                       : kind == Kind::Win32 ? L"Win32"
                                             : L"Panic";

    std::wstring detail = kind == Kind::Panic ? L"internal panic" : system_message(error_code);
    if (detail.empty()) {
        detail = L"<no system description>";
    }

    return std::format(L"[{}] 0x{:08X}: {}", k, error_code, detail);
}

} // namespace mw
