# Act 1 · 标本与骨架 · 执行计划

> 幕旨：建立解剖词汇。Win32 是「最透明的标本」（每条消息/HWND/绘制可见），MiniUI 是「亲手拼的骨架」。两条线互为参照，以「大门」章（MiniUI→工业映射）收束，通往 Act 2。
> 资产已有，本幕以**迁移 + 深化 + 修复与复查**为主。

---

## A. `win32/`（标本）——解剖最透明的青蛙

### 迁移（现有 → 本幕）
- 基础（hello window / register / create / msgloop / paint / close）：`act1-specimen/win32/basics/`
- 控件（basic controls / ListView / TreeView / MoreControls / WM_NOTIFY）：`controls/`
- 对话框（modal / modeless / dialog proc）：`dialogs/`
- 资源（resource files / menu / icon-cursor-bitmap / string table / dialog template / VS editor）：`resources/`
- 进阶（advanced messages / subclassing / hook / system tray / drag-drop / timer / accelerator / msg mechanism / system messages）：`advanced/`（**前移**——复查 P2 指出这些纯 Win32 章在 GPU 渲染后突然回炉，新结构天然修正）

### 写作视角（关键）
每章以「**Win32 如何实现某个器官**」重写开篇导言，把现有内容锚定到解剖词汇（窗口/事件循环/控件树/绘制/信号=WM_COMMAND…）。**不重写正文**，只补「器官视角」的导言段，让 Act 2 解剖时能回链。

### 新增（补复查 P2 缺失的现代 Win32 刚需）
- **深色模式 + DWM 视觉定制**（DWMWA_USE_IMMERSIVE_DARK_MODE、Win11 圆角、Mica/Acrylic、WM_THEMECHANGED）— 复查 H1
- **应用清单 Manifest**（comctl32 v6、UAC requestedExecutionLevel、DPI manifest 整合）— 复查 H2
- **现代文件对话框 IFileDialog** — 复查 H4
- **Shell API**（SHGetFileInfo 文件图标、SHGetKnownFolderPath、IPropertyStore）— 复查 H5
- **IME 输入法**（中文受众关键）— 复查 M5
- **剪贴板 / 多显示器混合 DPI / GDI 打印 / ITaskbarList3** — 复查 M6/M7/M8

> COM 基础（复查 L2）**不在此幕**——它属于 Act 2 器官 6（对象模型），与 GObject/ImGui「无」对照。

---

## B. `miniui/`（骨架）——亲手拼一具骨架

### 迁移（现有 handbook → 本幕）
ch62 XCB 窗口 → ch63 事件循环 → ch64 Cairo 绘制 → ch65 控件树+RAII → ch66 布局+Concepts → ch67 信号+Property → ch68 渲染管线+异步IO → ch69 迷你编辑器+协程 → ch70 Observable → ch71 线程池。全部迁到 `act1-specimen/miniui/`。

> ⚠️ MiniUI 现有文章用**旧编号与旧 `miniui/.hpp` 框架命名**（实际代码是 per-stage `include/*.h`），迁移时**按主题映射、勿照搬编号/路径**，并顺带对齐文档—代码漂移。

### 深化（把骨架做厚——补 Phase 2 deferred 的三个缺口）
1. **theming**：主题/CSS 化机制（呼应 GTK CSS、WinUI3 Style）
2. **DPI**：缩放因子融入布局与绘制（呼应 Act 4 DPI）
3. **GPU 后端**：把 Cairo 后端抽象出 `Painter` 接口，加一个 Direct2D 后端（呼应 Act 3 渲染栈）——证明骨架可换「皮肤」

### 「大门」章（原 ch72，收束 + 桥到 Act 2）
MiniUI→工业框架八行映射表（UniqueHandle→com_ptr、Signal/Property→event/DependencyProperty、Task→IAsyncOperation、postTask→DispatcherQueue、Observable→IObservableVector、ThreadPool→WinRT ThreadPool、LayoutStrategy→Panel Measure/Arrange、Widget 树→DependencyObject 树）。**这章是 Act 2 的入口**。

---

## C. 复查待修（本幕范围）

**P0 API/技术（高危，迁移时必修）**
- `handbook/70_observable.md` **M6**：`take_until(Observable<std::any>&)` + `reinterpret_cast` → UB。改函数模板 `take_until<U>`
- `handbook/70_observable.md` **M7**：lambda 捕获 `[&, dragStream]` 但 `dragStream` 未声明 → 无法编译
- `handbook/65_widget_tree.md` **M1**：称 `std::scope_exit` 是 C++23。实际仅 LFTS TS，`-std=c++23` 编译失败 → 改用可用的 RAII 手法
- `handbook/62_xcb_window.md` **B1**：`xcb_connect` 返回值判失败错（须 `xcb_connection_has_error`）
- `handbook/62_xcb_window.md` **B2**：`& ~0x80` 语义错（剥离 SendEvent 标志，非区分错误；错误判 `response_type==0`）
- `handbook/71_threadpool.md` **M12/M14**：【需人工二次确认】shutdown 丢唤醒？runAsync 捕获 `[this]` 悬空？

**P0 编号/结构**
- `17_5` 编号冲突 + `sortEntries` 字典序倒挂（见 [migration.md](migration.md) §4）
- 中文序号系统性偏移 -1（ch1/ch3 补号）

**P0 代码—文章脱节**
- stage2/3/5/7/8/9：文章 `run()` vs 源码 `execution()`、`add_child` vs `addChild`、文章引用的 `frontSurface_`/`taskPipeWrite_`/`TextBuffer::loadFromLines` 源码不存在 → 统一加「教学骨架 vs 仓库源码」提示，或对齐命名

**P3 配图**
- ch63 事件循环、ch66 布局 Measure→Arrange→Render、ch68 渲染管线、ch62 X11 架构：补架构/流程图（复查列为核心概念图）

---

## 产出验收
- [ ] win32/ 全部章节迁移完成 + 现代缺口章补齐 + 每章加「器官视角」导言
- [ ] miniui/ 10 章迁移 + theming/DPI/GPU 三深化章 + 大门章
- [ ] 本幕复查 P0 全修
- [ ] `pnpm build` 通过，侧栏正确显示 act1-specimen
