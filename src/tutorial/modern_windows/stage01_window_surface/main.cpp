#include "mw/windows_factory.h"
#include "mw/window.h"
#include "mw/drawing_src.h"
#include "mw/window_event.h"
#include "mw/window_observer.h"
#include <Windows.h>
#include <cstdint>

namespace {
// 把窗口事件接到 DrawingSurface：resize 时重建缓冲 + 重画一帧。
class SurfaceObserver : public mw::WindowsObserver {
    mw::DrawingSurface* surface_;
  public:
    explicit SurfaceObserver(mw::DrawingSurface* s) : surface_(s) {}
    void onEvent(const mw::IEvent& e) override {
        if (e.type != mw::IEvent::Type::Resize) return;
        const auto& r = static_cast<const mw::ResizeEvent&>(e);
        if (SUCCEEDED(surface_->resize(static_cast<std::uint32_t>(r.newSize.width),
                                       static_cast<std::uint32_t>(r.newSize.height)))) {
            surface_->render();
        }
    }
};
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int show) {
    mw::DrawingSurface surface;
    SurfaceObserver    observer(&surface);

    mw::WindowFactory factory;
    auto made = factory.setTitle(L"器官一·窗口与绘制面")
                       .setSize({960, 600})
                       .setPosition({CW_USEDEFAULT, 0})
                       .setObserver(&observer)
                       .make();
    if (!made) {
        MessageBoxW(nullptr, made.error().error_message().c_str(), L"建窗失败", MB_ICONERROR);
        return 1;
    }
    auto win = std::move(*made);

    // 有 HWND 了，初始化绘制面（建窗期间早到的 WM_SIZE 已被 resize 的"未初始化"分支挡掉）
    if (FAILED(surface.create(win->native_handle()))) {
        MessageBoxW(nullptr, L"DrawingSurface::create 失败", L"绘制面失败", MB_ICONERROR);
        return 1;
    }

    ShowWindow(win->native_handle(), show);
    surface.render(); // 首帧

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
