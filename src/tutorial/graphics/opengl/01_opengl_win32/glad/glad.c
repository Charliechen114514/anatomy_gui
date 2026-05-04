/**
 * glad.c - Minimal GLAD Implementation for OpenGL 3.3 Core Profile
 *
 * 使用 wglGetProcAddress 加载 OpenGL 1.2+ 的函数指针，
 * 对于 1.1 基础函数则回退到 opengl32.dll 的 GetProcAddress。
 *
 * 完整版 GLAD 可从 https://glad.dav1d.de/ 生成。
 */

#define GLAD_IMPLEMENTATION
#include "glad.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

/**
 * gladLoadGLLoader - 加载所有 OpenGL 函数指针
 *
 * @param loader  函数指针加载回调，通常传入 wglGetProcAddress 的包装函数。
 *                loader("glGenBuffers") 应返回 glGenBuffers 的函数地址。
 * @return 成功加载的函数数量。
 *
 * 加载策略:
 *   - 优先使用 loader（即 wglGetProcAddress）加载所有函数。
 *     wglGetProcAddress 可以获取 OpenGL 1.2+ 的所有函数以及扩展函数。
 *   - 如果 loader 返回 NULL，对于 OpenGL 1.1 基础函数（如 glClear、
 *     glClearColor、glViewport、glDrawArrays），会尝试通过
 *     GetProcAddress 从 opengl32.dll 中获取。这些函数在 opengl32.lib
 *     中静态导出，因此总是可用的。
 */
int gladLoadGLLoader(GLADloadproc loader)
{
    if (loader == NULL)
        return 0;

    int loaded = 0;

    /* ================================================================== */
    /* OpenGL 1.1 基础函数 — 优先用 wglGetProcAddress，回退到 opengl32.dll */
    /* ================================================================== */

    glad_glClear = (PFNGLCLEARPROC)loader("glClear");
    if (glad_glClear == NULL)
    {
        HMODULE hOpenGL = GetModuleHandleA("opengl32.dll");
        if (hOpenGL)
            glad_glClear = (PFNGLCLEARPROC)GetProcAddress(hOpenGL, "glClear");
    }
    if (glad_glClear) loaded++;

    glad_glClearColor = (PFNGLCLEARCOLORPROC)loader("glClearColor");
    if (glad_glClearColor == NULL)
    {
        HMODULE hOpenGL = GetModuleHandleA("opengl32.dll");
        if (hOpenGL)
            glad_glClearColor = (PFNGLCLEARCOLORPROC)GetProcAddress(hOpenGL, "glClearColor");
    }
    if (glad_glClearColor) loaded++;

    glad_glViewport = (PFNGLVIEWPORTPROC)loader("glViewport");
    if (glad_glViewport == NULL)
    {
        HMODULE hOpenGL = GetModuleHandleA("opengl32.dll");
        if (hOpenGL)
            glad_glViewport = (PFNGLVIEWPORTPROC)GetProcAddress(hOpenGL, "glViewport");
    }
    if (glad_glViewport) loaded++;

    glad_glDrawArrays = (PFNGLDRAWARRAYSPROC)loader("glDrawArrays");
    if (glad_glDrawArrays == NULL)
    {
        HMODULE hOpenGL = GetModuleHandleA("opengl32.dll");
        if (hOpenGL)
            glad_glDrawArrays = (PFNGLDRAWARRAYSPROC)GetProcAddress(hOpenGL, "glDrawArrays");
    }
    if (glad_glDrawArrays) loaded++;

    /* ================================================================== */
    /* OpenGL 1.5 缓冲区对象函数 — 只能通过 wglGetProcAddress 获取          */
    /* ================================================================== */

    glad_glGenBuffers = (PFNGLGENBUFFERSPROC)loader("glGenBuffers");
    if (glad_glGenBuffers) loaded++;

    glad_glBindBuffer = (PFNGLBINDBUFFERPROC)loader("glBindBuffer");
    if (glad_glBindBuffer) loaded++;

    glad_glBufferData = (PFNGLBUFFERDATAPROC)loader("glBufferData");
    if (glad_glBufferData) loaded++;

    glad_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)loader("glDeleteBuffers");
    if (glad_glDeleteBuffers) loaded++;

    /* ================================================================== */
    /* OpenGL 2.0 着色器函数                                                */
    /* ================================================================== */

    glad_glCreateShader = (PFNGLCREATESHADERPROC)loader("glCreateShader");
    if (glad_glCreateShader) loaded++;

    glad_glShaderSource = (PFNGLSHADERSOURCEPROC)loader("glShaderSource");
    if (glad_glShaderSource) loaded++;

    glad_glCompileShader = (PFNGLCOMPILESHADERPROC)loader("glCompileShader");
    if (glad_glCompileShader) loaded++;

    glad_glAttachShader = (PFNGLATTACHSHADERPROC)loader("glAttachShader");
    if (glad_glAttachShader) loaded++;

    glad_glLinkProgram = (PFNGLLINKPROGRAMPROC)loader("glLinkProgram");
    if (glad_glLinkProgram) loaded++;

    glad_glUseProgram = (PFNGLUSEPROGRAMPROC)loader("glUseProgram");
    if (glad_glUseProgram) loaded++;

    glad_glGetShaderiv = (PFNGLGETSHADERIVPROC)loader("glGetShaderiv");
    if (glad_glGetShaderiv) loaded++;

    glad_glGetProgramiv = (PFNGLGETPROGRAMIVPROC)loader("glGetProgramiv");
    if (glad_glGetProgramiv) loaded++;

    glad_glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)loader("glGetShaderInfoLog");
    if (glad_glGetShaderInfoLog) loaded++;

    glad_glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)loader("glGetProgramInfoLog");
    if (glad_glGetProgramInfoLog) loaded++;

    glad_glDeleteShader = (PFNGLDELETESHADERPROC)loader("glDeleteShader");
    if (glad_glDeleteShader) loaded++;

    glad_glDeleteProgram = (PFNGLDELETEPROGRAMPROC)loader("glDeleteProgram");
    if (glad_glDeleteProgram) loaded++;

    glad_glCreateProgram = (PFNGLCREATEPROGRAMPROC)loader("glCreateProgram");
    if (glad_glCreateProgram) loaded++;

    glad_glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)loader("glGetUniformLocation");
    if (glad_glGetUniformLocation) loaded++;

    glad_glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)loader("glUniformMatrix4fv");
    if (glad_glUniformMatrix4fv) loaded++;

    /* ================================================================== */
    /* OpenGL 2.0 顶点属性函数                                              */
    /* ================================================================== */

    glad_glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)loader("glEnableVertexAttribArray");
    if (glad_glEnableVertexAttribArray) loaded++;

    glad_glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)loader("glVertexAttribPointer");
    if (glad_glVertexAttribPointer) loaded++;

    /* ================================================================== */
    /* OpenGL 3.0 顶点数组对象函数                                          */
    /* ================================================================== */

    glad_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)loader("glGenVertexArrays");
    if (glad_glGenVertexArrays) loaded++;

    glad_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)loader("glBindVertexArray");
    if (glad_glBindVertexArray) loaded++;

    glad_glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)loader("glDeleteVertexArrays");
    if (glad_glDeleteVertexArrays) loaded++;

    return loaded;
}
