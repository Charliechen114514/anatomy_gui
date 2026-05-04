/**
 * glad.h - Minimal GLAD Header for OpenGL 3.3 Core Profile
 *
 * 仅包含本教程示例所需的 OpenGL 3.3 核心函数和常量。
 * 完整版 GLAD 可从 https://glad.dav1d.de/ 生成。
 *
 * 用法:
 *   在 glad.c 中定义 GLAD_IMPLEMENTATION 后包含此头文件，
 *   即可生成函数指针变量和加载实现。
 */

#ifndef GLAD_H
#define GLAD_H

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/* 基础类型定义                                                               */
/* ========================================================================= */

#include <stddef.h>

typedef unsigned int      GLenum;
typedef int               GLint;
typedef unsigned int      GLuint;
typedef int               GLsizei;
typedef float             GLfloat;
typedef double            GLdouble;
typedef unsigned char     GLboolean;
typedef unsigned char     GLubyte;
typedef signed char       GLbyte;
typedef short             GLshort;
typedef unsigned short    GLushort;
typedef void              GLvoid;
typedef ptrdiff_t         GLintptr;
typedef ptrdiff_t         GLsizeiptr;
typedef char              GLchar;
typedef unsigned int      GLbitfield;

#ifndef GLAPI
#define GLAPI extern
#endif

#ifndef GLAPIENTRY
#define GLAPIENTRY
#endif

/* ========================================================================= */
/* OpenGL 常量定义（仅本教程使用的）                                            */
/* ========================================================================= */

#define GL_FALSE                          0
#define GL_TRUE                           1

#define GL_COLOR_BUFFER_BIT               0x00004000
#define GL_DEPTH_BUFFER_BIT               0x00000100

#define GL_TRIANGLES                      0x0004

#define GL_FLOAT                          0x1406

#define GL_ARRAY_BUFFER                   0x8892
#define GL_STATIC_DRAW                    0x88E4

#define GL_VERTEX_SHADER                  0x8B31
#define GL_FRAGMENT_SHADER                0x8B30

#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82

/* ========================================================================= */
/* 函数指针类型定义                                                            */
/* ========================================================================= */

/* OpenGL 1.0 / 1.1 基础函数 */
typedef void   (GLAPIENTRY *PFNGLCLEARPROC)(GLbitfield mask);
typedef void   (GLAPIENTRY *PFNGLCLEARCOLORPROC)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void   (GLAPIENTRY *PFNGLVIEWPORTPROC)(GLint x, GLint y, GLsizei width, GLsizei height);

