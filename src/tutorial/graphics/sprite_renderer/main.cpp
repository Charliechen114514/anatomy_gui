/**
 * sprite_renderer — D3D11 2D 精灵渲染器阶段项目 (ch61)
 *
 * 本程序演示：
 * 1. SpriteBatch 批处理渲染 — 同纹理精灵合并为一次 Draw Call
 * 2. 顶点颜色调制 — 纯白纹理 × 顶点颜色 = 不同颜色的精灵
 * 3. GDI 文字离屏渲染 — 文字渲染到纹理，再作为精灵绘制
 * 4. 动态顶点缓冲 — 每帧 Map/Discard 重写顶点数据
 * 5. 500+ 精灵性能演示 + FPS 计数器
 * 6. 鼠标点击交互
 *
 * 参考: tutorial/native_win32/61_ProgrammingGUI_Graphics_D3D11_SpriteRenderer.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <dxgi.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <ctime>

#include "RenderWindow.hpp"
#include "SpriteBatch.hpp"
#include "TextureLoader.hpp"
#include "TextRenderer.hpp"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "user32.lib")

// ============================================================================
// 常量
// ============================================================================

static const int  WINDOW_WIDTH      = 1024;
static const int  WINDOW_HEIGHT     = 768;
static const int  SPRITE_COUNT      = 500;
static const int  SPRITE_SIZE       = 32;

// ============================================================================
// 精灵实例数据
// ============================================================================

struct SpriteInstance
{
    float x, y;              // 位置
    float baseSpeedX, baseSpeedY;  // 基础巡航速度
    float burstX, burstY;    // 爆炸偏移速度（快速衰减）
    float rotation;          // 当前旋转角度
    float rotSpeed;          // 旋转速度（弧度/秒）
    float scale;             // 缩放因子
    float r, g, b;           // 精灵颜色（顶点颜色调制）
};

// ============================================================================
// SpriteApp — 主应用类
// ============================================================================

class SpriteApp : public RenderWindow<SpriteApp>
{
public:
    void OnCreate()
    {
        srand(static_cast<unsigned>(time(nullptr)));

        InitD3D11();
        InitTextures();
        InitSprites();
        m_spriteBatch.Initialize(m_device.Get());
        m_textRenderer.Initialize(m_device.Get(), 256, 48);

        m_lastTime = GetTickCount64();
    }

    void OnResize(int width, int height)
    {
        if (width == 0 || height == 0 || !m_swapChain) return;

        m_renderTarget.Reset();
        ThrowIfFailed(
            m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0),
            "ResizeBuffers 失败");

        ComPtr<ID3D11Texture2D> backBuffer;
        ThrowIfFailed(
            m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuffer.GetAddressOf()),
            "GetBuffer 失败");

        ThrowIfFailed(
            m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTarget),
            "CreateRenderTargetView 失败");
    }

    void Render()
    {
        if (!m_context || !m_renderTarget) return;

        // 计算帧时间
        UINT64 now = GetTickCount64();
        float dt = static_cast<float>(now - m_lastTime) / 1000.0f;
        if (dt > 0.1f) dt = 0.1f;  // 防止大跳帧
        m_lastTime = now;

        // 更新精灵位置和旋转
        UpdateSprites(dt);

        // 更新 FPS
        m_frameCount++;
        if (now - m_lastFpsTime >= 1000)
        {
            m_fps = m_frameCount;
            m_frameCount = 0;
            m_lastFpsTime = now;

            wchar_t fpsText[64];
            swprintf(fpsText, 64, L"FPS: %d | Sprites: %d", m_fps, SPRITE_COUNT);
            m_textRenderer.RenderText(fpsText, 20.0f, 1.0f, 1.0f, 0.0f);
        }

        // 绑定渲染目标，再清屏
        m_context->OMSetRenderTargets(1, m_renderTarget.GetAddressOf(), nullptr);
        float clearColor[4] = { 0.05f, 0.05f, 0.12f, 1.0f };
        m_context->ClearRenderTargetView(m_renderTarget.Get(), clearColor);

        D3D11_VIEWPORT vp = {};
        vp.TopLeftX = 0; vp.TopLeftY = 0;
        vp.Width    = static_cast<float>(m_width);
        vp.Height   = static_cast<float>(m_height);
        vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &vp);

        // 开始精灵批处理渲染
        float screenW = static_cast<float>(m_width);
        float screenH = static_cast<float>(m_height);
        m_spriteBatch.Begin(m_context.Get(), screenW, screenH);

        // 绘制平铺背景
        float tileU = screenW / 256.0f;
        float tileV = screenH / 256.0f;
        m_spriteBatch.Draw(
            m_bgTexture.Get(),
            0, 0, screenW, screenH,
            0.0f,
            0.0f, 0.0f,
            1.0f, 1.0f, 1.0f, 1.0f,
            0.0f, 0.0f, tileU, tileV);

        // 绘制所有精灵（纯白纹理 + 顶点颜色调制）
        for (const auto& sprite : m_sprites)
        {
            float size = SPRITE_SIZE * sprite.scale;
            m_spriteBatch.Draw(
                m_spriteTexture.Get(),
                sprite.x, sprite.y, size, size,
                sprite.rotation,
                size * 0.5f, size * 0.5f,  // 原点居中
                sprite.r, sprite.g, sprite.b, 0.85f);
        }

        // 绘制 FPS 文字精灵
        m_spriteBatch.Draw(
            m_textRenderer.GetTextureSRV(),
            8.0f, 8.0f,
            static_cast<float>(m_textRenderer.GetWidth()),
            static_cast<float>(m_textRenderer.GetHeight()));

        m_spriteBatch.End();

        // 呈现
        HRESULT hr = m_swapChain->Present(1, 0);
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
            PostQuitMessage(1);
        }
    }

    void OnDestroy()
    {
        m_sprites.clear();
        m_bgTexture.Reset();
        m_spriteTexture.Reset();
        m_renderTarget.Reset();
        m_context.Reset();
        m_swapChain.Reset();
        m_device.Reset();
    }

    // 处理鼠标点击 — 让精灵向四周散开，然后自动回归巡航
    bool HandleMessage(HWND, UINT uMsg, WPARAM, LPARAM lParam, LRESULT&)
    {
        if (uMsg == WM_LBUTTONDOWN)
        {
            float mx = static_cast<float>(GET_X_LPARAM(lParam));
            float my = static_cast<float>(GET_Y_LPARAM(lParam));

            for (auto& sprite : m_sprites)
            {
                float dx = sprite.x + SPRITE_SIZE * sprite.scale * 0.5f - mx;
                float dy = sprite.y + SPRITE_SIZE * sprite.scale * 0.5f - my;
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist < 1.0f) dist = 1.0f;

                // 爆炸偏移：距离越近力越大，上限 800 px/s
                float force = 800.0f / (1.0f + dist * 0.01f);
                float randFactor = 0.7f + static_cast<float>(rand()) / RAND_MAX * 0.6f;
                sprite.burstX = (dx / dist) * force * randFactor;
                sprite.burstY = (dy / dist) * force * randFactor;
            }
            return true;
        }
        return false;
    }

private:
    // D3D11 核心资源
    ComPtr<ID3D11Device>           m_device;
    ComPtr<ID3D11DeviceContext>    m_context;
    ComPtr<IDXGISwapChain>         m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_renderTarget;

    // 精灵渲染器
    SpriteBatch  m_spriteBatch;
    TextRenderer m_textRenderer;

    // 纹理
    ComPtr<ID3D11ShaderResourceView> m_bgTexture;
    ComPtr<ID3D11ShaderResourceView> m_spriteTexture;

    // 精灵实例
    std::vector<SpriteInstance> m_sprites;

    // 计时
    UINT64 m_lastTime    = 0;
    int    m_fps         = 0;
    int    m_frameCount  = 0;
    UINT64 m_lastFpsTime = 0;

    void InitD3D11()
    {
        DXGI_SWAP_CHAIN_DESC scDesc = {};
        scDesc.BufferCount       = 1;
        scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scDesc.BufferDesc.RefreshRate.Numerator   = 60;
        scDesc.BufferDesc.RefreshRate.Denominator = 1;
        scDesc.SampleDesc.Count  = 1;
        scDesc.OutputWindow      = GetHwnd();
        scDesc.Windowed          = TRUE;
        scDesc.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;
        scDesc.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;

        D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_HARDWARE;
        D3D_FEATURE_LEVEL levels[] = {
            D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3
        };
        D3D_FEATURE_LEVEL selectedLevel;

        ThrowIfFailed(
            D3D11CreateDeviceAndSwapChain(
                nullptr, driverType, nullptr, 0,
                levels, _countof(levels), D3D11_SDK_VERSION,
                &scDesc, &m_swapChain, &m_device, &selectedLevel, &m_context),
            "D3D11CreateDeviceAndSwapChain 失败");

        ComPtr<ID3D11Texture2D> backBuffer;
        ThrowIfFailed(
            m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuffer.GetAddressOf()),
            "GetBuffer 失败");

        ThrowIfFailed(
            m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTarget),
            "CreateRenderTargetView 失败");
    }

    void InitTextures()
    {
        TextureLoader::CreateCheckerboard(
            m_device.Get(), 256, 256,
            0xFF442D2D, 0xFF361E1E, 32,
            m_bgTexture);

        TextureLoader::CreateSolidTexture(m_device.Get(), SPRITE_SIZE, m_spriteTexture);
    }

    void InitSprites()
    {
        m_sprites.resize(SPRITE_COUNT);
        for (int i = 0; i < SPRITE_COUNT; ++i)
        {
            auto& s = m_sprites[i];
            s.x = static_cast<float>(rand() % WINDOW_WIDTH);
            s.y = static_cast<float>(rand() % WINDOW_HEIGHT);
            s.baseSpeedX = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 120.0f;
            s.baseSpeedY = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 120.0f;
            s.burstX = 0.0f;
            s.burstY = 0.0f;
            s.rotation = 0.0f;
            s.rotSpeed = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 3.0f;
            s.scale = 0.8f + (static_cast<float>(rand()) / RAND_MAX) * 1.4f;

            // 随机 HSV 转颜色
            float hue = fmodf(i * 37.0f, 360.0f);
            float sat = 0.7f, val = 0.9f;
            float c = val * sat;
            float x = c * (1.0f - fabsf(fmodf(hue / 60.0f, 2.0f) - 1.0f));
            float m = val - c;
            float rr = 0, gg = 0, bb = 0;
            if      (hue < 60)  { rr = c; gg = x; }
            else if (hue < 120) { rr = x; gg = c; }
            else if (hue < 180) { gg = c; bb = x; }
            else if (hue < 240) { gg = x; bb = c; }
            else if (hue < 300) { rr = x; bb = c; }
            else                { rr = c; bb = x; }
            s.r = rr + m; s.g = gg + m; s.b = bb + m;
        }
    }

    void UpdateSprites(float dt)
    {
        float w = static_cast<float>(m_width);
        float h = static_cast<float>(m_height);
        if (w <= 0 || h <= 0) return;

        for (auto& s : m_sprites)
        {
            // 实际速度 = 基础巡航 + 爆炸偏移
            float vx = s.baseSpeedX + s.burstX;
            float vy = s.baseSpeedY + s.burstY;

            s.x += vx * dt;
            s.y += vy * dt;
            s.rotation += s.rotSpeed * dt;

            // 爆炸偏移快速衰减，回归基础速度
            s.burstX *= powf(0.02f, dt);
            s.burstY *= powf(0.02f, dt);

            float size = SPRITE_SIZE * s.scale;

            // 边界弹跳（只弹基础速度）
            if (s.x < 0)      { s.x = 0;      s.baseSpeedX = fabsf(s.baseSpeedX); }
            if (s.x + size > w) { s.x = w - size; s.baseSpeedX = -fabsf(s.baseSpeedX); }
            if (s.y < 0)      { s.y = 0;      s.baseSpeedY = fabsf(s.baseSpeedY); }
            if (s.y + size > h) { s.y = h - size; s.baseSpeedY = -fabsf(s.baseSpeedY); }
        }
    }
};

// ============================================================================
// 程序入口
// ============================================================================

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR pCmdLine,
    int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);

    ThrowIfFailed(
        CoInitializeEx(nullptr, COINIT_MULTITHREADED),
        "CoInitializeEx 失败");

    SpriteApp app;
    if (!app.Create(
        hInstance,
        L"D3D11SpriteRendererClass",
        L"D3D11 2D 精灵渲染器 — 点击鼠标让精灵散开",
        WINDOW_WIDTH, WINDOW_HEIGHT))
    {
        CoUninitialize();
        return 0;
    }

    int ret = app.RunMessageLoop();
    CoUninitialize();
    return ret;
}
