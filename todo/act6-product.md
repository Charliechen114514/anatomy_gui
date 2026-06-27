# 草稿 06 · 产品化、人因与交付

> 目标：把“能跑 demo”推进到“像真实桌面软件”。GUI 教程如果不讲输入法、可访问性、DPI、国际化、测试、打包、性能和设计系统，读者最终仍会卡在产品化阶段。

---

## 一、为什么这一板块必须主线化

很多 GUI 教程止步于：

- 创建窗口。
- 放几个控件。
- 响应点击。
- 绘制图形。

但真实软件会遇到：

- 中文输入法不工作。
- 高 DPI 模糊或布局截断。
- 多显示器拖动后尺寸错乱。
- 屏幕阅读器读不到自绘控件。
- 深色模式下文字看不清。
- 列表数据一多就卡。
- 打包后干净机器不能运行。
- 自动化测试无法稳定定位控件。
- 崩溃现场没有日志。

这些不是边角料，而是 GUI 框架和应用的“成人礼”。

---

## 二、建议章节族

### P01 · GUI 的产品化最低线

定义一个真实桌面应用的最低验收：

- Unicode 路径。
- 中文输入。
- 高 DPI。
- 深色 / 高对比度。
- 键盘可用。
- 可访问性基本语义。
- 异步不冻结。
- 安装包或免安装交付。
- 崩溃和日志。

输出：

- 全书统一 checklist。

### P02 · 输入法、键盘与文本编辑

覆盖：

- 文本输入不是 `WM_KEYDOWN`。
- IME composition。
- shortcut 与文本输入冲突。
- caret、selection、clipboard。
- undo / redo。
- text command。

Demo：

- 一个可中文输入、复制粘贴、撤销重做的 TextBox。

### P03 · 焦点、键盘导航与命令系统

覆盖：

- tab order。
- focus visual。
- default button。
- accelerator。
- menu command。
- command routing。
- disabled state。

Demo：

- 全键盘可操作的设置窗口。

### P04 · 可访问性与语义树

覆盖：

- role / name / value / state。
- UI Automation。
- AT-SPI / ARIA 旁证。
- 自绘控件的 a11y provider。
- high contrast。
- keyboard-only。

Demo：

- Inspect.exe 能正确识别自绘按钮和 slider。

### P05 · DPI、多显示器与窗口管理

覆盖：

- Per-Monitor DPI。
- `WM_DPICHANGED`。
- 多显示器坐标。
- snap / maximize / restore。
- 保存窗口位置。
- 图标和位图资源缩放。

Demo：

- 拖到不同 DPI 显示器不会糊、不跳、不截断。

### P06 · 国际化、本地化与区域格式

覆盖：

- 文案外置。
- plural。
- 日期、数字、货币。
- RTL。
- 字体 fallback。
- 翻译长度导致布局变化。

Demo：

- 中英双语切换，布局不崩。

### P07 · 主题、设计系统与视觉一致性

覆盖：

- design token。
- dark mode。
- high contrast。
- 系统色。
- spacing / typography。
- icon 体系。
- 控件状态矩阵。

Demo：

- 一套 token 驱动整个设置窗口。

### P08 · 性能：让 UI 不只是不卡

覆盖：

- startup time。
- idle time。
- message storm。
- paint count。
- list virtualization。
- async loading。
- memory allocation。
- frame time。

Demo：

- 100k 行虚拟列表。

### P09 · GUI 自动化测试

覆盖：

- 控件 id / automation id。
- UIA 自动化。
- screenshot golden test。
- input replay。
- layout snapshot。
- accessibility test。
- flaky test 的来源。

Demo：

- 自动打开窗口、点击按钮、输入中文、检查结果。

### P10 · 日志、崩溃与诊断

覆盖：

- structured logging。
- crash dump。
- last session restore。
- diagnostic bundle。
- ETW。
- 用户可复制的错误报告。

Demo：

- 人为崩溃后生成 dump 和日志包。

### P11 · 打包、安装、更新

覆盖：

- portable zip。
- Inno / WiX。
- MSIX。
- code signing。
- auto update。
- VC runtime。
- WebView2 runtime。
- first run experience。

Demo：

- 干净 Windows 虚拟机可安装运行。

### P12 · 安全与权限

覆盖：

- 文件选择权限边界。
- WebView2 安全。
- 本地文件与脚本通信。
- 拖放文件信任。
- updater 安全。
- least privilege。

Demo：

- 安全的 WebView2 Markdown 预览器。

### P13 · 设计系统不是美工，是约束系统

覆盖：

- 信息架构。
- navigation。
- toolbar / menu / status bar。
- command palette。
- empty state。
- error state。
- progressive disclosure。

Demo：

- 把一个“功能堆叠窗口”重构成可扫描的信息界面。

---

## 三、可作为全书统一验收的 checklist

每个中大型 demo 至少检查：

- [ ] 窗口可用键盘关闭。
- [ ] 主要操作有快捷键或菜单。
- [ ] 中文输入正常。
- [ ] 中文路径正常。
- [ ] DPI 变化后布局正常。
- [ ] 深色 / 浅色主题可切。
- [ ] 高对比度不丢信息。
- [ ] UI 不因文件 IO 冻结。
- [ ] 长列表使用虚拟化或增量加载。
- [ ] 自绘控件有基本可访问性语义。
- [ ] 关键流程可自动化测试。
- [ ] 打包后可在干净环境运行。

---

## 四、与其他板块的连接

| 产品化主题 | 回链 |
|:---|:---|
| IME | 理论 T07、现代 Win32 W20-10、Anatomy GUI Stage 8 |
| DPI | 理论 T09、现代 Win32 W20-08、渲染 R01 |
| 可访问性 | 理论 T08、比较 C12、Anatomy GUI Stage 13 |
| 性能 | 渲染 R15、异步 C11、虚拟列表 capstone |
| 设计系统 | 理论 T09、渲染 R13、Anatomy GUI Stage 10 |
| 打包 | 现代 Win32 WebView2、毕业项目 |

---

## 五、建议的“真实软件能力”示例

- Settings App：主题、DPI、键盘导航、a11y。
- Log Viewer：大文件异步读取、虚拟列表、搜索。
- Image Browser：异步解码、缩略图缓存、DPI。
- Markdown Editor：IME、WebView2、异步保存、打包。
- UI Inspector：控件树、布局框、事件日志、性能观测。

这些例子应复用，不要每章重新发明不同 demo。
