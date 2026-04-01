# 通用GUI编程技术——图形渲染实战（五十二）——Qt QOpenGLWidget封装：跨平台GPU渲染

> 上一篇我们在 Win32 窗口中嵌入了 OpenGL——从 PIXELFORMATDESCRIPTOR 到两步法创建 Core Profile 上下文，从 GLAD 扩展加载到 VAO/VBO/GLSL 着色器的标准渲染流程。整个过程涉及大量平台相关的代码：WGL 函数、Win32 DC 管理、`SwapBuffers` 调用……这些代码在 Linux 上要换成 GLX（`glXCreateContext`），在 macOS 上要换成 CGL 或 NSOpenGL。
>
> 如果你希望你的 OpenGL 渲染代码能在不同平台上运行而不需要重写上下文创建逻辑，Qt 的 `QOpenGLWidget` 是一个非常好的封装方案。它把上下文创建、帧缓冲管理、像素格式选择、缓冲区交换这些平台细节全部隐藏在三个虚函数背后——`initializeGL`、`resizeGL`、`paintGL`。你只需要关注渲染逻辑本身。

## 前言：为什么选择 Qt + OpenGL

在 GUI 应用开发中，Qt 是 C++ 世界里最成熟的跨平台框架之一。它提供了丰富的控件库、信号槽机制、布局管理、事件系统等完整的 GUI 基础设施。`QOpenGLWidget` 则在 Qt 的控件体系内嵌入了一个 OpenGL 渲染视口——你可以把它当作一个普通的 Qt 控件来使用（放到布局中、响应事件、设置属性），同时它的内部由 OpenGL 来驱动渲染。

这种模式非常适合需要"GPU 加速渲染视口 + 传统 GUI 控件"混合布局的应用场景。比如一个 3D 建模工具——预览区域用 OpenGL 渲染 3D 场景，周围的工具栏、属性面板、状态栏用 Qt Widgets 实现。再比如一个数据可视化应用——OpenGL 渲染高性能的图表和热力图，Qt Widgets 提供参数调节界面。

## 环境说明

- **操作系统**: Windows 10/11（代码在 Linux/macOS 上同样可编译运行）
- **编译器**: MSVC (Visual Studio 2022) 或 MinGW
- **Qt 版本**: Qt 5.15+ 或 Qt 6.x（本文代码兼容两者）
- **CMake**: 3.16+（项目构建系统）
- **前置知识**: 文章 51（Win32 + OpenGL），Qt 基础使用经验

## QOpenGLWidget 的三函数模型

