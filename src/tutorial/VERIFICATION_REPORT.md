# GUI 教程现象验证报告

> 生成时间: 2026-05-04
> 构建工具: MinGW (g++ / cmake)
> 构建脚本: `build_mingw.ps1`

---

## 第一部分：Win32 基础教程 (01-10)

| # | 教程名 | 预期现象 |
|---|--------|---------|
| 01 | Hello World | 窗口居中显示 "Hello, World!"，标准 Win32 窗口，可正常关闭 |
| 02 | Register Window Class | 窗口显示 "窗口类注册成功！这个窗口使用了自定义的窗口类。" |
| 03 | Create Window | 同时创建 6 个不同风格的窗口（标准、弹出、工具窗口、细边框、帮助按钮、RTL），每个窗口标注其风格名 |
| 04 | Message Loop | 窗口显示 "消息循环示例 / 点击鼠标或按键查看消息"；点击/按键在控制台/DebugView 输出调试信息 |
| 05 | Window Procedure | 窗口底部显示鼠标点击坐标、按键虚拟码、窗口尺寸信息 |
| 06 | Paint | 窗口内绘制静态图形（矩形、椭圆、三角形、线条）+ 动画（弹跳方块、旋转方块），60FPS |
| 07 | Close Window | 点击关闭按钮弹出确认对话框 "确定要关闭窗口吗？"，选"否"不关闭 |
| 08 | Global State | 左键点击递增计数 "Click Count: N"，右键重置为 0 |
| 09 | Instance Data | 创建多个窗口，每个窗口独立计数并独立变色（使用 GWLP_USERDATA） |
| 10 | OOP Wrapper | 显示 "Hello, OOP World!" 和 "Using BaseWindow template class"，点击计数 |

---

## 第二部分：练习项目 (exercises/01-06)

| # | 教程名 | 预期现象 |
|---|--------|---------|
| E01 | Click Counter | 大号数字居中显示，左键 +1，右键归零，顶部提示操作说明 |
| E02 | Random Rects | 白色画布，左键点击生成随机位置/大小/颜色的矩形，累积显示 |
| E03 | Mouse Tracker | 标题栏实时显示鼠标坐标；客户区显示坐标文本和左键状态 |
| E04 | Simple Notepad | 简易文本编辑器，支持输入/退格/Delete/回车/方向键/Home/End，底部状态栏显示行列号 |
| E05 | Draggable Ball | 红色小球，鼠标悬停变手形光标，可拖动；显示拖动/等待状态和坐标 |
| E06 | Double Buffer | 15 个彩色弹跳球，60FPS 动画；按 SPACE 切换双缓冲/单缓冲模式对比闪烁 |

---

## 第三部分：扩展 Win32 教程 (11-25)

| # | 教程名 | 预期现象 |
|---|--------|---------|
| 11 | DPI Aware | 显示当前 DPI 值和缩放比例，拖到不同 DPI 显示器窗口自动调整 |
| 12 | Basic Controls | 按钮（点击计数）、复选框、单选按钮(A/B/C)、ListBox、ComboBox，右侧事件日志 |
| 13 | WM_NOTIFY | 左侧 ListBox，单击触发 WM_COMMAND，双击触发 WM_NOTIFY，事件日志区分两者 |
| 14 | ListView | 表格显示文件数据（名称/类型/大小/日期），支持选中/双击/添加/删除，网格线+整行选择 |
| 15 | TreeView | 树形控件显示目录结构，支持展开/折叠/选中/添加节点/全部展开 |
| 16 | More Controls | 进度条、滑动条、微调器(UpDown 0-100)、选项卡(3页)，自动进度动画 |
| 17 | Modal Dialog | 主窗口 "打开关于对话框" 按钮 → 弹出模态对话框（阻塞主窗口），显示应用信息 |
| 18 | Modeless Dialog | 主窗口绘图区域 + "显示调色板" 按钮 → 无模态调色板，选色实时影响主窗口 |
| 19 | DialogProc | "编辑用户信息" 按钮 → 对话框含姓名/年龄(1-150校验)/性别，实时预览，确认后回传主窗口 |
| 20 | Resource Files | 文本全部来自 STRINGTABLE 资源，菜单栏含"关于"和"退出" |
| 21 | Menu Resource | 简易文本编辑器，菜单：文件(新建/打开/保存/另存/退出)、编辑(撤销/剪切/复制/粘贴)、帮助 |
| 22 | Icon/Cursor/Bitmap | ComboBox 选光标类型，光标进入窗口时变化；"绘制位图"按钮生成渐变位图并显示 |
| 23 | String Table | 所有文本来自字符串表资源，按钮点击显示格式化消息（点击计数），关闭时确认对话框 |
| 24 | Dialog Template | 对话框模板定义在 .rc 文件，主窗口按钮打开关于对话框，OK/Cancel 结果回传 |
| 25 | Toolbar/StatusBar | 工具栏(新建/打开/保存/剪切/复制/粘贴) + 3段状态栏，悬停显示提示文本 |

