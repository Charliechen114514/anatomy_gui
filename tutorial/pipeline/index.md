---
title: 渲染管线
---

# 渲染管线 · 从像素到合成器

本线讲的是画面本身的来路。主线那套从零造骨架的教程，带咱们亲手造过窗口、控件树、事件与布局，绘制却始终躺在系统的怀里，咱们一直没有动它。控件为什么会有自己的像素、文字为什么难画得多、窗口的画面最后怎么拼到屏幕上，咱们这些疑问都排在后面的篇目里，一篇一篇地看清楚。

读者设定是对渲染几乎零基础的开发者，顺序也就从最外圈起步：咱们开头看清一次界面更新从头到尾走过的站，接着咱们依次解剖 GDI、Direct2D、Skia、文本与 Direct3D，最后一站落到了 DWM 合成器。每篇咱们只讲一个概念，API 咱们讲到您能对照写出真代码的程度，讲不清的地方宁可明说，咱们不糊弄。

## 第一段 · 全景

- [01 · 从状态到像素：一次界面更新的全程](01_status_to_pixels.md)
- [02 · 屏幕、帧与垂直同步](02_screen_frame_vsync.md)
- [03 · 坐标、DIP 与 DPI](03_dip_and_dpi.md)

## 第二段 · GDI 标本

- [04 · HDC 与 GDI 对象模型](04_gdi_objects.md)
- [05 · 失效、脏区与重绘](05_invalidation_dirty_region.md)

## 第三段 · Direct2D

- [06 · 工厂、设备与渲染目标](06_d2d_targets.md)
- [07 · 画刷：纯色、渐变、位图](07_d2d_brushes.md)
- [08 · 几何与路径](08_d2d_geometry.md)
- [09 · 坐标变换](09_d2d_transforms.md)
- [10 · 位图与离屏渲染](10_d2d_bitmaps.md)
- [11 · 特效：Effect 图](11_d2d_effects.md)
- [12 · 与 GDI 互操作](12_d2d_interop.md)

## 第四段 · Skia

- [13 · Surface、Canvas、Paint 三件套](13_skia_surface_canvas_paint.md)
- [14 · SkPaint 深入：shader 与混合](14_skia_paint_deep.md)
- [15 · Skia 的文本：SkFont 与 TextBlob](15_skia_text.md)
- [16 · GPU 后端：DirectContext 与后端目标](16_skia_gpu_backend.md)
- [17 · SkSL：运行时着色器](17_sksl.md)
- [18 · 实战：Win32 窗口跑 Skia 渲染循环](18_skia_win32_loop.md)

## 第五段 · 文本

- [19 · 字符、码点与字形](19_chars_and_glyphs.md)
- [20 · 整形：连字、kerning、BiDi](20_shaping.md)
- [21 · DirectWrite：TextLayout 全解](21_directwrite_layout.md)
- [22 · 测量、命中测试与光标](22_text_metrics.md)

## 第六段 · Direct3D

- [23 · GPU 管线：从顶点到像素的各阶段](23_gpu_pipeline.md)
- [24 · D3D11 设备与立即上下文](24_d3d11_device.md)
- [25 · 资源与视图](25_d3d11_resources.md)
- [26 · 着色器与常量缓冲](26_d3d11_shaders.md)
- [27 · 交换链与呈现（DXGI）](27_dxgi_swapchain.md)
- [28 · D3D12 对照：显式化的代价与收益](28_d3d12_contrast.md)
- [29 · 图像：解码、缓存与 mipmap](29_image_pipeline.md)

## 第七段 · 合成与系统

- [30 · DWM：重定向表面与窗口合成](30_dwm_composition.md)
- [31 · DirectComposition：visual 树](31_directcomposition.md)
- [32 · 图层与裁剪](32_layers_clipping.md)
- [33 · 透明、阴影、模糊](33_transparency_effects.md)
- [34 · 动画与帧调度](34_animation_frames.md)
- [35 · 架构对照：场景图、显示列表、即时模式](35_render_architectures.md)
- [36 · 性能观测：PIX、RenderDoc、ETW](36_performance_tools.md)
