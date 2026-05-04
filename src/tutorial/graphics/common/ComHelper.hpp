/**
 * ComHelper.hpp - COM 智能指针和错误检查工具
 *
 * 用于 Direct2D / D3D11 / D3D12 / DirectWrite 等基于 COM 的图形 API。
 * 提供轻量级的 ComPtr<T> 和 HRESULT 检查函数。
 */

#pragma once

#include <windows.h>
#include <cstddef>
#include <cstdio>

// 轻量级 COM 智能指针
template <typename T>
class ComPtr
{
public:
    ComPtr() noexcept : m_ptr(nullptr) {}
    ComPtr(std::nullptr_t) noexcept : m_ptr(nullptr) {}
    explicit ComPtr(T* ptr) noexcept : m_ptr(ptr) { if (m_ptr) m_ptr->AddRef(); }

    ComPtr(const ComPtr& other) noexcept : m_ptr(other.m_ptr) { if (m_ptr) m_ptr->AddRef(); }
    ComPtr(ComPtr&& other) noexcept : m_ptr(other.m_ptr) { other.m_ptr = nullptr; }

    ~ComPtr() { Reset(); }

    ComPtr& operator=(const ComPtr& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            m_ptr = other.m_ptr;
            if (m_ptr) m_ptr->AddRef();
        }
        return *this;
    }

    ComPtr& operator=(ComPtr&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    void Reset() noexcept
    {
        if (m_ptr)
        {
            m_ptr->Release();
            m_ptr = nullptr;
        }
    }

    T* Get() const noexcept { return m_ptr; }
    T** GetAddressOf() noexcept { return &m_ptr; }
    T* operator->() const noexcept { return m_ptr; }
    operator T*() const noexcept { return m_ptr; }
    T** operator&() noexcept { return &m_ptr; }
    T& operator*() const noexcept { return *m_ptr; }
    explicit operator bool() const noexcept { return m_ptr != nullptr; }

    void Swap(ComPtr& other) noexcept
    {
        T* tmp = m_ptr;
        m_ptr = other.m_ptr;
        other.m_ptr = tmp;
    }

    // QueryInterface to a different COM interface
    template <typename U>
    HRESULT As(U** pp) const noexcept
    {
        return m_ptr->QueryInterface(__uuidof(U), (void**)pp);
    }

private:
    T* m_ptr;
};

// HRESULT 错误检查 — 失败时输出调试信息并退出
inline void ThrowIfFailed(HRESULT hr, const char* msg = nullptr)
{
    if (FAILED(hr))
    {
        char buf[256];
        if (msg)
            snprintf(buf, sizeof(buf), "COM Error: 0x%08X — %s", (unsigned)hr, msg);
        else
            snprintf(buf, sizeof(buf), "COM Error: 0x%08X", (unsigned)hr);

        OutputDebugStringA(buf);
        MessageBoxA(nullptr, buf, "COM Error", MB_ICONERROR | MB_OK);
        ExitProcess(hr);
    }
}

// 释放 COM 对象并置空（用于不使用 ComPtr 的简单示例）
template <typename T>
inline void SafeRelease(T*& ptr)
{
    if (ptr)
    {
        ptr->Release();
        ptr = nullptr;
    }
}