---

## 第四部分：GDI 图形系列 (ch18-25)

| # | 教程名 | 预期现象 |
|---|--------|---------|
| G01 | HDC Basics | 4象限：BeginPaint / GetDC点击画十字 / GetWindowDC在标题栏画红框 / CreateCompatibleDC内存离屏渐变 |
| G02 | GDI Objects | 4象限：画笔样式(实线/虚线/点线)、画刷(纯色+5种阴影)、自定义字体、SaveDC/RestoreDC；底部GDI对象泄漏计数 |
| G03 | Shapes | 3x2网格：五角星、圆角矩形、韦恩图、五边形+三角形、弧/饼/弦、正弦波 |
| G04 | Text Rendering | 3x2网格：TextOut示例、DrawText对齐/换行、前景/背景色模式、文本对齐参考点、GetTextMetrics、LOGFONT自定义字体 |
| G05 | Bitmap Operations | 3面板：BitBlt复制3x2彩色矩形、StretchBlt缩放、DIB像素渐变+PatBlt棋盘格 |
| G06 | Double Buffer | 复选框切换双缓冲/直接绘制，5x4彩色方块50ms动画，对比闪烁效果 |
| G07 | Region Clipping | 4象限：矩形/椭圆/多边形裁切 + CombineRgn(AND/OR/XOR/DIFF)；底部3个可点击区域测试PtInRegion |
| G08 | Alpha Blend | 上方：3个半透明重叠矩形(红/绿/蓝) + 滑块控制透明度；中间：逐像素Alpha紫色渐变条；底部TrackBar |

---

## 第五部分：GDI+ 系列 (ch26-28)

| # | 教程名 | 预期现象 |
|---|--------|---------|
| GP01 | GDI+ Hello | 左右对比：左侧GDI锯齿图形 vs 右侧GDI+抗锯齿图形（矩形/圆/弧/文字/星形） |
| GP02 | Transform Matrix | 房子图形 + 按钮(旋转+30°/缩放x1.5/重置)，显示原始位置虚影，Prepend/Append模式切换 |
| GP03 | Image Codec | 内存创建100x100渐变位图 → "保存PNG"按钮 → "加载PNG"按钮显示；右侧列出系统图像编解码器 |

---

## 第六部分：Direct2D / DirectWrite 系列 (ch29-33)

| # | 教程名 | 预期现象 |
|---|--------|---------|
| D2D01 | D2D Hello | 4面板动画：填充矩形、重叠椭圆、旋转射线(30fps)、嵌套矩形 |
| D2D02 | Geometry | 6面板：矩形/椭圆/圆角矩形/自定义星形路径/描边样式(实线/虚线/点线)/填充对比 |
| D2D03 | Effects & Layers | 3面板深色背景：不透明度遮罩(3圆渐隐)、几何遮罩(彩虹条纹裁切为椭圆)、离屏缓存 |
| D2D04 | DirectWrite | 多色富文本排版：标题28pt微软雅黑、彩色段落(DirectWrite红/蓝色/TrueType绿/OpenType橙)、代码块语法高亮 |
| D2D05 | GDI Interop | 左右对比：GDI锯齿渲染 vs D2D抗锯齿渲染；底部GDI+D2D混合渲染 |

