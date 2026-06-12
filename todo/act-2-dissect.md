# Act 2 · 比较解剖 · 执行计划（系列核心）

> 幕旨：拿 Act 1 拼好的骨架，去解剖 GTK / WinUI3 / ImGui 三个工业标本。结构创新点是**按器官组织**——同一器官，多标本对照，而非「按框架分篇」。这是 anatomy_gui 的独占内容。
> 全新写作，是整条路线的心脏。

---

## A. `organs/` · 八个器官比较解剖章

每章统一结构：① MiniUI 骨架回顾（链 Act 1 对应章）→ ② Win32 标本（链 Act 1）→ ③ GTK 解剖 → ④ WinUI3 解剖 → ⑤ ImGui 解剖 → ⑥ 横向对照表 + 设计哲学点评 → ⑦ 可运行对照 Demo。

| # | 器官 | 骨架回顾 | 解剖要点（四标本） | Demo |
|:---:|:---|:---|:---|:---|
| 1 | **窗口与显示面** | ch62 XCB | HWND 生命周期 / GtkWindow+Snapshot / ContentControl×visual tree / ImGui viewport+draw | 四框架各一个最小窗口并排 |
| 2 | **事件循环与输入** | ch63 | GetMessage 阻塞 / glib main loop / DispatcherQueue / 每帧 poll | 同一交互四框架事件链路图 |
| 3 | **控件树与所有权** | ch65 | HWND 父子树 / GtkWidget 树 / DependencyObject 树 / 即时模式「树」的幻象 | 控件树 dump 工具 |
| 4 | **布局** | ch66 Concepts | MoveWindow 绝对 / LayoutManager 盒 / MeasureOverride+ArrangeOverride / 手动坐标 | 同一表单四框架布局代码并排 |
| 5 | **绘制与脏区**（保留 vs 即时正面交锋） | ch68 | HDC+InvalidateRect / Snapshot / Composition / draw list 无脏区 | 脏区可视化 + FPS 对比 |
| 6 | **对象模型与类型系统** | ch65/67 | 句柄+WNDPROC / **GObject** / **COM·WinRT** / **无** | GObject vs COM 最小对象 + ImGui 反例 |
| 7 | **信号 / 响应式 / 绑定** | ch67 Property | WM_COMMAND / g_signal / DependencyProperty+{x:Bind} / 返回值 | 同一计数器四框架响应式写法 |
| 8 | **异步与并发** | ch68/71 | PostMessage / g_idle_add / IAsyncOperation+co_await / 同步 | 异步加载文件四框架写法 |

> **COM 落点**：器官 6。把 COM 基础（IUnknown/引用计数/com_ptr vs wil/apartment）作为「WinUI3 标本的对象模型」来讲，与 GObject、ImGui 的「无」正面对照——这是 COM 最该出现的位置，也是 Act 1→Act 2 大门章映射的落地。
> 器官 5 是全系列高潮：**保留模式（MiniUI/Win32/GTK/WinUI3）vs 即时模式（ImGui）首次正面交锋并亲手对比**——已验证的全球独占 niche。

---

## B. `specimens/` · 标本实操（精简，只讲工具链/idiom）

器官章讲「内部原理」，实操章讲「上手干活」。每框架一篇，不重复器官内容：

- **`gtk4/`**：Meson/pkg-config/MSYS2 构建、GtkApplication 工程结构、CSS 主题、GTK Inspector、gtkmm C++ 绑定 + CMake。**GTK4-only**（GTK3 差异进 `reference/`）。阶段项目：系统信息面板。
- **`winui3/`**：Windows App SDK 工程结构、C++/WinRT、XAML+{x:Bind}、MSIX 打包。接 Act 1 大门章的映射。

> 拆分映射：**COM 并入器官 6、WinUI3 并入 specimens/winui3、WebView2 并入 specimens/webview2**——按主题组织，勿照搬旧章号。
- **`webview2/`**（← 旧 Phase 5 的 WebView2 落点）：原生窗口嵌入 WebView2——架构、初始化、C++↔JS 双向通信（`ExecuteScript` / `postMessage`）、Evergreen vs Fixed、虚拟主机映射、CDP。**强依赖 COM（器官 6）**，是 Act 5「Win32+WebView2」毕业版的基石；章首给选型概览（原生 vs Web、WebView2 vs CEF vs Electron/Tauri）。
- **`imgui/`**：Dear ImGui 集成（D3D11/OpenGL 后端）、Docking、主题、工具开发实战。

---

## C. 砍 / 合并（落实定位收窄）

- ❌ GTK3 双线主体 → `reference/gtk3-diff.md`
- ❌ CEF 独立 2 章 → 合并进器官 5（绘制：嵌入 web 视为一种绘制源）+ 器官 6（CEF 进程模型 vs WebView2 COM）的对照；不做独立教学
- ❌ Vulkan 独立 2 章 → `act3-render/vulkan-vs-d3d12.md` 单章对比

---

## D. 写作依赖与节奏
- **强依赖 Act 1**：器官章的「骨架回顾」直接链 Act 1 miniui 章节，须 Act 1 先就位。
- 器官 1–8 有内在顺序（窗口→事件→树→布局→绘制→对象模型→信号→异步），建议按序写。
- `specimens/` 可与 `organs/` 并行（不同作者/会话可各认领一框架）。

---

## E. 产出验收
- [ ] 8 个器官章，每章含四标本解剖 + 横向对照表 + 可运行 Demo
- [ ] specimens/ 三框架实操章（GTK4/WinUI3/ImGui）
- [ ] 器官 5 的保留 vs 即时对比 Demo 上线（系列旗舰演示）
- [ ] 每章回链 Act 1 对应骨架章，形成闭环
