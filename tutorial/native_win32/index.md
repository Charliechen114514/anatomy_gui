# Win32 原生编程

从 Windows 最底层的 Win32 API 开始，系统学习窗口管理、消息机制、控件、对话框、资源文件、GDI/GDI+ 绘图、Direct2D、D3D11/D3D12 渲染。

## 章节列表

### 基础篇

| 章节 | 内容 | 文件 |
|:-----|:-----|:-----|
| 0 | Windows 程序的本质 | [0_ProgrammingGUI_Basic](0_ProgrammingGUI_Basic.md) |
| 1 | 消息机制与窗口创建 | [1_ProgrammingGUI_NativeWindows](1_ProgrammingGUI_NativeWindows.md) |
| 2 | 常用系统消息 | [2_ProgrammingGUI_NativeWindows_2](2_ProgrammingGUI_NativeWindows_2.md) |
| 3 | DPI 适配专题 | [3_ProgrammingGUI_WhatAboutDPI](3_ProgrammingGUI_WhatAboutDPI.md) |

### 控件篇

| 章节 | 内容 | 文件 |
|:-----|:-----|:-----|
| 4 | 标准控件（按钮、编辑框等） | [4_ProgrammingGUI_NativeWindows_Controls](4_ProgrammingGUI_NativeWindows_Controls.md) |
| 5 | WM_NOTIFY 通知消息 | [5_ProgrammingGUI_NativeWindows_WM_NOTIFY](5_ProgrammingGUI_NativeWindows_WM_NOTIFY.md) |
| 6 | ListView 列表视图控件 | [6_ProgrammingGUI_NativeWindows_ListView](6_ProgrammingGUI_NativeWindows_ListView.md) |
| 7 | TreeView 树形视图控件 | [7_ProgrammingGUI_NativeWindows_TreeView](7_ProgrammingGUI_NativeWindows_TreeView.md) |
| 8 | 更多控件（进度条、滚动条等） | [8_ProgrammingGUI_NativeWindows_MoreControls](8_ProgrammingGUI_NativeWindows_MoreControls.md) |

### 对话框篇

| 章节 | 内容 | 文件 |
|:-----|:-----|:-----|
| 9 | 模态对话框 | [9_ProgrammingGUI_NativeWindows_ModalDialog](9_ProgrammingGUI_NativeWindows_ModalDialog.md) |
| 10 | 非模态对话框 | [10_ProgrammingGUI_NativeWindows_ModelessDialog](10_ProgrammingGUI_NativeWindows_ModelessDialog.md) |
| 11 | 对话框过程深入 | [11_ProgrammingGUI_NativeWindows_DialogProc](11_ProgrammingGUI_NativeWindows_DialogProc.md) |

### 资源篇

| 章节 | 内容 | 文件 |
|:-----|:-----|:-----|
| 12 | 资源文件完全指南 | [12_ProgrammingGUI_NativeWindows_ResourceFiles](12_ProgrammingGUI_NativeWindows_ResourceFiles.md) |
| 13 | 菜单资源 | [13_ProgrammingGUI_NativeWindows_MenuResource](13_ProgrammingGUI_NativeWindows_MenuResource.md) |
| 14 | 图标、光标与位图 | [14_ProgrammingGUI_NativeWindows_IconCursorBitmap](14_ProgrammingGUI_NativeWindows_IconCursorBitmap.md) |
| 15 | 字符串表与国际化 | [15_ProgrammingGUI_NativeWindows_StringTable](15_ProgrammingGUI_NativeWindows_StringTable.md) |
| 16 | 对话框模板 | [16_ProgrammingGUI_NativeWindows_DialogTemplate](16_ProgrammingGUI_NativeWindows_DialogTemplate.md) |
| 17 | VS 资源编辑器 | [17_ProgrammingGUI_NativeWindows_VSResourceEditor](17_ProgrammingGUI_NativeWindows_VSResourceEditor.md) |

### 工具栏与状态栏

| 章节 | 内容 | 文件 |
|:-----|:-----|:-----|
| 17.5 | 工具栏与状态栏 | [17_5_ProgrammingGUI_NativeWindows_ToolbarStatusBar](17_5_ProgrammingGUI_NativeWindows_ToolbarStatusBar.md) |

### GDI 图形篇

