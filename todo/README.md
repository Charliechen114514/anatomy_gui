# todo/ · 规划索引

> anatomy_gui 的规划层。权威路线在根 [`Roadmap.md`](../Roadmap.md)。

---

## 近 / 远双层

- **近(当前执行)**：增量挂载。现有 72 章冻结为主干 + 标本库，站点呈现已修、CI stage1-10 全绿。新内容逐篇写、挂新目录，绝不回头重组现有章节。
- **远(北极星)**：下面的 8 幕愿景。逐批增量采纳，不一次性全做。

## 战略决策（D1–D4，2026-06 拍板）

| # | 决策 | 一句话 |
|:---:|:---|:---|
| D1 | 结构 = 增量挂载 | 否决六幕全量重排；72 章冻结为主干，新内容逐篇挂新目录 |
| D2 | 写作 = 分而治之 | Win32/graphics 工程教程（给代码）；MiniUI 启发式手册（不给骨架） |
| D3 | Anatomy v2 后端 | 保持 XCB+Cairo 主线；Win32+D2D 作补充接入说明（否决重做） |
| D4 | 比较样本 | Qt/Web/Slint 加回作远期旁证；近期 A 级 5 样本 |

---

## 远期 8 幕愿景（逐批增量采纳）

| 幕 | 文件 | 重点 | 状态 |
|:---:|:---|:---|:---|
| Act 0 | [act0-theory.md](act0-theory.md) | GUI 思想史与理论词汇（12 器官、保留/即时、命令/声明/响应式） | 🟢 增量首选 |
| Act 1 | *(现有 native_win32 主干)* | 传统 Win32 解剖：HWND / 消息泵 / 控件 / GDI | ✅ 已就绪 |
| Act 2 | [act2-modern-win32.md](act2-modern-win32.md) | 现代 Win32/C++20：RAII / COM / DWM / WebView2 / 协程 | 🟢 增量 |
| Act 3 | [act3-anatomy-v2.md](act3-anatomy-v2.md) | Anatomy GUI v2 = MiniUI v1 深化（theming/DPI/a11y/IME/DevTools）+ 后端接入 | 🟡 深化 |
| Act 4 | [act4-comparative.md](act4-comparative.md) | 工业框架比较解剖：12 器官 × A 级 5 样本 + 旁证 | 🟡 系列核心 |
| Act 5 | [act5-rendering.md](act5-rendering.md) | 渲染视觉系统：GDI→D2D→D3D + 文本 / 合成 / 动画 | 🟢 扩展 |
| Act 6 | [act6-product.md](act6-product.md) | 产品化人因：a11y / IME / i18n / 测试 / 打包 / 性能 | 🟢 高价值 |
| Act 7 | [act7-capstone.md](act7-capstone.md) | 多毕业项目：Markdown / Inspector / 文件管理器 / 绘图 | 🟡 扩展 |

### 推进顺序

**第一批**（改变读者理解方式）：Act0 理论总论 → 保留/即时模式 → 传统→现代 Win32 桥接 → 协程版消息循环 → Anatomy v2 总架构 → v2 五个最小 stage → 同题多框架对照。

**第二批**（真实软件能力）：IME/文本/Unicode → DPI/多显示器/高对比 → a11y/UI Automation → 虚拟列表/脏区/帧调度 → 打包/更新/崩溃/自动化测试。

### 待逐步采纳

Act0 作正式入口？/ Win32 传统·现代拆两幕？/ 器官 8→12？/ 产品化提升主线？/ 多毕业项目？/ 8 幕正式更新 Roadmap？

---

## 近期修复清单

| 文件 | 内容 |
|:---|:---|
| [content-fixes.md](content-fixes.md) | 文字质量修复（错别字 / 病句 / 术语 / 断链） |
| [optimize_site.md](optimize_site.md) | VitePress 站点优化 T1–T23（T1/T2/T5/T7/T8 已做） |
| [references.md](references.md) | 可借鉴教程系列 + 定位依据 |

---

## 状态图例
✅ 已就绪 / 🟢 增量首选 / 🟡 待深化或扩展 / 🚧 待写
