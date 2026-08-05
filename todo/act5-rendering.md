# 草稿 05 · 渲染与视觉系统

> 目标：把现有 graphics 章节从“图形 API 学习路线”扩展成“GUI 视觉系统解剖”。GUI 渲染不只是 GDI、D2D、D3D，还包括文本、颜色、DPI、合成、动画、图像资源、dirty region、scene graph、custom control 和设计系统。

---

## 一、为什么要扩展渲染板块

现有图形章节覆盖面已经很强：

- GDI。
- GDI+。
- Direct2D。
- DirectWrite。
- HLSL。
- Direct3D 11 / 12。
- OpenGL。
- 自绘控件。

但 GUI 读者还需要知道：

- 框架为什么要区分 layout、paint、compose？
- 为什么文本渲染比画矩形复杂得多？
- 高 DPI 下图像、字体、坐标如何统一？
- GPU 加速 UI 和 3D 渲染管线有什么关系？
- 脏区、缓存、layer、clip、transform 如何影响性能？
- 现代框架为什么常用 scene graph 或 display list？
- 动画、阴影、透明、模糊为什么牵涉合成器？

建议把 Act 5 改成“渲染与视觉系统”，而不是单纯“graphics API”。

---

## 二、建议内容分层

### 第一层：GUI 绘制基本模型

- invalidation。
- paint callback。
- dirty region。
- retained display list。
- immediate draw list。
- double buffering。
- vsync / frame scheduling。

### 第二层：2D 渲染 API

- GDI。
- GDI+。
- Cairo。
- Direct2D。
- Skia。
- CoreGraphics 旁证。

### 第三层：文本系统

- glyph、font、font fallback。
- shaping。
- kerning、ligature。
- BiDi。
- emoji。
- DirectWrite、Pango、HarfBuzz。
- caret、selection、hit test。

### 第四层：GPU 与合成

- swap chain。
- compositor。
- layer。
- transform。
- opacity。
- clip。
- effect。
- DWM。
- DirectComposition。
- D3D11 / D3D12 / Vulkan 概念对照。

### 第五层：视觉产品化

- design token。
- theme。
- icon。
- bitmap resource。
- color management。
- high contrast。
- animation。
- screenshot / printing。

---

## 三、建议章节草案

### R00 · GUI 渲染不是“画图 API”

- 从 “控件状态改变” 到 “屏幕像素变化” 的完整路径。
- layout、paint、compose 的分工。
- CPU raster、GPU raster、compositor 的关系。

### R01 · 坐标、单位与 DPI

- 物理像素。
- 逻辑像素。
- DIP。
- transform。
- 多显示器。
- 资源缩放。

Demo：

- 一个窗口显示所有坐标转换和 DPI 变化。

### R02 · Invalidate、Dirty Region 与 Paint

- 为什么不每次都全窗口重画。
- region 合并。
- repaint storm。
- occlusion。
- resize 时的重绘。

Demo：

- 脏区可视化。

### R03 · GDI：传统 GUI 绘制的透明标本

- HDC。
- pen / brush / font / bitmap。
- selected object。
- clipping。
- double buffer。
- 常见泄漏和闪烁。

定位：

- 作为传统理解，不作为现代主力。

### R04 · Cairo / Skia / Direct2D：现代 2D 抽象

- path。
- stroke / fill。
- transform。
- antialias。
- bitmap。
- gradient。
- text integration。

比较：

- Cairo 更适合解释跨平台 CPU raster。
- Direct2D 贴近 Windows 现代桌面。
- Skia 贴近 Chrome / Flutter / Electron 等现代栈。

### R05 · DirectWrite / Pango / HarfBuzz：文本不是字符串

- 字符、码点、grapheme cluster。
- shaping。
- font fallback。
- BiDi。
- hit test。
- line break。

Demo：

- 混合中文、英文、emoji、阿拉伯文的文本布局实验。

### R06 · Image Pipeline：图像加载、解码、缓存与缩放