| 章节 | 内容 | 文件 |
|:-----|:-----|:-----|
| 18 | GDI 设备上下文（HDC）完全指南 | [18_ProgrammingGUI_NativeWindows_GDI_HDC](18_ProgrammingGUI_NativeWindows_GDI_HDC.md) |
| 19 | GDI 绘图对象：画笔、画刷、字体 | [19_ProgrammingGUI_NativeWindows_GDI_Objects](19_ProgrammingGUI_NativeWindows_GDI_Objects.md) |
| 20 | GDI 图形绘制：线条、矩形与多边形 | [20_ProgrammingGUI_NativeWindows_GDI_Shapes](20_ProgrammingGUI_NativeWindows_GDI_Shapes.md) |
| 21 | GDI 文字渲染 | [21_ProgrammingGUI_NativeWindows_GDI_Text](21_ProgrammingGUI_NativeWindows_GDI_Text.md) |
| 22 | GDI 位图操作 | [22_ProgrammingGUI_NativeWindows_GDI_Bitmap](22_ProgrammingGUI_NativeWindows_GDI_Bitmap.md) |
| 23 | GDI 双缓冲技术 | [23_ProgrammingGUI_NativeWindows_GDI_DoubleBuffer](23_ProgrammingGUI_NativeWindows_GDI_DoubleBuffer.md) |
| 24 | GDI Region 与裁切 | [24_ProgrammingGUI_Graphics_GDI_Region](24_ProgrammingGUI_Graphics_GDI_Region.md) |
| 25 | Alpha 混合与透明效果 | [25_ProgrammingGUI_Graphics_GDI_AlphaBlend](25_ProgrammingGUI_Graphics_GDI_AlphaBlend.md) |

### GDI+ 篇

| 章节 | 内容 | 文件 |
|:-----|:-----|:-----|
| 26 | GDI+ 架构与抗锯齿渐变 | [26_ProgrammingGUI_Graphics_GdiPlus_Architecture](26_ProgrammingGUI_Graphics_GdiPlus_Architecture.md) |
| 27 | 坐标变换与矩阵 | [27_ProgrammingGUI_Graphics_GdiPlus_Transform](27_ProgrammingGUI_Graphics_GdiPlus_Transform.md) |
| 28 | 图像格式与编解码 | [28_ProgrammingGUI_Graphics_GdiPlus_ImageCodec](28_ProgrammingGUI_Graphics_GdiPlus_ImageCodec.md) |

### Direct2D / DirectWrite 篇

| 章节 | 内容 | 文件 |
|:-----|:-----|:-----|
| 29 | Direct2D 架构与资源体系 | [29_ProgrammingGUI_Graphics_Direct2D_Architecture](29_ProgrammingGUI_Graphics_Direct2D_Architecture.md) |
| 30 | Direct2D 几何体系统 | [30_ProgrammingGUI_Graphics_Direct2D_Geometry](30_ProgrammingGUI_Graphics_Direct2D_Geometry.md) |
| 31 | Direct2D 效果与图层 | [31_ProgrammingGUI_Graphics_Direct2D_EffectsLayer](31_ProgrammingGUI_Graphics_Direct2D_EffectsLayer.md) |
| 32 | DirectWrite 高质量文字排版 | [32_ProgrammingGUI_Graphics_DirectWrite_Typography](32_ProgrammingGUI_Graphics_DirectWrite_Typography.md) |
| 33 | Direct2D 与 Win32/GDI 互操作 | [33_ProgrammingGUI_Graphics_Direct2D_GDIInterop](33_ProgrammingGUI_Graphics_Direct2D_GDIInterop.md) |

### HLSL 着色器篇

| 章节 | 内容 | 文件 |
|:-----|:-----|:-----|
| 34 | HLSL 语言基础 | [34_ProgrammingGUI_Graphics_HLSL_Basics](34_ProgrammingGUI_Graphics_HLSL_Basics.md) |
| 35 | HLSL 编译与调试 | [35_ProgrammingGUI_Graphics_HLSL_CompileDebug](35_ProgrammingGUI_Graphics_HLSL_CompileDebug.md) |
| 36 | Constant Buffer 与数据传递 | [36_ProgrammingGUI_Graphics_HLSL_CBuffer](36_ProgrammingGUI_Graphics_HLSL_CBuffer.md) |

### Direct3D 11 篇

| 章节 | 内容 | 文件 |
|:-----|:-----|:-----|
| 37 | D3D11 初始化与 SwapChain | [37_ProgrammingGUI_Graphics_D3D11_InitSwapChain](37_ProgrammingGUI_Graphics_D3D11_InitSwapChain.md) |
| 38 | 顶点缓冲与输入布局 | [38_ProgrammingGUI_Graphics_D3D11_VertexInput](38_ProgrammingGUI_Graphics_D3D11_VertexInput.md) |
| 39 | 纹理与采样器 | [39_ProgrammingGUI_Graphics_D3D11_TextureSampler](39_ProgrammingGUI_Graphics_D3D11_TextureSampler.md) |
| 40 | 深度缓冲与 3D 变换 | [40_ProgrammingGUI_Graphics_D3D11_Depth3D](40_ProgrammingGUI_Graphics_D3D11_Depth3D.md) |
| 41 | 光照模型基础 | [41_ProgrammingGUI_Graphics_D3D11_Lighting](41_ProgrammingGUI_Graphics_D3D11_Lighting.md) |
| 42 | 混合与透明渲染 | [42_ProgrammingGUI_Graphics_D3D11_BlendAlpha](42_ProgrammingGUI_Graphics_D3D11_BlendAlpha.md) |

