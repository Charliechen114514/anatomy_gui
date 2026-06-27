# 迁移清单 · 旧结构 → 新 act 结构

> ⚠ **本文件已废弃（2026-06-18）**：六幕全量重排不再执行，改用「增量挂载」——现有 72 章冻结为主干，新内容逐篇写、挂新目录。下文的全量迁移操作清单**不再适用**，仅保留作历史背景。
>
> ~~目标（原）：把现有 `tutorial/` + `src/tutorial/` 资产按解剖新路线重新归位。**非破坏性**——所有文件 git 可追踪；执行前先开分支 `refactor/anatomy-restructure`，分批 commit。~~
> 原则：先把 `todo/` 理顺（已完成），再动实际文件。本文件是「干活」时的操作清单。

---

## 一、tutorial/ 内容迁移

| 现有路径 | → 新路径 | 说明 |
|:---|:---|:---|
| `native_win32/0–3_*.md`（基础） | `act0-prep/` + `act1-specimen/win32/` | 0 篇世界观入 act0；hello window/register/create/msgloop 入 win32 标本 |
| `native_win32/4–8_*.md`（控件/ListView/TreeView/MoreControls） | `act1-specimen/win32/controls/` | |
| `native_win32/9–11_*.md`（对话框） | `act1-specimen/win32/dialogs/` | |
| `native_win32/12–17_*.md`（资源/菜单/图标/串表/模板/VS编辑器） | `act1-specimen/win32/resources/` | **先修 `17_5` 编号冲突**（见 §4） |
| `native_win32/17_5_*.md`（工具栏状态栏） | `act1-specimen/win32/controls/` | 重命名为 `18`，后续顺延（复查 P0） |
| `native_win32/52–60_*.md`（进阶消息/子类化/Hook/托盘/拖放/定时器/加速键/消息机制/系统消息） | `act1-specimen/win32/advanced/` | **前移**：复查 P2 建议这些纯 Win32 章从 GPU 渲染后回炉——新结构天然解决 |
| `native_win32/18–25_*.md`（GDI） | `act3-render/gdi/` | |
| `native_win32/26–28_*.md`（GDI+） | `act3-render/gdiplus/` | |
| `native_win32/29–33_*.md`（Direct2D/DirectWrite） | `act3-render/direct2d/` | |
| `native_win32/34–36_*.md`（HLSL） | `act3-render/hlsl/` | |
| `native_win32/37–42_*.md`（D3D11） | `act3-render/d3d11/` | |
| `native_win32/43–47_*.md`（D3D12） | `act3-render/d3d12/` | |
| `native_win32/48–50_*.md`（自定义控件 OwnerDraw/FullCustom/HitTest） | `act1-specimen/win32/advanced/`（或 `act3-render/custom/`） | 桥接章：Win32 自绘控件 = 器官 3/5 的标本实例 |
| `native_win32/51_*.md`（OpenGL） | `act3-render/opengl/` | |
| `native_win32/61_*.md`（精灵渲染器） | `act3-render/d3d11/`（阶段项目） | 复查指出内容实为 D3D11 |
| `hands_on_ur_own_gui/handbook/62–71_*.md` | `act1-specimen/miniui/` | MiniUI 骨架 |
| `hands_on_ur_own_gui/fullread/` | 评估：留作长读视图则补 index，否则删 | 复查 P3-T23 |
| `bonus/` | `act1-specimen/win32/bonus/` 或 `reference/` | 接回站点（optimize_site T6） |

**新增目录**（无现有资产）：
- `act2-dissect/organs/`（8 章全新）
- `act2-dissect/specimens/{gtk4,winui3,imgui}/`
- `act4-ship/`
- `act5-capstone/`
- `reference/`（GTK3 差异、系统 API、被精简内容）

---

## 二、src/ 源码迁移

镜像 `tutorial/` 的 act 结构：

| 现有 | → 新 |
|:---|:---|
| `src/tutorial/native_win32/*` | `src/tutorial/act1-specimen/win32/*` |
| `src/tutorial/graphics/*` | `src/tutorial/act3-render/*` |
| `src/tutorial/anatomy_gui_for_tutorials/stage1-10` | `src/tutorial/act1-specimen/miniui/*` |

> `src/` 顶层职责不变（仍装源码）。CMake 顶层入口随子目录更名同步。

---

## 三、站点配置同步（VitePress，阻塞项）

目录一改，站点侧必须同步，否则线上全乱：

1. **`site/.vitepress/config/sidebar.ts`**：
   - 修 `getGroupIndex` 的 prefix bug（`>=` → `>`，或 prefix 改填组首章号）——见 [optimize_site.md](optimize_site.md) **T1**。
   - `buildSidebar()` 扫描目录从 `native_win32`/`bonus` 改为按 act 扫描（`act0-prep`…`act5-capstone` + `reference`）。
2. **接回 MiniUI**：当前 handbook 全站零入口——见 **T2**（迁移后 MiniUI 落到 `act1-specimen/miniui/`，自然进侧栏）。
3. **`nav.ts` / `HomeRoadmap.vue`**：导航与首页路线图改为六幕结构。
4. **hlsl 高亮**：`languageAlias` 加 `hlsl:'cpp'`（T5）。

---

## 四、编号与命名约定（迁移时一次性扫平）

- **`17_5` 冲突**：重命名为 `18`，后续顺延 +1（复查 P0）；`sortEntries` 改 `parseFloat(replace('_','.'))`。
- **中文序号偏移 -1**：ch1 补「（一）」、ch3 补「（三）」（复查 P0）。
- **新 act 内编号**：每幕内独立编号（如 `act2-dissect/organs/01-window-display.md`），站点按目录顺序生成侧栏。
- **「文章示例 vs 仓库源码」脱节**：迁移时在每个代码单元 README 或文章顶部统一加一句「本文为教学骨架，仓库源码已模块化/走另一实现路线」（复查 P0 维度 5 统一对策）。

---

## 五、砍 / 合并清单

| 内容 | 处理 | 去向 |
|:---|:---|:---|
| GTK3 双线主体 | 砍 | `reference/gtk3-diff.md`（差异附录） |
| CEF 独立 2 章 | 合并 | 进器官 5（绘制）/ 6（对象模型）的标本对照 |
| Vulkan 独立 2 章 | 压缩为 1 章 | `act3-render/vulkan-vs-d3d12.md` |
| 注册表 / 系统服务章 | 移 | `reference/win32-system-api.md` |
| 毕业项目第 3 个完整实现 | 改 | 设计哲学对比文（`act5-capstone/`） |

---

## 六、执行顺序

1. 开分支 `refactor/anatomy-restructure`。
2. **先修编号 bug**（§4：17_5、中文序号、sortEntries）——独立 commit。
3. **迁移 Act 1**（win32 + miniui）→ 同步改 sidebar 扫描 + 接回 MiniUI → 本地 `pnpm build` 验证。
4. 写 **Act 2** 新内容（器官章，依赖 Act 1 就位）。
5. 迁移 **Act 3** + 修图形 bug。
6. 写 **Act 4 / Act 5**。
7. 全程：每迁移一章，对照 [`content-fixes.md`](content-fixes.md) 与各 act 文件的「复查待修」节修本幕相关条目。

> 每个 act 的具体「写什么 + 修哪些复查条目」见 `act-1-specimen.md` … `act-5-capstone.md`。
