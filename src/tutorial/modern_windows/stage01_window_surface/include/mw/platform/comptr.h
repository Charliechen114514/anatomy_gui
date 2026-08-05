#pragma once

#include <wrl/client.h>

namespace mw {
/**
 * Microsoft makes a long long name, make a short one :)
 */
template <typename Src> using ComPtr = Microsoft::WRL::ComPtr<Src>;
} // namespace mw