### Direct3D 12 篇

| 章节 | 内容 | 文件 |
|:-----|:-----|:-----|
| 43 | D3D12 设计哲学 | [43_ProgrammingGUI_Graphics_D3D12_Philosophy](43_ProgrammingGUI_Graphics_D3D12_Philosophy.md) |
| 44 | 命令列表、队列与围栏 | [44_ProgrammingGUI_Graphics_D3D12_CmdQueue](44_ProgrammingGUI_Graphics_D3D12_CmdQueue.md) |
| 45 | 资源与堆管理 | [45_ProgrammingGUI_Graphics_D3D12_ResourceHeap](45_ProgrammingGUI_Graphics_D3D12_ResourceHeap.md) |
| 46 | 描述符堆与根签名 | [46_ProgrammingGUI_Graphics_D3D12_DescRootSig](46_ProgrammingGUI_Graphics_D3D12_DescRootSig.md) |
| 47 | D3D12 与 D3D11 互操作 | [47_ProgrammingGUI_Graphics_D3D12_D3D11Interop](47_ProgrammingGUI_Graphics_D3D12_D3D11Interop.md) |

### 自定义控件篇

| 章节 | 内容 | 文件 |
|:-----|:-----|:-----|
| 48 | Owner-Draw 控件 | [48_ProgrammingGUI_Graphics_CustomCtrl_OwnerDraw](48_ProgrammingGUI_Graphics_CustomCtrl_OwnerDraw.md) |
| 49 | 完全自绘控件架构 | [49_ProgrammingGUI_Graphics_CustomCtrl_FullCustom](49_ProgrammingGUI_Graphics_CustomCtrl_FullCustom.md) |
| 50 | 命中测试与鼠标事件路由 | [50_ProgrammingGUI_Graphics_CustomCtrl_HitTest](50_ProgrammingGUI_Graphics_CustomCtrl_HitTest.md) |

### OpenGL 篇

| 章节 | 内容 | 文件 |
|:-----|:-----|:-----|
| 51 | Win32 嵌入 OpenGL | [51_ProgrammingGUI_Graphics_OpenGL_Win32](51_ProgrammingGUI_Graphics_OpenGL_Win32.md) |

### 高级消息篇

| 章节 | 内容 | 文件 |
|:-----|:-----|:-----|
| 52 | 高级输入消息：触控、Raw Input 与窗口管理 | [52_ProgrammingGUI_NativeWindows_AdvancedMessages](52_ProgrammingGUI_NativeWindows_AdvancedMessages.md) |

### Win32 进阶篇

| 章节 | 内容 | 文件 |
|:-----|:-----|:-----|
| 53 | 子类化与超类化 | [53_ProgrammingGUI_NativeWindows_Subclassing](53_ProgrammingGUI_NativeWindows_Subclassing.md) |
| 54 | Hook 机制 | [54_ProgrammingGUI_NativeWindows_Hook](54_ProgrammingGUI_NativeWindows_Hook.md) |
| 55 | 系统托盘 | [55_ProgrammingGUI_NativeWindows_SystemTray](55_ProgrammingGUI_NativeWindows_SystemTray.md) |
| 56 | 拖放 | [56_ProgrammingGUI_NativeWindows_DragDrop](56_ProgrammingGUI_NativeWindows_DragDrop.md) |
| 57 | 定时器 | [57_ProgrammingGUI_NativeWindows_Timer](57_ProgrammingGUI_NativeWindows_Timer.md) |
| 58 | 加速键表与通用对话框 | [58_ProgrammingGUI_NativeWindows_AcceleratorCommonDialog](58_ProgrammingGUI_NativeWindows_AcceleratorCommonDialog.md) |
| 59 | 消息机制补充：SendMessage、PostMessage 与跨线程通信 | [59_ProgrammingGUI_NativeWindows_MsgMechanism](59_ProgrammingGUI_NativeWindows_MsgMechanism.md) |
| 60 | 常用系统消息补充：滚轮、命中测试、窗口限制与背景擦除 | [60_ProgrammingGUI_NativeWindows_SystemMessages](60_ProgrammingGUI_NativeWindows_SystemMessages.md) |

## 配套示例代码

每个章节都配有完整的可运行示例，位于 [src/tutorial/native_win32/](https://github.com/Charliechen114514/anatomy_gui/tree/main/src/tutorial/native_win32) 目录：

- **基础示例**：01_hello_world ~ 10_oop_wrapper（10 个渐进式示例）
- **练习项目**：6 个实战练习（点击计数器、随机方块、鼠标追踪器、简单记事本、拖动小球、双缓冲）
