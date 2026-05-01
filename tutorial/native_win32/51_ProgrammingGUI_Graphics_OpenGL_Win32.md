# 通用GUI编程技术——图形渲染实战（五十一）——Win32嵌入OpenGL：从WGL到Core Profile

> 上一篇文章我们深入了命中测试和鼠标事件路由——从 `PtInRect` 的矩形判断到 Direct2D Geometry 的精确几何测试，从 `SetCapture`/`ReleaseCapture` 的鼠标捕获到 Rubber Band 拖拽选择框。至此，我们完成了 Win32 平台上自定义控件绘制的三个层次：Owner-Draw（在系统控件框架内定制外观）、完全自绘控件（从零构建控件架构）、以及精确命中测试与交互。
>
> 现在我们把视野进一步拓展。前几篇我们用的都是微软自家的图形 API——GDI、Direct2D、D3D11/12。但 GUI 图形渲染的世界不止 DirectX。OpenGL 是另一套广泛使用的 GPU 渲染 API，跨平台是它的核心优势。今天我们就来看看如何在 Win32 窗口中嵌入 OpenGL 渲染。

## 前言：为什么要在 Win32 中用 OpenGL

你可能会问：Windows 上有 DirectX 全家桶，为什么还要折腾 OpenGL？

答案通常有两个。第一，跨平台需求。如果你的渲染代码需要在 Windows、macOS 和 Linux 上运行，OpenGL 是最自然的选择——同一套渲染逻辑只需要替换平台相关的窗口创建代码，核心的 GL 调用在三个平台上完全一致。第二，学习和技能覆盖。OpenGL 和 DirectX 代表了两种不同的 API 设计哲学——OpenGL 是状态机模型（全局状态 + 函数调用），DirectX 是对象模型（COM 接口 + 方法调用）。理解两种范式有助于你形成更全面的图形编程视野。

当然，在 Windows 上 OpenGL 的性能通常不如 DirectX——因为 OpenGL 的 Windows 实现（WGL）需要经过额外的翻译层才能调用底层的 DXGI/D3D 运行时。但对于 GUI 应用和工具开发来说，这个性能差异通常可以忽略。

## 环境说明

- **操作系统**: Windows 10/11
- **编译器**: MSVC (Visual Studio 2022)
- **OpenGL 版本**: 3.3 Core Profile（兼容性最广泛的现代版本）
- **扩展加载库**: GLAD（推荐）或 GLEW
- **链接库**: `opengl32.lib`（系统自带，仅包含 WGL 和 GL 1.1 的基础函数）
- **前置知识**: 文章 37-42（D3D11 渲染管线概念，Shader 基础）

## WGL：Windows 上的 OpenGL 胶合层

在 Windows 上创建 OpenGL 上下文的机制叫做 WGL（Windows GL）。它是一组以 `wgl` 为前缀的 Win32 扩展函数，负责将 OpenGL 渲染输出与 Win32 窗口关联起来。WGL 的核心函数只有三个：`wglCreateContext`（创建 OpenGL 上下文）、`wglMakeCurrent`（将上下文绑定到当前线程的 DC）和 `wglDeleteContext`（销毁上下文）。

但这里有一个让初学者非常困惑的问题：这三个函数只能创建"兼容模式"的 OpenGL 上下文——它会暴露 OpenGL 的所有历史版本功能，包括 1.x 时代的立即模式（`glBegin`/`glEnd`）。如果你想要一个现代的 Core Profile 上下文（比如 OpenGL 3.3），你不能直接用 `wglCreateContext`，而必须使用 WGL 的扩展函数 `wglCreateContextAttribsARB`。

这就引出了一个鸡生蛋蛋生鸡的问题：要使用 `wglCreateContextAttribsARB`，你需要先通过 `wglGetProcAddress` 获取这个函数的指针；但要调用 `wglGetProcAddress`，你需要先有一个有效的 OpenGL 上下文。解决方案是"两步法"——先创建一个临时的兼容模式上下文，用它获取扩展函数指针，然后销毁临时上下文，再用 `wglCreateContextAttribsARB` 创建正式的 Core Profile 上下文。

这个流程确实有点绕，但只有第一次需要这么做。在实际项目中，你通常会把这段初始化代码封装成一个函数，之后就不用再管了。

## PIXELFORMATDESCRIPTOR：选择像素格式

