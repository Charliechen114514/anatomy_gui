#include "mw/drawing_src.h"

namespace mw {
namespace {
constexpr float kBgR = 0.106f, kBgG = 0.117f, kBgB = 0.133f; // 大致 #1B1E22
}

HRESULT DrawingSurface::bindTarget() {
    // 解绑旧目标 + 释放旧位图（resize 前必须先松手，否则 ResizeBuffers 报 INVALID_CALL）
    d2d_ctx_->SetTarget(nullptr);
    target_bitmap_.Reset();

    ComPtr<IDXGISurface> surface;
    HRESULT hr = swap_chain_->GetBuffer(0, IID_PPV_ARGS(&surface));
    if (FAILED(hr)) return hr;

    D2D1_BITMAP_PROPERTIES1 bp{};
    bp.pixelFormat =
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
    bp.dpiX = bp.dpiY = 96.0f;
    bp.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

    hr = d2d_ctx_->CreateBitmapFromDxgiSurface(surface.Get(), &bp, &target_bitmap_);
    if (FAILED(hr)) return hr;
    d2d_ctx_->SetTarget(target_bitmap_.Get());
    return S_OK;
}

HRESULT DrawingSurface::create(HWND hwnd) {
    hwnd_ = hwnd;

    // 1) D3D11 device —— BGRA_SUPPORT 是 D2D 互操作的硬要求
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    ComPtr<ID3D11DeviceContext> immCtx;
    D3D_FEATURE_LEVEL fl{};
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                   nullptr, 0, D3D11_SDK_VERSION, &d3d_, &fl, &immCtx);
    if (FAILED(hr)) return hr;
    hr = d3d_.As(&dxgi_dev_); // D3D11 device 同时也是 DXGI device
    if (FAILED(hr)) return hr;

    // 2) DXGI 交换链 —— ForComposition（不绑 HWND）+ FLIP_DISCARD
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory> factoryBase;
    dxgi_dev_->GetParent(IID_PPV_ARGS(&adapter));
    adapter->GetParent(IID_PPV_ARGS(&factoryBase));
    ComPtr<IDXGIFactory2> factory;
    factoryBase.As(&factory);

    RECT rc{};
    GetClientRect(hwnd_, &rc);
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width       = static_cast<UINT>(rc.right - rc.left);
    desc.Height      = static_cast<UINT>(rc.bottom - rc.top);
    desc.Format      = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc  = {1, 0};
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode   = DXGI_ALPHA_MODE_PREMULTIPLIED;
    desc.Scaling     = DXGI_SCALING_STRETCH;
    hr = factory->CreateSwapChainForComposition(d3d_.Get(), &desc, nullptr, &swap_chain_);
    if (FAILED(hr)) return hr;

    // 3) D2D 1.1 —— Factory1 → Device（由 DXGI device 喂）→ DeviceContext
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d_factory_.GetAddressOf());
    if (FAILED(hr)) return hr;
    hr = d2d_factory_->CreateDevice(dxgi_dev_.Get(), &d2d_device_);
    if (FAILED(hr)) return hr;
    hr = d2d_device_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2d_ctx_);
    if (FAILED(hr)) return hr;
    const FLOAT dpi = static_cast<FLOAT>(GetDpiForWindow(hwnd_));
    d2d_ctx_->SetDpi(dpi, dpi);
    hr = bindTarget();
    if (FAILED(hr)) return hr;

    // 4) DComp —— device → target(hwnd) → visual → SetContent(交换链) → SetRoot → Commit
    hr = DCompositionCreateDevice(dxgi_dev_.Get(), IID_PPV_ARGS(&dcomp_dev_));
    if (FAILED(hr)) return hr;
    hr = dcomp_dev_->CreateTargetForHwnd(hwnd_, TRUE, &dcomp_target_);
    if (FAILED(hr)) return hr;
    hr = dcomp_dev_->CreateVisual(&dcomp_visual_);
    if (FAILED(hr)) return hr;
    hr = dcomp_visual_->SetContent(swap_chain_.Get());
    if (FAILED(hr)) return hr;
    dcomp_target_->SetRoot(dcomp_visual_.Get());
    return dcomp_dev_->Commit(); // ⚠️ 漏调 Commit = 黑屏
}

HRESULT DrawingSurface::resize(std::uint32_t w, std::uint32_t h) {
    if (!swap_chain_ || w == 0 || h == 0) return S_OK; // 未 create / 尺寸为 0，跳过
    d2d_ctx_->SetTarget(nullptr);
    target_bitmap_.Reset();
    HRESULT hr = swap_chain_->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) return hr;
    return bindTarget();
}

void DrawingSurface::render() {
    if (!d2d_ctx_) return;
    d2d_ctx_->BeginDraw();
    d2d_ctx_->Clear(D2D1::ColorF(kBgR, kBgG, kBgB, 1.0f));
    HRESULT hr = d2d_ctx_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET || hr == DXGI_ERROR_DEVICE_REMOVED) {
        // TODO(后续器官)：设备丢失 → 重建整条管线。本阶段先记着不处理。
    }
    swap_chain_->Present(1, 0);
}

} // namespace mw