`QOpenGLWidget` 的工作方式非常简洁——你继承它，重写三个虚函数，Qt 框架会在合适的时机调用它们。根据 [Qt Documentation - QOpenGLWidget](https://doc.qt.io/qt-6/qopenglwidget.html) 的描述，三个函数的调用时机如下：

`initializeGL()` 在控件第一次显示时调用一次。这是你创建 OpenGL 资源的地方——编译着色器、创建 VAO/VBO、加载纹理、设置渲染状态。此时 OpenGL 上下文已经被 Qt 创建好并绑定为当前上下文，你可以直接调用 GL 函数。

`resizeGL(int w, int h)` 在控件尺寸发生变化时调用。参数是新的宽度和高度（像素单位）。这是你更新视口（`glViewport`）和投影矩阵的地方。

`paintGL()` 在控件需要重绘时调用。这是你执行实际渲染命令的地方——清屏、绑定着色器、绑定 VAO、调用 Draw 命令。调用结束后 Qt 会自动处理缓冲区交换，你不需要手动调用 `SwapBuffers`。

这个三函数模型和 GLUT 的 `init`/`reshape`/`display` 非常相似，也和 Android 的 `GLSurfaceView.Renderer` 接口如出一辙。如果你之前有任何 OpenGL 框架的使用经验，这个模型应该让你感到很亲切。

## 第一步——创建 QOpenGLWidget 子类

我们从定义一个继承自 `QOpenGLWidget` 的渲染控件开始：

```cpp
// GLWidget.h
#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QMatrix4x4>
#include <QTimer>

class GLWidget : public QOpenGLWidget,
                 protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    explicit GLWidget(QWidget* parent = nullptr);
    ~GLWidget();

protected:
    // QOpenGLWidget 的三个核心虚函数
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    // 鼠标交互
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    // OpenGL 资源
    QOpenGLShaderProgram* m_program = nullptr;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo;

    // 渲染状态
    QMatrix4x4 m_projection;
    QMatrix4x4 m_modelView;
    float m_rotationAngle = 0.0f;
    QPoint m_lastMousePos;

    // 动画定时器
    QTimer m_timer;
};
```

这里有几个值得展开的设计选择。

我们同时继承了 `QOpenGLWidget` 和 `QOpenGLFunctions_3_3_Core`。后者是一个"混入"类——它提供了 OpenGL 3.3 Core Profile 的所有函数声明。通过 `protected` 继承，我们可以在 `GLWidget` 的成员函数中直接调用 `glGenBuffers`、`glClear` 等 GL 函数，就像它们是本地函数一样。这比手动使用 GLAD 或 GLEW 更 Qt 风格。

`QOpenGLShaderProgram`、`QOpenGLVertexArrayObject`、`QOpenGLBuffer` 是 Qt 对 OpenGL 对象的 RAII 封装——它们在构造时创建底层 OpenGL 对象，在析构时自动释放。你不需要手动调用 `glDeleteBuffers` 或 `glDeleteVertexArrays`。

`QTimer` 用于驱动动画——每 16ms 触发一次 `update()` 调用，刷新渲染画面。

## 第二步——initializeGL：创建资源

```cpp
// GLWidget.cpp
#include "GLWidget.h"
#include <QMouseEvent>

GLWidget::GLWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    // 启用键盘和鼠标事件
    setFocusPolicy(Qt::StrongFocus);

    // 动画定时器：约 60fps
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        m_rotationAngle += 1.0f;
        update();  // 请求重绘（触发 paintGL）
    });
    m_timer.start(16);
}

GLWidget::~GLWidget()
{
    // 确保在析构前 OpenGL 上下文是当前的
    makeCurrent();

    // Qt 的 RAII 对象会自动释放
    delete m_program;

    // 释放 VAO 和 VBO
    m_vao.destroy();
    m_vbo.destroy();

    doneCurrent();
}

void GLWidget::initializeGL()
{
    // 初始化 OpenGL 3.3 Core 函数表
    if (!initializeOpenGLFunctions())
    {
        qCritical() << "Failed to initialize OpenGL functions";
        return;
    }

    // 输出 OpenGL 版本信息（调试用）
    qInfo() << "OpenGL Version:" << (const char*)glGetString(GL_VERSION);
    qInfo() << "GLSL Version:" << (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
    qInfo() << "Renderer:" << (const char*)glGetString(GL_RENDERER);

    // 设置清屏颜色
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

    // 启用深度测试
    glEnable(GL_DEPTH_TEST);

    // 创建并编译着色器
    m_program = new QOpenGLShaderProgram(this);

    // 顶点着色器（内嵌字符串）
    const char* vertexShaderSource = R"(
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

    // 片段着色器
    const char* fragmentShaderSource = R"(
        #version 330 core
        in vec3 vColor;
        out vec4 FragColor;
        void main()
        {
            FragColor = vec4(vColor, 1.0);
        }
    )";

    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex,
        vertexShaderSource);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment,
        fragmentShaderSource);
    m_program->link();

    // 创建顶点数据
    float vertices[] = {
        // 前面
        -0.5f, -0.5f,  0.5f,  1.0f, 0.2f, 0.2f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.2f, 0.2f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.2f, 0.2f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.2f, 0.2f,
        // 后面
         0.5f, -0.5f, -0.5f,  0.2f, 0.2f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.2f, 0.2f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.2f, 0.2f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.2f, 0.2f, 1.0f,
        // 顶面
        -0.5f,  0.5f,  0.5f,  0.2f, 1.0f, 0.2f,
         0.5f,  0.5f,  0.5f,  0.2f, 1.0f, 0.2f,
         0.5f,  0.5f, -0.5f,  0.2f, 1.0f, 0.2f,
        -0.5f,  0.5f, -0.5f,  0.2f, 1.0f, 0.2f,
        // 底面
        -0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.2f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.2f,
         0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.2f,
        -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.2f,
        // 右面
         0.5f, -0.5f,  0.5f,  0.2f, 1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.2f, 1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.2f, 1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.2f, 1.0f, 1.0f,
        // 左面
        -0.5f, -0.5f, -0.5f,  1.0f, 0.5f, 0.2f,
        -0.5f, -0.5f,  0.5f,  1.0f, 0.5f, 0.2f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.5f, 0.2f,
        -0.5f,  0.5f, -0.5f,  1.0f, 0.5f, 0.2f,
    };

    // 创建 VAO
    m_vao.create();
    m_vao.bind();

    // 创建 VBO
    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(vertices, sizeof(vertices));

    // 配置顶点属性
    // 位置属性
    m_program->enableAttributeArray(0);
    m_program->setAttributeBuffer(0, GL_FLOAT, 0, 3, 6 * sizeof(float));

    // 颜色属性
    m_program->enableAttributeArray(1);
    m_program->setAttributeBuffer(1, GL_FLOAT,
        3 * sizeof(float), 3, 6 * sizeof(float));

    // 解绑
    m_vao.release();
}
```

`initializeOpenGLFunctions()` 是 `QOpenGLFunctions_3_3_Core` 的方法，它负责加载所有 OpenGL 3.3 Core 函数的函数指针。这个调用等价于上一篇中 GLAD 的 `gladLoadGL()`——但它是 Qt 内置的，不需要额外引入第三方库。

Qt 的着色器编译通过 `QOpenGLShaderProgram` 封装——`addShaderFromSourceCode` 接受着色器源码字符串并编译，`link` 执行链接。编译和链接的错误可以通过 `log()` 方法获取，比原生的 `glGetShaderInfoLog` 更方便。

`setAttributeBuffer` 是 Qt 对 `glVertexAttribPointer` 的封装——它自动处理类型转换、偏移计算等细节。第一个参数对应着色器中的 `layout(location = N)`。

## 第三步——resizeGL 与 paintGL

```cpp
void GLWidget::resizeGL(int w, int h)
{
    // 更新视口
    glViewport(0, 0, w, h);

    // 更新投影矩阵
    m_projection.setToIdentity();
    m_projection.perspective(45.0f,          // FOV
        (float)w / (float)h,                 // 宽高比
        0.1f,                                // 近裁剪面
        100.0f);                             // 远裁剪面
}

void GLWidget::paintGL()
{
    // 清屏
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 计算模型视图矩阵
    m_modelView.setToIdentity();
    m_modelView.translate(0.0f, 0.0f, -3.0f);        // 后退3个单位
    m_modelView.rotate(m_rotationAngle,               // 旋转角度
        0.5f, 1.0f, 0.0f);                            // 绕(0.5, 1, 0)轴旋转

    // 计算MVP矩阵
    QMatrix4x4 mvp = m_projection * m_modelView;

    // 绑定着色器
    m_program->bind();

    // 传递MVP矩阵
    m_program->setUniformValue("uMVP", mvp);

    // 绘制立方体
    m_vao.bind();
    glDrawArrays(GL_QUADS, 0, 24);
    m_vao.release();

    m_program->release();
}
```

Qt 的 `QMatrix4x4` 提供了非常方便的矩阵运算接口——`perspective`、`translate`、`rotate` 这些方法直接对标 OpenGL 的固定管线函数（`gluPerspective`、`glTranslatef`、`glRotatef`），但它们操作的是 CPU 端的矩阵对象，最终通过 `setUniformValue` 传递给着色器。`setUniformValue` 是 Qt 对 `glUniform*` 系列函数的封装，支持 `QMatrix4x4`、`QVector3D`、`float`、`int` 等多种类型。

⚠️ 注意 `GL_QUADS` 在 OpenGL 3.3 Core Profile 中**已经被移除了**。这里为了简化示例代码长度，我使用了 `GL_QUADS`。在实际项目中，你需要把每个四边形拆分成两个三角形，使用 `GL_TRIANGLES` 和索引缓冲（`QOpenGLBuffer::IndexBuffer` + `glDrawElements`）。如果你在 Core Profile 上下文中使用 `GL_QUADS`，会得到渲染错误或者什么都不显示。

## makeCurrent() 与线程安全

`QOpenGLWidget` 有一个非常重要的线程安全机制：`makeCurrent()` 和 `doneCurrent()`。

默认情况下，`initializeGL`、`resizeGL` 和 `paintGL` 都在 Qt 的渲染线程中调用，OpenGL 上下文已经被自动绑定。但如果你需要在其他地方调用 OpenGL 函数——比如在鼠标事件处理函数中拾取像素（`glReadPixels`），或者在一个后台线程中上传纹理——你必须手动管理上下文绑定。

```cpp
void GLWidget::mousePressEvent(QMouseEvent* event)
{
    m_lastMousePos = event->pos();

    // 如果需要在这里调用 GL 函数，必须先绑定上下文
    makeCurrent();

    // 例如：读取点击位置的像素颜色
    // GLint viewport[4];
    // glGetIntegerv(GL_VIEWPORT, viewport);
    // unsigned char pixel[4];
    // glReadPixels(event->x(), viewport[3] - event->y(), 1, 1,
    //     GL_RGBA, GL_UNSIGNED_BYTE, pixel);

    doneCurrent();
}

void GLWidget::mouseMoveEvent(QMouseEvent* event)
{
    int dx = event->pos().x() - m_lastMousePos.x();
    int dy = event->pos().y() - m_lastMousePos.y();

    m_rotationAngle += dx * 0.5f;
    m_lastMousePos = event->pos();

    update();  // 请求重绘
}
```

⚠️ 在 `paintGL` 以外调用 OpenGL 函数时，**必须先调用 `makeCurrent()`**，结束后调用 `doneCurrent()`。如果你忘了 `makeCurrent()`，OpenGL 函数调用会失败——因为当前线程没有绑定的 OpenGL 上下文。如果你忘了 `doneCurrent()`，可能导致 Qt 的内部渲染流程出问题。`makeCurrent()` 会将 `QOpenGLWidget` 的 OpenGL 上下文绑定到当前线程；`doneCurrent()` 解除绑定。根据 Qt 文档的建议，大多数情况下你不需要手动调用这两个函数——只在 `paintGL` 以外的代码中需要 OpenGL 调用时才使用。

## 主窗口：OpenGL 视口 + 控件面板

`QOpenGLWidget` 最大的优势之一就是它可以和普通的 Qt 控件无缝混排。我们可以轻松实现一个"左侧 OpenGL 渲染视口 + 右侧参数控制面板"的布局：

```cpp
#include <QApplication>
#include <QMainWindow>
#include <QHBoxLayout>
#include <QSlider>
#include <QLabel>
#include <QGroupBox>
#include "GLWidget.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QMainWindow mainWindow;
    mainWindow.setWindowTitle("Qt OpenGL Demo");
    mainWindow.resize(1024, 600);

    // 中央控件
    QWidget* central = new QWidget(&mainWindow);
    mainWindow.setCentralWidget(central);

    // 水平布局
    QHBoxLayout* layout = new QHBoxLayout(central);

    // OpenGL 渲染视口
    GLWidget* glWidget = new GLWidget(central);
    glWidget->setMinimumSize(600, 400);
    layout->addWidget(glWidget, 3);  // stretch = 3

    // 右侧控制面板
    QGroupBox* controlPanel = new QGroupBox("Parameters", central);
    QVBoxLayout* panelLayout = new QVBoxLayout(controlPanel);

    // 旋转速度滑块
    panelLayout->addWidget(new QLabel("Rotation Speed:"));
    QSlider* speedSlider = new QSlider(Qt::Horizontal);
    speedSlider->setRange(0, 100);
    speedSlider->setValue(16);
    panelLayout->addWidget(speedSlider);

    // 状态信息
    QLabel* infoLabel = new QLabel("OpenGL 3.3 Core Profile");
    panelLayout->addWidget(infoLabel);

    panelLayout->addStretch();  // 底部弹性空间

    layout->addWidget(controlPanel, 1);  // stretch = 1

    mainWindow.show();
    return app.exec();
}
```

这就是 Qt 的威力——不到 50 行代码就创建了一个包含 OpenGL 渲染视口和控制面板的完整应用。同样的布局在 Win32 中需要几百行的窗口创建和布局代码。

## Qt + ImGui 集成

如果你需要在 OpenGL 视口中叠加即时模式的 UI（工具窗口、调试面板、属性编辑器），Qt + Dear ImGui 是一个非常实用的组合。ImGui 提供了 `imgui_impl_qt` 风格的集成方案，或者你可以使用 `QOpenGLWidget` + `imgui_impl_opengl3` 的标准后端组合。

集成的基本思路是：

1. 在 `initializeGL` 中初始化 ImGui 的 OpenGL 后端
2. 在 `paintGL` 的渲染结束后（3D 场景绘制完毕后），开始 ImGui 帧（`ImGui_ImplOpenGL3_NewFrame`），构建 UI 元素，然后调用 `ImGui_ImplOpenGL3_RenderDrawData`
3. Qt 的事件系统需要将鼠标和键盘事件转发给 ImGui

这个集成在 Dear ImGui 的示例代码中有完整的参考实现，我们这里就不展开了。

## ⚠️ 踩坑预警

**坑点一：在 paintGL 以外调用 GL 函数忘记 makeCurrent**

这是 `QOpenGLWidget` 使用中最高频的坑。在 `paintGL` 函数内部，Qt 已经帮你调用了 `makeCurrent()`。但如果你在其他任何地方（鼠标事件、定时器回调、工作线程）调用了 OpenGL 函数，必须先手动 `makeCurrent()`。否则你会得到一个 GL 错误或者程序崩溃。

**坑点二：析构顺序问题**

`QOpenGLWidget` 的析构函数会销毁 OpenGL 上下文。如果你的 OpenGL 资源（着色器、VBO、纹理）的析构发生在上下文销毁之后，`glDelete*` 调用会失败。解决方案是在 `GLWidget` 的析构函数中先调用 `makeCurrent()`，手动释放所有 OpenGL 资源，然后调用 `doneCurrent()`。

**坑点三：Qt5 和 Qt6 的 API 差异**

Qt 5 和 Qt 6 在 OpenGL 模块上有一些不兼容的改动。Qt 5 使用 `QOpenGLFunctions_3_3_Core`（通过 `QOpenGLFunctions` 体系），Qt 6 推荐使用 `QOpenGLFunctions`（自动适配当前上下文版本）。如果你需要同时支持两个版本，建议用 `#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)` 做条件编译。

**坑点四：GL_QUADS 在 Core Profile 中不可用**

如前所述，`GL_QUADS` 在 OpenGL 3.3 Core Profile 中已被移除。如果你的立方体使用了 `GL_QUADS`，在某些驱动上可能什么都不显示。正确做法是使用索引缓冲将四边形拆分为三角形。

## 常见问题

### Q: QOpenGLWidget 和 QGLWidget 有什么区别？

`QGLWidget` 是 Qt 4 时代的旧类，已被标记为废弃（Deprecated）。`QOpenGLWidget` 是 Qt 5.4 引入的替代品，架构更优（通过 framebuffer object 而非直接渲染到窗口），支持多采样、HiDPI 等现代特性。新项目应该始终使用 `QOpenGLWidget`。

### Q: QOpenGLWidget 支持 Vulkan 或 Direct3D 吗？

不直接支持。`QOpenGLWidget` 只封装了 OpenGL 上下文。如果你需要在 Qt 中使用 Vulkan，可以使用 `QVulkanWindow`（Qt 5.10+）或 `QRhi`（Qt 6 的跨平台渲染抽象层）。对于 Direct3D，Qt 没有内置的封装，你需要自己创建 D3D 设备并与 Qt 的窗口系统集成。

### Q: 如何在 QOpenGLWidget 中读取 GPU 帧缓冲的像素？

在 `paintGL` 中（上下文已经绑定的状态下），直接调用 `glReadPixels` 即可。如果需要在其他地方读取，先 `makeCurrent()`，读取完后 `doneCurrent()`。注意 `glReadPixels` 会触发 GPU-CPU 同步，在高帧率渲染中可能成为性能瓶颈。

## 总结

这篇我们用 Qt 的 `QOpenGLWidget` 封装实现了跨平台的 OpenGL 渲染。

核心是三函数模型：`initializeGL` 创建资源（着色器、VAO/VBO），`resizeGL` 更新视口和投影矩阵，`paintGL` 执行渲染命令。Qt 框架在背后处理了上下文创建、帧缓冲管理、像素格式选择、缓冲区交换等所有平台相关细节。

我们利用 Qt 的 RAII 封装类（`QOpenGLShaderProgram`、`QOpenGLVertexArrayObject`、`QOpenGLBuffer`）简化了 OpenGL 资源管理，利用 `QMatrix4x4` 简化了矩阵运算，利用 Qt 的布局系统轻松实现了 OpenGL 视口和传统控件面板的混合布局。`makeCurrent()`/`doneCurrent()` 的线程安全机制确保了在 `paintGL` 以外的代码中也能安全调用 OpenGL 函数。

至此，我们的"图形渲染实战"系列从 GDI 走到了跨平台 GPU 渲染——从 Windows 专有的 GDI/GDI+/Direct2D/D3D11/D3D12，到跨平台的 OpenGL 和 Qt 封装。接下来，我们将进入这个系列的下一个篇章——跨平台框架篇，探讨如何在不同的操作系统和图形 API 之上构建统一的 GUI 渲染架构。

---

## 练习

1. 在 Qt 应用中创建一个 `QOpenGLWidget` 子类，渲染一个旋转的彩色立方体。右侧添加控制面板，包含旋转速度滑块和 FOV 滑块，实时调整渲染参数。

2. 将上一篇（Win32 + OpenGL）中的旋转三角形迁移到 `QOpenGLWidget` 框架中。对比两种方案的代码量，理解 Qt 封装节省了多少平台相关的样板代码。

3. 实现鼠标拖拽旋转：在 `GLWidget` 中处理 `mousePressEvent`/`mouseMoveEvent`/`mouseReleaseEvent`，用鼠标水平移动量修改旋转角度，用垂直移动量修改俯仰角。提示：使用 `QMatrix4x4::rotate` 叠加旋转。

4. 在 `GLWidget` 中实现纹理渲染：创建一个 `QOpenGLTexture` 对象，从图片文件（`QImage`）加载纹理数据，在着色器中采样纹理并贴到立方体的每个面上。

---

**参考资料**:
- [QOpenGLWidget Class - Qt Documentation](https://doc.qt.io/qt-6/qopenglwidget.html)
- [QOpenGLFunctions Class - Qt Documentation](https://doc.qt.io/qt-6/qopenglfunctions.html)
- [QOpenGLShaderProgram Class - Qt Documentation](https://doc.qt.io/qt-6/qopenglshaderprogram.html)
- [Qt OpenGL Examples - Qt Documentation](https://doc.qt.io/qt-6/examples-opengl.html)
- [QMatrix4x4 Class - Qt Documentation](https://doc.qt.io/qt-6/qmatrix4x4.html)
- [OpenGL Window Example - Qt Documentation](https://doc.qt.io/qt-6/qtgui-openglwindow-example.html)
- [QOpenGLTexture Class - Qt Documentation](https://doc.qt.io/qt-6/qopengltexture.html)