- WIC。
- PNG/JPEG/WebP。
- ICC profile。
- mipmap / multi-scale assets。
- async decode。
- cache。

Demo：

- 异步图像浏览器。

### R07 · Layer、Clip、Transform 与 Effect

- 2D UI 为什么需要 layer。
- opacity、clip、rounded corner、shadow、blur。
- 离屏渲染代价。
- GPU 合成与 CPU 绘制的边界。

Demo：

- 含动画阴影和裁剪的卡片控件。

### R08 · Animation 与 Frame Scheduler

- timer 不是动画系统。
- vsync。
- easing。
- layout animation vs render transform animation。
- requestAnimationFrame 旁证。

Demo：

- 展开/折叠面板，比较 layout animation 与 transform animation。

### R09 · Scene Graph、Display List 与 Draw List

- retained scene graph。
- display list。
- ImGui draw list。
- Flutter layer tree。
- Qt scene graph。

Demo：

- 用同一批 draw command 输出到 Direct2D 和 SVG。

### R10 · DirectComposition 与 DWM

- Windows compositor。
- composition visual。
- swap chain。
- Mica / Acrylic / transparency 的边界。
- 何时需要 DirectComposition。

Demo：

- D2D 内容 + composition layer。

### R11 · D3D11 / D3D12 对 GUI 的意义

- GUI 不等于 3D，但现代 UI 经常需要 GPU。
- D3D11 对 UI 足够友好。
- D3D12 更贴近显式资源管理，教学价值高但成本高。
- Vulkan / Metal / WebGPU 概念对照。

Demo：

- D2D UI 上嵌入一个 D3D viewport。

### R12 · Custom Control Rendering

- owner draw。
- full custom。
- hit test 与 painting 的对应。
- state rendering。
- accessibility 与 custom draw 的关系。

Demo：

- 自绘 slider 或 timeline 控件。

### R13 · Design Token 与视觉一致性

- color token。
- spacing token。
- typography token。
- icon size。
- state layer。
- high contrast。

Demo：

- 同一组 token 驱动按钮、文本框、列表。

### R14 · Screenshot、Printing 与 Export

- 屏幕绘制不等于打印。
- vector export。
- PDF / bitmap export。
- GDI printing 作为历史接口。

Demo：

- 将一个控件树导出成 PNG / PDF 草案。

### R15 · 渲染性能观测

- paint count。
- frame time。
- overdraw。
- cache hit。
- allocation。
- GPU capture。

工具：

- PIX。
- RenderDoc。
- ETW / WPA。
- 自制 paint profiler。

---

## 四、与 Anatomy GUI v2 的闭环

Anatomy GUI v2 里至少需要这些渲染实验：

- Painter 抽象。
- Direct2D 后端。
- Cairo 后端可选。
- Dirty region overlay。
- Layout bounds overlay。
- Text layout。
- Theme token。
- Basic animation。
- Render profiler。

这样渲染板块就不是孤立 API 教程，而是框架器官的一部分。

---

## 五、与旧 graphics 章节的关系

| 旧内容 | 新定位 |
|:---|:---|
| GDI HDC / Objects / Shapes | 传统 paint 模型标本 |
| GDI Double Buffer / Region / AlphaBlend | dirty、buffer、region 的基础 |
| GDI+ | 现代 2D API 的过渡标本 |
| Direct2D / DirectWrite | Anatomy GUI v2 主渲染线 |
| HLSL / D3D11 / D3D12 | GPU 概念与嵌入式 viewport |
| OpenGL Win32 | 旁证和 legacy interop |
| Custom Control | 回到控件树、输入、a11y、渲染综合 |

---

## 六、必须补的视觉领域

- 字体 fallback 与中文排版。
- Emoji 和 grapheme cluster。
- Color management。
- High contrast。
- 资源缩放策略。
- 动画调度。
- Scene graph 与 display list。
- GPU/CPU 边界。
- 渲染性能工具。
- 截图、打印、导出。
