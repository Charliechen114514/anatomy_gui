#pragma once
#include <expected>
#include <string> // std::wstring

#ifndef _WIN32
#    error "error.h depends on FormatMessageW (Windows-only)."
#endif

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace mw {

// The single error type carried by every std::expected<T, Error> in the
// framework. One struct, three flavours of failure: COM/Direct HRESULTs,
// classic Win32 GetLastError() codes, and internal "should never happen" panics.
struct Error {
    enum class Kind { HResult, Win32, Panic } kind;
    unsigned long error_code;

    // "[Kind] 0xCODE: <OS message>". HResult/Win32 resolve a localized
    // description through the native FormatMessageW; Panic carries no system
    // text. Defined in src/error.cpp so this header stays free of <Windows.h>.
    std::wstring error_message() const;
};

template <typename T> using Result = std::expected<T, Error>;

inline std::unexpected<Error> from_hresult(HRESULT hr) {
    return std::unexpected(Error{Error::Kind::HResult, static_cast<unsigned long>(hr)});
}
inline std::unexpected<Error> from_win32(DWORD e = GetLastError()) {
    return std::unexpected(Error{Error::Kind::Win32, static_cast<unsigned long>(e)});
}

} // namespace mw
