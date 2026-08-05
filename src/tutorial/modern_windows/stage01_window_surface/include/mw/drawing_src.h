#pragma once
#include "mw/platform/comptr.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h> // IDXGISwapChain1 / CreateSwapChainForComposition
#include <d2d1_1.h>  // ID2D1Factory1 / Device / DeviceContext / Bitmap1
#include <dcomp.h>   // IDComposition*

#include <cstdint>

namespace mw {

// 绘制面：独占整条 GPU 管线（D3D11 device + DXGI 交换链 + D2D 1.1 上下文 + DComp 视觉树），
// 对外只有一个职责——把"画一帧"变成屏幕上的像素。
class DrawingSurface {
  public:
    DrawingSurface() = default;
    ~DrawingSurface() = default;
    DrawingSurface(const DrawingSurface&) = delete;
    DrawingSurface& operator=(const DrawingSurface&) = delete;

    // 初始化整条管线并挂到这扇窗上。失败返回 HRESULT。
    HRESULT create(HWND hwnd);

    // 窗口尺寸变了：交换链缓冲重建 + 重新绑目标。未 create 时无动作
    //（建窗期间的 WM_SIZE 会早到，那时还没初始化）。
    HRESULT resize(std::uint32_t w, std::uint32_t h);

    // 画一帧：清成深灰，present。
    void render();

  private:
    HRESULT bindTarget(); // 后缓冲 → D2D 位图目标（首帧 & resize 后都要调）

    HWND hwnd_ = nullptr;
    ComPtr<ID3D11Device>         d3d_;
    ComPtr<IDXGIDevice>          dxgi_dev_;
    ComPtr<IDXGISwapChain1>      swap_chain_;
    ComPtr<ID2D1Factory1>        d2d_factory_;
    ComPtr<ID2D1Device>          d2d_device_;
    ComPtr<ID2D1DeviceContext>   d2d_ctx_;
    ComPtr<ID2D1Bitmap1>         target_bitmap_;
    ComPtr<IDCompositionDevice>  dcomp_dev_;
    ComPtr<IDCompositionTarget>  dcomp_target_;
    ComPtr<IDCompositionVisual>  dcomp_visual_;
};

} // namespace mw
