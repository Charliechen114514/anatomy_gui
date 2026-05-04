/**
 * 01_opengl_win32 - Win32 嵌入 OpenGL 示例
 *
 * 本程序演示了在 Win32 窗口中嵌入 OpenGL 3.3 Core Profile 渲染的完整流程：
 * 1. ChoosePixelFormat / SetPixelFormat — 设置像素格式（双缓冲 + OpenGL 支持）
 * 2. wglCreateContext / wglMakeCurrent   — 创建并激活 OpenGL 渲染上下文
 * 3. gladLoadGLLoader                    — 使用 GLAD 加载 OpenGL 3.3 函数指针
 * 4. 编译 GLSL 顶点/片段着色器             — 创建着色器程序
 * 5. 创建 VBO + VAO                      — 配置顶点数据（位置 + 颜色）
 * 6. PeekMessage 渲染循环                 — 每帧清屏、旋转三角形、SwapBuffers
 * 7. WM_SIZE: glViewport                 — 窗口大小变化时调整视口
 * 8. wglDeleteContext                    — 退出时清理 OpenGL 资源
 *
 * 渲染内容：一个绕 Z 轴旋转的 RGB 彩色三角形。
 *
 * 参考: tutorial/native_win32/51_ProgrammingGUI_Graphics_OpenGL_Win32.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <glad/glad.h>
#include <cmath>

#pragma comment(lib, "opengl32.lib")

// ============================================================================
// 常量定义
// ============================================================================

static const wchar_t* WINDOW_CLASS_NAME = L"OpenGLWin32Class";
static const int      WINDOW_WIDTH      = 800;
static const int      WINDOW_HEIGHT     = 600;

// ============================================================================
// 全局 OpenGL 资源
// ============================================================================

static HGLRC  g_hGLRC        = nullptr;   // OpenGL 渲染上下文
static HWND   g_hwnd         = nullptr;   // 主窗口句柄
static GLuint g_shaderProgram = 0;        // 着色器程序
static GLuint g_VAO           = 0;        // 顶点数组对象
static GLuint g_VBO           = 0;        // 顶点缓冲对象
static GLint  g_uniformMVP   = -1;        // uMVP uniform 位置

// ============================================================================
// GLSL 着色器源码
// ============================================================================

// 顶点着色器：接收位置和颜色，应用 MVP 变换，传递颜色到片段着色器
static const char* VERTEX_SHADER_SRC = R"(
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
)";

// 片段着色器：直接输出插值后的顶点颜色
static const char* FRAGMENT_SHADER_SRC = R"(
#version 330 core

in vec3 vColor;
out vec4 FragColor;

void main()
{
    FragColor = vec4(vColor, 1.0);
}
)";

// ============================================================================
// OpenGL 初始化
// ============================================================================

/**
 * 创建并编译一个着色器
 *
 * @param type  着色器类型（GL_VERTEX_SHADER 或 GL_FRAGMENT_SHADER）
 * @param src   着色器源码字符串
 * @return 着色器对象 ID，编译失败返回 0
 */
static GLuint CompileShader(GLenum type, const char* src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    // 检查编译状态
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char infoLog[512] = {};
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        OutputDebugStringA("着色器编译失败: ");
        OutputDebugStringA(infoLog);
        OutputDebugStringA("\n");
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

/**
 * 创建着色器程序（顶点 + 片段）
 *
 * @return 着色器程序 ID，链接失败返回 0
 */
static GLuint CreateShaderProgram()
{
    // 编译顶点着色器
    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, VERTEX_SHADER_SRC);
    if (vertexShader == 0) return 0;

    // 编译片段着色器
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER_SRC);
    if (fragmentShader == 0)
    {
        glDeleteShader(vertexShader);
        return 0;
    }

    // 链接着色器程序
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // 检查链接状态
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char infoLog[512] = {};
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        OutputDebugStringA("着色器程序链接失败: ");
        OutputDebugStringA(infoLog);
        OutputDebugStringA("\n");
        glDeleteProgram(program);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }

    // 着色器已链接到程序中，可以删除着色器对象
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

/**
 * 创建顶点缓冲（VBO）和顶点数组对象（VAO）
 *
 * 顶点数据包含位置（vec3）和颜色（vec3），交错存储。
 * 三角形的三个顶点分别为红、绿、蓝色。
 */
