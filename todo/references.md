# 参考资料 · 可借鉴教程系列与定位依据

## 一、可借鉴的教程系列

| 系列 | URL | 可借鉴点 / 用在 |
|:---|:---|:---|
| **build-your-own-x** | [github.com/codecrafters-io/build-your-own-x](https://github.com/codecrafters-io/build-your-own-x) | 「造轮子」元聚合器；**无 C++ 保留模式 GUI 条目**——争取被收录是免费曝光。Act 1 MiniUI 对标 |
| **pomber/didact** | [github.com/pomber/didact](https://github.com/pomber/didact) | 「造你自己的 React」逐步拆解（JSX/vDOM/reconciliation）——借鉴「框架解剖」分步格式。Act 1/2 范式 |
| **rxi/microui 实现概览** | [rxi.github.io](https://rxi.github.io/microui_v2_an_implementation_overview.html) | 「实现概览」式深度文——借鉴对**保留模式**做同等深度。Act 2 器官 5 |
| **3DGEP D3D12** | [3dgep.com](https://www.3dgep.com/learning-directx-12-1/) | 最常引英文 D3D12 教程，仅出 3 课停更——Act 3 可填其空白 |
| **X-Jun DirectX11** | [cnblogs.com/X-Jun](https://www.cnblogs.com/X-Jun/p/9028764.html) | 目前最系统的中文 DX11 系统教程——Act 3 定位为「配套」 |
| **Programming with gtkmm 4** | [gnome gtkmm docs](https://gnome.pages.gitlab.gnome.org/gtkmm-documentation/) | gtkmm 官方书——Act 2 specimens/gtk4 深度参考 |
| **ToshioCP/Gtk4-tutorial** | [github.com/ToshioCP/Gtk4-tutorial](https://github.com/ToshioCP/Gtk4-tutorial) | GTK4 入门（C 为主）——借鉴 GTK4 基础结构 |
| **Slint** | [slint.dev](https://slint.dev/) | 声明式对比素材；有 C++ API + KDAB 背书。Act 2 器官 7（响应式对照） |
| **Casey Muratori / Handmade Hero** | [caseymuratori.com](https://caseymuratori.com/blog_0001) | 「从零构建」精神——即时模式游戏 UI、无中文，正是 Act 1 MiniUI 的差异化空间 |

---

## 二、定位依据（营销/选材时记住）

**真稀缺（可作卖点）**
- 中英文均**无**「从零造保留模式 GUI 框架」的系统教程 → Act 1 MiniUI 是独占
- **同时造保留模式（MiniUI）+ 即时模式（ImGui）并亲手对比** → 全球独占 niche，落在 Act 2 器官 5

**被高估（不要当卖点）**
- 中文「信号槽从零实现」**已饱和**（debao.me 121 行实现 + 大量 CSDN/掘金/知乎）→ 器官 7 信号章定位为「整合复习」，不当创新点
- 「首个中文 C++ WebView2」**已部分被占**（知乎 liulun 已有 5 部分系列）→ 混合方案章定位为「系统化+工程级深度」
- MiniUI→WinUI3 映射与 MS Learn zh-cn 高度重叠 → 定位为「工业级对照」而非卖点

**2026 时效（选材依据）**
- WinUI 3 处于峰值相关性：Build 2026（2026-06）宣布为 Windows 原生生产平台，Shell 用 WinUI 重写；WinApp SDK 1.8 稳定。但社区反映有性能问题、Windows-only → Act 2 specimens/winui3 值得写，COM 作为器官 6 的标本
- Wayland 已是现代 Linux 主场，XCB/X11 属遗留 → Act 1 MiniUI 须补 Wayland 说明
- Slint（声明式、C++ API、KDAB 背书）值得在器官 7 作对比，但不纳入主路径