在创建任何 OpenGL 上下文之前，你需要为窗口的设备上下文（DC）选择一个像素格式。像素格式定义了帧缓冲区的属性——颜色深度、深度缓冲位数、模板缓冲位数、是否双缓冲等。

```cpp
PIXELFORMATDESCRIPTOR pfd = {};
pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
pfd.nVersion = 1;
pfd.dwFlags =
    PFD_DRAW_TO_WINDOW |    // 绘制到窗口（不是位图）
    PFD_SUPPORT_OPENGL |    // 支持 OpenGL
    PFD_DOUBLEBUFFER;       // 双缓冲
pfd.iPixelType = PFD_TYPE_RGBA;          // RGBA 颜色模式
pfd.cColorBits = 32;                      // 32 位颜色（每通道 8 位）
pfd.cDepthBits = 24;                      // 24 位深度缓冲
pfd.cStencilBits = 8;                     // 8 位模板缓冲
pfd.iLayerType = PFD_MAIN_PLANE;          // 主绘图层

// 选择最匹配的像素格式
int pixelFormat = ChoosePixelFormat(hDC, &pfd);
if (pixelFormat == 0)
{
    // 没有找到合适的像素格式
    return false;
}

// 设置像素格式
if (!SetPixelFormat(hDC, pixelFormat, &pfd))
{
    // 设置失败
    return false;
}
```

`ChoosePixelFormat` 根据 `PIXELFORMATDESCRIPTOR` 中指定的属性，在系统支持的像素格式列表中找到最匹配的一个。`SetPixelFormat` 将选定的像素格式绑定到设备上下文。

⚠️ 注意一个关键约束：**一个窗口的像素格式只能在设置一次后不能再改变**。如果你调用 `SetPixelFormat` 后又调用了一次，会失败。这意味着如果你需要切换到不同的像素格式（比如从 16 位切到 32 位颜色深度），你必须销毁窗口并重新创建。

根据 [Microsoft Learn - SetPixelFormat](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-setpixelformat) 的文档，设置像素格式后，窗口的设备上下文就与 OpenGL 的帧缓冲区配置关联起来了。后续创建的 OpenGL 上下文会使用这个像素格式来分配帧缓冲区。

## 第一步——临时上下文与扩展加载

现在我们来走完"两步法"的完整流程。

### 创建临时上下文

```cpp
#include <windows.h>
#include <gl/GL.h>
#pragma comment(lib, "opengl32.lib")

// 全局变量
HGLRC g_hGLRC = nullptr;   // OpenGL 渲染上下文

// 函数指针类型定义
typedef HGLRC (WINAPI* PFN_wglCreateContextAttribsARB)(
    HDC hdc, HGLRC hShareContext, const int* attribList);
PFN_wglCreateContextAttribsARB wglCreateContextAttribsARB = nullptr;

bool InitOpenGL(HWND hwnd)
{
    HDC hDC = GetDC(hwnd);

    // 设置像素格式
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pixelFormat = ChoosePixelFormat(hDC, &pfd);
    if (!SetPixelFormat(hDC, pixelFormat, &pfd))
    {
        ReleaseDC(hwnd, hDC);
        return false;
    }

    // 创建临时 OpenGL 上下文
    HGLRC hTempRC = wglCreateContext(hDC);
    if (!hTempRC)
    {
        ReleaseDC(hwnd, hDC);
        return false;
    }

    // 激活临时上下文
    wglMakeCurrent(hDC, hTempRC);
```

到这里，我们有了一个可用的（虽然是兼容模式的）OpenGL 上下文。现在可以通过 `wglGetProcAddress` 获取扩展函数指针了。

### 获取 wglCreateContextAttribsARB

```cpp
    // 获取创建 Core Profile 上下文所需的扩展函数
    wglCreateContextAttribsARB = (PFN_wglCreateContextAttribsARB)
        wglGetProcAddress("wglCreateContextAttribsARB");

    if (!wglCreateContextAttribsARB)
    {
        // 系统不支持 WGL_ARB_create_context 扩展
        // 退回到兼容模式上下文（或者报错退出）
        wglDeleteContext(hTempRC);
        ReleaseDC(hwnd, hDC);
        return false;
    }

    // 销毁临时上下文
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hTempRC);
```