/* OpenGL 2.0 着色器函数 */
typedef GLuint (GLAPIENTRY *PFNGLCREATESHADERPROC)(GLenum type);
typedef void   (GLAPIENTRY *PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
typedef void   (GLAPIENTRY *PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void   (GLAPIENTRY *PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void   (GLAPIENTRY *PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void   (GLAPIENTRY *PFNGLUSEPROGRAMPROC)(GLuint program);
typedef void   (GLAPIENTRY *PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint* params);
typedef void   (GLAPIENTRY *PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint* params);
typedef void   (GLAPIENTRY *PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef void   (GLAPIENTRY *PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef void   (GLAPIENTRY *PFNGLDELETESHADERPROC)(GLuint shader);
typedef void   (GLAPIENTRY *PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef GLuint (GLAPIENTRY *PFNGLCREATEPROGRAMPROC)(void);
typedef GLint  (GLAPIENTRY *PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar* name);
typedef void   (GLAPIENTRY *PFNGLUNIFORMMATRIX4FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);

/* OpenGL 1.5 缓冲区对象函数 */
typedef void   (GLAPIENTRY *PFNGLGENBUFFERSPROC)(GLsizei n, GLuint* buffers);
typedef void   (GLAPIENTRY *PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void   (GLAPIENTRY *PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
typedef void   (GLAPIENTRY *PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint* buffers);

/* OpenGL 3.0 顶点数组对象函数 */
typedef void   (GLAPIENTRY *PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint* arrays);
typedef void   (GLAPIENTRY *PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef void   (GLAPIENTRY *PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint* arrays);

/* OpenGL 2.0 顶点属性函数 */
typedef void   (GLAPIENTRY *PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void   (GLAPIENTRY *PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);

/* OpenGL 1.1 绘制函数 */
typedef void   (GLAPIENTRY *PFNGLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);

/* ========================================================================= */
/* 函数指针变量声明（由 GLAD_IMPLEMENTATION 定义）                               */
/* ========================================================================= */

#ifdef GLAD_IMPLEMENTATION
/* 在 glad.c 中定义变量 */
PFNGLCLEARPROC                   glad_glClear;
PFNGLCLEARCOLORPROC              glad_glClearColor;
PFNGLVIEWPORTPROC                glad_glViewport;
PFNGLCREATESHADERPROC            glad_glCreateShader;
PFNGLSHADERSOURCEPROC            glad_glShaderSource;
PFNGLCOMPILESHADERPROC           glad_glCompileShader;
PFNGLATTACHSHADERPROC            glad_glAttachShader;
PFNGLLINKPROGRAMPROC             glad_glLinkProgram;
PFNGLUSEPROGRAMPROC              glad_glUseProgram;
PFNGLGETSHADERIVPROC             glad_glGetShaderiv;
PFNGLGETPROGRAMIVPROC            glad_glGetProgramiv;
PFNGLGETSHADERINFOLOGPROC        glad_glGetShaderInfoLog;
PFNGLGETPROGRAMINFOLOGPROC       glad_glGetProgramInfoLog;
PFNGLDELETESHADERPROC            glad_glDeleteShader;
PFNGLDELETEPROGRAMPROC           glad_glDeleteProgram;
PFNGLCREATEPROGRAMPROC           glad_glCreateProgram;
PFNGLGETUNIFORMLOCATIONPROC      glad_glGetUniformLocation;
PFNGLUNIFORMMATRIX4FVPROC        glad_glUniformMatrix4fv;
PFNGLGENBUFFERSPROC              glad_glGenBuffers;
PFNGLBINDBUFFERPROC              glad_glBindBuffer;
PFNGLBUFFERDATAPROC              glad_glBufferData;
PFNGLDELETEBUFFERSPROC           glad_glDeleteBuffers;
PFNGLGENVERTEXARRAYSPROC         glad_glGenVertexArrays;
PFNGLBINDVERTEXARRAYPROC         glad_glBindVertexArray;
PFNGLDELETEVERTEXARRAYSPROC      glad_glDeleteVertexArrays;
PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray;
PFNGLVERTEXATTRIBPOINTERPROC     glad_glVertexAttribPointer;
PFNGLDRAWARRAYSPROC              glad_glDrawArrays;
#else
/* 在其他文件中声明为外部变量 */
GLAPI PFNGLCLEARPROC                   glad_glClear;
GLAPI PFNGLCLEARCOLORPROC              glad_glClearColor;
GLAPI PFNGLVIEWPORTPROC                glad_glViewport;
GLAPI PFNGLCREATESHADERPROC            glad_glCreateShader;
GLAPI PFNGLSHADERSOURCEPROC            glad_glShaderSource;
GLAPI PFNGLCOMPILESHADERPROC           glad_glCompileShader;
GLAPI PFNGLATTACHSHADERPROC            glad_glAttachShader;
GLAPI PFNGLLINKPROGRAMPROC             glad_glLinkProgram;
GLAPI PFNGLUSEPROGRAMPROC              glad_glUseProgram;
GLAPI PFNGLGETSHADERIVPROC             glad_glGetShaderiv;
GLAPI PFNGLGETPROGRAMIVPROC            glad_glGetProgramiv;
GLAPI PFNGLGETSHADERINFOLOGPROC        glad_glGetShaderInfoLog;
GLAPI PFNGLGETPROGRAMINFOLOGPROC       glad_glGetProgramInfoLog;
GLAPI PFNGLDELETESHADERPROC            glad_glDeleteShader;
GLAPI PFNGLDELETEPROGRAMPROC           glad_glDeleteProgram;
GLAPI PFNGLCREATEPROGRAMPROC           glad_glCreateProgram;
GLAPI PFNGLGETUNIFORMLOCATIONPROC      glad_glGetUniformLocation;
GLAPI PFNGLUNIFORMMATRIX4FVPROC        glad_glUniformMatrix4fv;
GLAPI PFNGLGENBUFFERSPROC              glad_glGenBuffers;
GLAPI PFNGLBINDBUFFERPROC              glad_glBindBuffer;
GLAPI PFNGLBUFFERDATAPROC              glad_glBufferData;
GLAPI PFNGLDELETEBUFFERSPROC           glad_glDeleteBuffers;
GLAPI PFNGLGENVERTEXARRAYSPROC         glad_glGenVertexArrays;
GLAPI PFNGLBINDVERTEXARRAYPROC         glad_glBindVertexArray;
GLAPI PFNGLDELETEVERTEXARRAYSPROC      glad_glDeleteVertexArrays;
GLAPI PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray;
GLAPI PFNGLVERTEXATTRIBPOINTERPROC     glad_glVertexAttribPointer;
GLAPI PFNGLDRAWARRAYSPROC              glad_glDrawArrays;
#endif

/* ========================================================================= */
/* 便捷宏：直接使用 glXxx 名称调用函数指针                                       */
/* ========================================================================= */

#define glClear                   glad_glClear
#define glClearColor              glad_glClearColor
#define glViewport                glad_glViewport
#define glCreateShader            glad_glCreateShader
#define glShaderSource            glad_glShaderSource
#define glCompileShader           glad_glCompileShader
#define glAttachShader            glad_glAttachShader
#define glLinkProgram             glad_glLinkProgram
#define glUseProgram              glad_glUseProgram
#define glGetShaderiv             glad_glGetShaderiv
#define glGetProgramiv            glad_glGetProgramiv
#define glGetShaderInfoLog        glad_glGetShaderInfoLog
#define glGetProgramInfoLog       glad_glGetProgramInfoLog
#define glDeleteShader            glad_glDeleteShader
#define glDeleteProgram           glad_glDeleteProgram
#define glCreateProgram           glad_glCreateProgram
#define glGetUniformLocation      glad_glGetUniformLocation
#define glUniformMatrix4fv        glad_glUniformMatrix4fv
#define glGenBuffers              glad_glGenBuffers
#define glBindBuffer              glad_glBindBuffer
#define glBufferData              glad_glBufferData
#define glDeleteBuffers           glad_glDeleteBuffers
#define glGenVertexArrays         glad_glGenVertexArrays
#define glBindVertexArray         glad_glBindVertexArray
#define glDeleteVertexArrays      glad_glDeleteVertexArrays
#define glEnableVertexAttribArray glad_glEnableVertexAttribArray
#define glVertexAttribPointer     glad_glVertexAttribPointer
#define glDrawArrays              glad_glDrawArrays

/* ========================================================================= */
/* GLAD 加载函数声明                                                           */
/* ========================================================================= */

/**
 * 加载函数指针的回调类型。
 * 参数 name 为函数名，返回值为函数指针（找不到则为 NULL）。
 */
typedef void* (* GLADloadproc)(const char *name);

/**
 * 使用指定的加载器加载所有 OpenGL 函数指针。
 *
 * 参数 loader: 函数指针加载回调（通常传入 wglGetProcAddress 的包装）。
 * 返回值: 成功加载的函数数量。
 */
int gladLoadGLLoader(GLADloadproc loader);

#ifdef __cplusplus
}
#endif

#endif /* GLAD_H */