static void CreateVertexBuffer()
{
    // 顶点数据：位置 (x, y, z) + 颜色 (r, g, b)
    float vertices[] = {
        // 位置              // 颜色
        -0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,   // 左下 - 红色
         0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,   // 右下 - 绿色
         0.0f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   // 顶部 - 蓝色
    };

    // 生成 VAO 和 VBO
    glGenVertexArrays(1, &g_VAO);
    glGenBuffers(1, &g_VBO);

    // 绑定 VAO（记录后续的顶点属性配置）
    glBindVertexArray(g_VAO);

    // 绑定 VBO 并上传顶点数据
    glBindBuffer(GL_ARRAY_BUFFER, g_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 配置顶点属性 0：位置（3 个 float，步长 6 个 float，偏移 0）
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 配置顶点属性 1：颜色（3 个 float，步长 6 个 float，偏移 3 个 float）
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 解绑 VAO（保存配置，后续只需 glBindVertexArray 即可恢复全部状态）
    glBindVertexArray(0);
}

/**
 * 初始化 OpenGL 环境
 *
 * 完整流程：设置像素格式 -> 创建 OpenGL 上下文 -> 加载函数指针 ->
 *           创建着色器程序 -> 创建顶点缓冲
 *
 * @param hwnd 窗口句柄
 * @return 成功返回 true
 */
static bool InitOpenGL(HWND hwnd)
{
    HDC hDC = GetDC(hwnd);

    // ------------------------------------------------------------------
    // 第 1 步：设置像素格式
    // ------------------------------------------------------------------
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize      = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW |    // 绘制到窗口
                     PFD_SUPPORT_OPENGL |    // 支持 OpenGL
                     PFD_DOUBLEBUFFER;       // 双缓冲
    pfd.iPixelType = PFD_TYPE_RGBA;          // RGBA 颜色模式
    pfd.cColorBits = 32;                      // 32 位颜色深度
    pfd.cDepthBits = 24;                      // 24 位深度缓冲
    pfd.cStencilBits = 8;                     // 8 位模板缓冲
    pfd.iLayerType = PFD_MAIN_PLANE;          // 主绘图层

    int pixelFormat = ChoosePixelFormat(hDC, &pfd);
    if (pixelFormat == 0)
    {
        MessageBox(hwnd, L"ChoosePixelFormat 失败!", L"错误", MB_ICONERROR);
        ReleaseDC(hwnd, hDC);
        return false;
    }

    if (!SetPixelFormat(hDC, pixelFormat, &pfd))
    {
        MessageBox(hwnd, L"SetPixelFormat 失败!", L"错误", MB_ICONERROR);
        ReleaseDC(hwnd, hDC);
        return false;
    }

    // ------------------------------------------------------------------
    // 第 2 步：创建 OpenGL 上下文并激活
    // ------------------------------------------------------------------
    g_hGLRC = wglCreateContext(hDC);
    if (!g_hGLRC)
    {
        MessageBox(hwnd, L"wglCreateContext 失败!", L"错误", MB_ICONERROR);
        ReleaseDC(hwnd, hDC);
        return false;
    }

    if (!wglMakeCurrent(hDC, g_hGLRC))
    {
        MessageBox(hwnd, L"wglMakeCurrent 失败!", L"错误", MB_ICONERROR);
        wglDeleteContext(g_hGLRC);
        g_hGLRC = nullptr;
        ReleaseDC(hwnd, hDC);
        return false;
    }

    ReleaseDC(hwnd, hDC);

    // ------------------------------------------------------------------
    // 第 3 步：使用 GLAD 加载 OpenGL 函数指针
    //
    // wglGetProcAddress 只能在有效的 OpenGL 上下文上调用，
    // 所以必须在 wglMakeCurrent 之后执行。
    // ------------------------------------------------------------------
    int loaded = gladLoadGLLoader((GLADloadproc)wglGetProcAddress);
    if (loaded == 0)
    {
        MessageBox(hwnd, L"GLAD 加载 OpenGL 函数失败!", L"错误", MB_ICONERROR);
        return false;
    }

    // ------------------------------------------------------------------
    // 第 4 步：创建着色器程序
    // ------------------------------------------------------------------
    g_shaderProgram = CreateShaderProgram();
    if (g_shaderProgram == 0)
    {
        MessageBox(hwnd, L"着色器程序创建失败!", L"错误", MB_ICONERROR);
        return false;
    }

    // 获取 uniform 变量位置
    g_uniformMVP = glGetUniformLocation(g_shaderProgram, "uMVP");

    // ------------------------------------------------------------------
    // 第 5 步：创建顶点缓冲（VBO + VAO）
    // ------------------------------------------------------------------
    CreateVertexBuffer();

    return true;
}

// ============================================================================
// 渲染
// ============================================================================

/**
 * 渲染一帧画面
 *
 * @param angle 旋转角度（弧度）
 */
static void RenderFrame(float angle)
{
    // 获取窗口客户区尺寸
    RECT clientRc;
    GetClientRect(g_hwnd, &clientRc);
    int width  = clientRc.right - clientRc.left;
    int height = clientRc.bottom - clientRc.top;

    // 清屏（深蓝灰色背景）
    glClearColor(0.10f, 0.10f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // 使用着色器程序
    glUseProgram(g_shaderProgram);

    // 构建绕 Z 轴旋转的 4x4 矩阵（列主序存储）
    // OpenGL 使用列主序（Column-Major），矩阵在内存中按列排列
    float s = sinf(angle);
    float c = cosf(angle);
    float mvp[16] = {
        c,    s,    0.0f, 0.0f,    // 第 1 列
        -s,   c,    0.0f, 0.0f,    // 第 2 列
        0.0f, 0.0f, 1.0f, 0.0f,   // 第 3 列
        0.0f, 0.0f, 0.0f, 1.0f,   // 第 4 列
    };

    // 将旋转矩阵传递到顶点着色器的 uMVP uniform
    glUniformMatrix4fv(g_uniformMVP, 1, GL_FALSE, mvp);

    // 绑定 VAO 并绘制三角形
    glBindVertexArray(g_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    // 交换前后缓冲区（将渲染结果显示到屏幕）
    HDC hDC = GetDC(g_hwnd);
    SwapBuffers(hDC);
    ReleaseDC(g_hwnd, hDC);
}

// ============================================================================
// 资源清理
// ============================================================================

/**
 * 释放所有 OpenGL 资源
 */
static void CleanupOpenGL()
{
    if (g_hGLRC)
    {
        // 先解除上下文绑定
        wglMakeCurrent(nullptr, nullptr);

        // 删除 OpenGL 资源
        if (g_VAO)            glDeleteVertexArrays(1, &g_VAO);
        if (g_VBO)            glDeleteBuffers(1, &g_VBO);
        if (g_shaderProgram)  glDeleteProgram(g_shaderProgram);

        // 删除 OpenGL 渲染上下文
        wglDeleteContext(g_hGLRC);
        g_hGLRC = nullptr;
    }
}

// ============================================================================
// 窗口过程
// ============================================================================

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 保存窗口句柄
        g_hwnd = hwnd;

        // 初始化 OpenGL
        if (!InitOpenGL(hwnd))
        {
            MessageBox(hwnd, L"OpenGL 初始化失败!", L"错误", MB_ICONERROR);
            PostQuitMessage(1);
            return -1;
        }
        return 0;
    }

    case WM_DESTROY:
    {
        // 清理 OpenGL 资源
        CleanupOpenGL();
        PostQuitMessage(0);
        return 0;
    }

    case WM_SIZE:
    {
        // 窗口大小变化时调整 OpenGL 视口
        int width  = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (width > 0 && height > 0)
        {
            glViewport(0, 0, width, height);
        }
        return 0;
    }

    case WM_ERASEBKGND:
    {
        // 阻止系统擦除背景，由 OpenGL 负责全部绘制
        // 避免闪烁
        return 1;
    }

    case WM_PAINT:
    {
        // 仅验证更新区域，实际渲染在主循环中完成
        ValidateRect(hwnd, nullptr);
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// ============================================================================
// 注册窗口类
// ============================================================================

static ATOM RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = WindowProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;                  // OpenGL 自行管理背景
    wc.lpszMenuName  = nullptr;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    return RegisterClassEx(&wc);
}

// ============================================================================
// 创建主窗口
// ============================================================================

static HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow)
{
    HWND hwnd = CreateWindowEx(
        0,                              // 扩展窗口样式
        WINDOW_CLASS_NAME,              // 窗口类名称
        L"Win32 OpenGL 示例",           // 窗口标题
        WS_OVERLAPPEDWINDOW,            // 窗口样式
        CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_WIDTH, WINDOW_HEIGHT,    // 窗口大小 800x600
        nullptr, nullptr, hInstance, nullptr
    );

    if (hwnd == nullptr)
        return nullptr;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    return hwnd;
}

// ============================================================================
// 程序入口
// ============================================================================

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR     pCmdLine,
    int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);

    // 注册窗口类
    if (RegisterWindowClass(hInstance) == 0)
    {
        MessageBox(nullptr, L"窗口类注册失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    // 创建主窗口
    HWND hwnd = CreateMainWindow(hInstance, nCmdShow);
    if (hwnd == nullptr)
    {
        MessageBox(nullptr, L"窗口创建失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    // ======================================================================
    // 渲染循环 — 使用 PeekMessage 实现持续渲染
    //
    // 与普通 Win32 程序的 GetMessage 循环不同，OpenGL 渲染需要
    // 在没有消息时也持续绘制（每帧更新旋转角度）。
    // PeekMessage 在没有消息时立即返回 FALSE，让我们可以执行渲染。
    // ======================================================================

    LARGE_INTEGER freq = {};
    LARGE_INTEGER prevTime = {};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prevTime);

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        // 处理所有待处理的消息
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                break;
        }

        if (msg.message == WM_QUIT)
            break;

        // 计算经过的时间，用于驱动旋转动画
        LARGE_INTEGER currentTime = {};
        QueryPerformanceCounter(&currentTime);
        float deltaTime = (float)(currentTime.QuadPart - prevTime.QuadPart)
                        / (float)freq.QuadPart;
        prevTime = currentTime;

        // 根据时间计算旋转角度（每秒约旋转 90 度）
        static float angle = 0.0f;
        angle += deltaTime * 1.5708f;   // 90 度/秒 ≈ PI/2 弧度/秒

        // 渲染一帧
        RenderFrame(angle);
    }

    return (int)msg.wParam;
}