`wglGetProcAddress` 和 Win32 的 `GetProcAddress` 不同——它是专门用于获取 OpenGL 扩展函数指针的。根据 [Microsoft Learn - wglGetProcAddress](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-wglgetprocaddress) 的文档，它必须在有效的 OpenGL 上下文上调用，否则返回 NULL。

### 创建正式的 Core Profile 上下文

```cpp
    // 定义 OpenGL 3.3 Core Profile 上下文属性
    int attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,        // 主版本号 3
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,        // 次版本号 3
        WGL_CONTEXT_PROFILE_MASK_ARB,
            WGL_CONTEXT_CORE_PROFILE_BIT_ARB,     // Core Profile
        0  // 终止标记
    };

    g_hGLRC = wglCreateContextAttribsARB(hDC, nullptr, attribs);
    if (!g_hGLRC)
    {
        ReleaseDC(hwnd, hDC);
        return false;
    }

    // 激活正式上下文
    wglMakeCurrent(hDC, g_hGLRC);
```

`WGL_CONTEXT_MAJOR_VERSION_ARB` 和 `WGL_CONTEXT_MINOR_VERSION_ARB` 指定了 OpenGL 的版本。`WGL_CONTEXT_CORE_PROFILE_BIT_ARB` 表示我们要求一个 Core Profile 上下文——这意味着所有被标记为废弃的旧功能（立即模式、固定管线等）都不可用。你必须使用着色器（Shader）和顶点缓冲（VBO）来渲染任何东西。

OpenGL 3.3 Core Profile 是现代 OpenGL 开发中最常用的版本——它提供了足够的现代特性（着色器、FBO、VAO、实例化绘制等），同时被几乎所有 2008 年以后制造的 GPU 支持。如果你需要更高级的特性（Compute Shader、Tessellation Shader），可以要求 4.3 或 4.6 版本，但兼容性会相应降低。

## 第二步——使用 GLAD 加载 OpenGL 函数

创建好 Core Profile 上下文后，你只能使用 OpenGL 1.1 的基础函数（这些在 `opengl32.lib` 中导出）。所有 1.1 之后的函数——包括 `glGenBuffers`、`glCreateShader`、`glGenVertexArrays` 这些最基本的现代 OpenGL 函数——都需要通过 `wglGetProcAddress` 动态加载。