---

## 第七部分：HLSL 着色器系列 (ch34-36)

| # | 教程名 | 预期现象 |
|---|--------|---------|
| H01 | HLSL Hello | 彩色三角形（红/绿/蓝顶点），GPU插值渐变 |
| H02 | Compile Debug | 同彩色三角形 + 编译错误处理演示 + 着色器反射元数据 + 反汇编输出 |
| H03 | Cbuffer | 全屏四边形，颜色随时间动画变化(sin函数)；按SPACE切换 Map/Unmap 和 UpdateSubresource 更新方式 |

---

## 第八部分：Direct3D 11 系列 (ch37-42)

| # | 教程名 | 预期现象 |
|---|--------|---------|
| D3D01 | Init SwapChain | 屏幕颜色持续平滑变化(sin/cos)，无几何体 |
| D3D02 | Vertex Input | 彩色三角形（红/绿/蓝顶点渐变） |
| D3D03 | Texture Sampler | 棋盘格纹理四边形(橙/米色)，纹理重复4x4次 |
| D3D04 | Depth 3D | 旋转3D立方体，每面不同颜色，透视投影+深度缓冲 |
| D3D05 | Lighting | 旋转立方体 + 方向光照，面亮度随旋转变化 |
| D3D06 | Blend Alpha | 3个半透明重叠四边形(红/绿/蓝)，Alpha值脉冲呼吸效果(0.2~1.0) |

---

## 第九部分：Direct3D 12 系列 (ch43-47)

| # | 教程名 | 预期现象 |
|---|--------|---------|
| D12_01 | Cmd Queue | 渐变背景颜色持续平滑变化 |
| D12_02 | Resource Heap | 彩色三角形（红/绿/蓝顶点） |
| D12_03 | Root Signature | 彩色旋转三角形（红/绿/蓝顶点绕Z轴旋转） |
| D12_04 | D3D11 Interop | D3D12 渐变背景 + D2D 白色文字叠加 "D3D12 + D2D 互操作" |

---

## 第十部分：自定义控件系列 (ch48-50)

| # | 教程名 | 预期现象 |
|---|--------|---------|
| CC01 | Owner Draw | 4个自绘按钮(红/绿渐变/蓝圆角/警告黄) + 底部点击状态，悬停/按下效果 |
| CC02 | Full Custom | HSL色轮选择器（36色环），点击/拖动选色，右侧实时预览+RGB/HSL数值 |
| CC03 | Hit Test | 自定义标题栏+最小化/最大化/关闭按钮，悬停高亮，右侧命中测试信息面板 |

---

## 第十一部分：OpenGL 系列 (ch51)

| # | 教程名 | 预期现象 |
|---|--------|---------|
| GL01 | OpenGL Win32 | 深蓝灰背景，彩色旋转三角形（红/绿/蓝顶点，~90°/秒） |

---

## 构建与运行说明

```powershell
# 构建全部
pwsh src\tutorial\build_mingw.ps1 all

# 仅构建 native_win32
pwsh src\tutorial\build_mingw.ps1 native_win32

# 仅构建 graphics
pwsh src\tutorial\build_mingw.ps1 graphics
```

可执行文件输出位置：
- `src/tutorial/native_win32/build_mingw/bin/*.exe`
- `src/tutorial/graphics/build_mingw/bin/*.exe`

**注意事项：**
- D3D12 教程需要 Windows 10+ 且显卡支持 D3D12
- Direct2D/DirectWrite 教程需要 Windows 7+ 平台更新
- OpenGL 教程需要显卡支持 OpenGL 3.3+
- 20-25 资源文件教程需要 windres 编译 .rc 文件（MinGW 自动处理）