手动加载数百个函数指针是不现实的。这就是扩展加载库（Extension Loader）的用武之地。GLAD 是目前最流行的选择——你访问 [GLAD Web Service](https://glad.dav1d.de/)，选择需要的 OpenGL 版本和扩展，它会生成一套头文件和源文件，包含了所有函数指针的声明和加载代码。

使用 GLAD 的步骤：

1. 在 [GLAD Web Service](https://glad.dav1d.de/) 上选择 Language: C/C++，Specification: OpenGL，API: gl Version 3.3，Profile: Core，Extensions: 按需勾选。
2. 下载生成的 `glad.h`、`glad.c` 和 `khrplatform.h`。
3. 在项目中包含 `glad.c` 并链接 `opengl32.lib`。

初始化 GLAD 的代码只有一行——在创建并激活 OpenGL 上下文之后调用：

```cpp
#include <glad/glad.h>

// ... 创建上下文后 ...

// 加载所有 OpenGL 3.3 Core 函数指针
if (!gladLoadGL())
{
    // GLAD 加载失败
    return false;
}

// 确认版本
int major = GLVersion.major;
int minor = GLVersion.minor;
// major 应该 >= 3, minor 应该 >= 3
```

加载成功后，你就可以自由使用所有 OpenGL 3.3 Core Profile 的函数了——`glGenBuffers`、`glBufferData`、`glCreateShader`、`glCompileShader` 等等。

## 第三步——渲染旋转三角形

现在我们有了一个完整的 OpenGL 3.3 Core Profile 环境，可以写一个最小的渲染示例了。我们用 VAO（Vertex Array Object）+ VBO（Vertex Buffer Object）+ 着色器来渲染一个旋转的彩色三角形。

### 着色器代码

```glsl
// ===== 顶点着色器 =====
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

out vec3 vColor;

uniform mat4 uMVP;

void main()
{
    gl_Position = uMVP * vec4(aPos, 1.0);
    vColor = aColor;
}

// ===== 片段着色器 =====
#version 330 core

in vec3 vColor;
out vec4 FragColor;

void main()
{
    FragColor = vec4(vColor, 1.0);
}
```

如果你之前学过 HLSL，GLSL 的语法会感觉很熟悉。`#version 330 core` 对应 OpenGL 3.3 Core Profile。`layout (location = 0)` 类似 HLSL 中的语义绑定，指定顶点属性在 VAO 中的位置。`uniform` 变量类似 HLSL 的 `cbuffer` 成员，用于从 CPU 传递参数。

### C++ 端的完整渲染流程

```cpp
// 顶点数据
float vertices[] = {
    // 位置              // 颜色
    -0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,  // 左下 - 红
     0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,  // 右下 - 绿
     0.0f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,  // 顶部 - 蓝
};

// 创建 VAO 和 VBO
GLuint VAO, VBO;
glGenVertexArrays(1, &VAO);
glGenBuffers(1, &VBO);

// 绑定 VAO（记录后续的顶点属性配置）
glBindVertexArray(VAO);

// 绑定 VBO 并上传数据
glBindBuffer(GL_ARRAY_BUFFER, VBO);
glBufferData(GL_ARRAY_BUFFER, sizeof(vertices),
    vertices, GL_STATIC_DRAW);

// 配置顶点属性
// 位置属性：location 0，3个float，步长 6 个 float，偏移 0
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
    6 * sizeof(float), (void*)0);
glEnableVertexAttribArray(0);

// 颜色属性：location 1，3个float，步长 6 个 float，偏移 3 个 float
glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
    6 * sizeof(float), (void*)(3 * sizeof(float)));
glEnableVertexAttribArray(1);

// 解绑 VAO（保存配置）
glBindVertexArray(0);
```

OpenGL 的 VAO/VBO 模型和 D3D11 的顶点缓冲 + 输入布局模型是对应的。VAO 相当于输入布局（`ID3D11InputLayout`）——它记录了顶点属性的配置（哪些字段、什么类型、什么偏移）。VBO 相当于顶点缓冲（`ID3D11Buffer`）——它存储实际的顶点数据。区别在于 OpenGL 用 `glVertexAttribPointer` 在绑定 VAO 的状态下隐式关联 VBO，而 D3D11 用 `IASetVertexBuffers` 在渲染时显式绑定。

渲染循环中：

```cpp
void RenderGL(float angle)
{
    // 清屏
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 使用着色器程序
    glUseProgram(shaderProgram);

    // 计算旋转矩阵（简化：只用 2D 旋转）
    float s = sinf(angle), c = cosf(angle);
    float mvp[16] = {
        c,    s,    0.0f, 0.0f,
        -s,   c,    0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };

    // 传递 uniform 变量
    glUniformMatrix4fv(
        glGetUniformLocation(shaderProgram, "uMVP"),
        1, GL_FALSE, mvp);

    // 绑定 VAO 并绘制
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    // 交换缓冲区
    HDC hDC = GetDC(g_hwnd);
    SwapBuffers(hDC);
    ReleaseDC(g_hwnd, hDC);
}
```

注意 `SwapBuffers` 是 Win32 的函数，不是 OpenGL 的。它负责将后台缓冲区的内容显示到窗口上——相当于 D3D11 的 `SwapChain->Present()`。

## ⚠️ 踩坑预警

**坑点一：旧式 wglCreateContext 只能创建兼容模式上下文**

如果你直接用 `wglCreateContext` 而不通过 `wglCreateContextAttribsARB`，你得到的是一个兼容模式上下文。在兼容模式下，你可以使用 `glBegin`/`glEnd` 这种立即模式的 API，但这些 API 在 Core Profile 下完全不存在。如果你的代码计划在 macOS 上运行（macOS 只支持 Core Profile），兼容模式代码会直接编译失败。养成习惯：始终用 `wglCreateContextAttribsARB` 创建 Core Profile 上下文。

**坑点二：像素格式设置后不能改变**

一个窗口的像素格式只能设置一次。如果你尝试对已经设置过像素格式的窗口再次调用 `SetPixelFormat`，函数会失败并返回 FALSE。如果你需要切换像素格式，唯一的办法是销毁窗口并重新创建。

**坑点三：wglMakeCurrent 的线程亲和性**

OpenGL 上下文在同一时刻只能被一个线程使用。如果你在主线程上调用了 `wglMakeCurrent(hDC, hRC)`，然后在工作线程上也调用 `wglMakeCurrent`，主线程的上下文会被自动解除绑定。OpenGL 的操作不是线程安全的——你需要自己管理上下文的线程亲和性。

**坑点四：GLAD 头文件必须在 GL.h 之前包含**

GLAD 的头文件 `glad/glad.h` 会重新定义 OpenGL 的函数声明为函数指针。如果你在 `glad.h` 之前包含了系统自带的 `<GL/gl.h>`，函数声明会冲突。确保 `glad.h` 是你包含的第一个 OpenGL 相关头文件。

## 常见问题

### Q: GLAD 和 GLEW 有什么区别？

两者都是 OpenGL 扩展加载库，核心功能相同。GLEW 是更老的库，代码生成是预编译的；GLAD 更轻量，通过 Web 服务按需生成。GLAD 的代码更清晰，维护更活跃，是当前社区的推荐选择。

### Q: OpenGL 和 Vulkan 的关系是什么？

Vulkan 是 OpenGL 的"下一代"——由同一个组织（Khronos）制定，设计目标是更底层的 GPU 控制（类似 D3D12 相对 D3D11 的关系）。OpenGL 仍然是活跃的标准，适合大多数 GUI 和工具开发场景。Vulkan 更适合对性能有极致要求的应用（游戏引擎、GPU 计算）。

### Q: Windows 上 OpenGL 的性能如何？

对于 GUI 应用和工具开发，性能完全足够。Windows 的 OpenGL 实现通过翻译层映射到 DXGI/D3D 运行时，有一些额外开销，但在渲染负载不高的场景下可以忽略。如果你的应用对 GPU 性能极其敏感，DirectX 在 Windows 上确实有优势。

## 总结

这篇我们完成了在 Win32 窗口中嵌入 OpenGL 的全流程。

核心机制是 WGL——Windows 提供的 OpenGL 胶合层。我们通过 `PIXELFORMATDESCRIPTOR` 选择像素格式（颜色深度、深度缓冲、双缓冲），通过"两步法"创建 Core Profile 上下文（先用 `wglCreateContext` 创建临时上下文获取 `wglCreateContextAttribsARB` 函数指针，再创建正式的 OpenGL 3.3 Core Profile 上下文），通过 GLAD 扩展加载库加载所有现代 OpenGL 函数指针。

渲染方面，我们用 VAO + VBO + GLSL 着色器的标准模式渲染了一个旋转的彩色三角形——这与 D3D11 的顶点缓冲 + 输入布局 + HLSL 着色器是直接对应的，概念可以无缝迁移。

到这里，我们已经掌握了在 Win32 窗口中嵌入 OpenGL 的完整流程。接下来可以尝试用 OpenGL 实现更复杂的 3D 场景，或者将 OpenGL 与 Direct2D/DirectWrite 混合使用以实现文字叠加渲染。

---

## 练习

1. 完成旋转三角形的完整程序：创建 Win32 窗口，初始化 OpenGL 3.3 Core Profile（两步法），编译 GLSL 着色器，创建 VAO/VBO，在渲染循环中让三角形绕 Z 轴旋转。

2. 修改三角形为立方体：添加深度缓冲测试（`glEnable(GL_DEPTH_TEST)`），创建 36 个顶点（6 面 x 2 三角形 x 3 顶点），用透视投影矩阵和视图矩阵渲染一个 3D 立方体。

3. 为 OpenGL 程序添加纹理支持：创建一个 `GL_TEXTURE_2D` 纹理对象，用 `glTexImage2D` 上载纹理数据，在片段着色器中用 `sampler2D` 采样纹理，将纹理贴到立方体的每个面上。

4. 对比 OpenGL 的 VAO/VBO + GLSL 和 D3D11 的 InputLayout + VertexBuffer + HLSL：写出两者的渲染初始化和渲染调用代码对照表，理解它们在概念上的对应关系。

---

**参考资料**:
- [OpenGL on Windows - OpenGL Wiki](https://www.khronos.org/opengl/wiki/Platform_specifics:_Windows)
- [wglCreateContext function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-wglcreatecontext)
- [PIXELFORMATDESCRIPTOR structure - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-pixelformatdescriptor)
- [SetPixelFormat function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-setpixelformat)
- [wglGetProcAddress function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-wglgetprocaddress)
- [GLAD - OpenGL Loader Generator](https://glad.dav1d.de/)
- [WGL_ARB_create_context extension spec](https://www.khronos.org/registry/OpenGL/extensions/ARB/WGL_ARB_create_context.txt)
