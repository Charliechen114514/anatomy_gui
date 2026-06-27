# todo/ · 规划索引

> anatomy_gui 的规划层。权威路线在根 [`Roadmap.md`](../Roadmap.md)；本目录放逐幕执行计划、内容修复清单与参考。
> 原则：**先理顺 todo，再动 `tutorial/` 与 `src/` 的实际内容。**
>
> ⚠ **2026-06-18 战略转向**：不再做"六幕全量重排 / 迁移"。现有 72 章冻结为 v1.0 主干（站点呈现已修、CI 全绿），新内容改为**增量挂载**——逐篇写、挂新目录。下表中"迁移"类任务（act-1 / act-3 / migration）**作废**，"新内容写作"类（act-2 / 4 / 5）改为增量推进。

---

## 执行（按幕）

| 文件 | 内容 | 状态 |
|:---|:---|:---|
| [`act-1-specimen.md`](act-1-specimen.md) | Act 1 标本与骨架：Win32 迁移 + MiniUI 深化 | ⚠ 迁移作废；MiniUI 已接回站点 |
| [`act-2-dissect.md`](act-2-dissect.md) | Act 2 比较解剖：8 器官章（系列核心） | 🚧 增量写——逐器官写、挂 `act2-dissect/` |
| [`act-3-render.md`](act-3-render.md) | Act 3 渲染栈迁移 + Vulkan 对比章 + 图形 bug | ⚠ 迁移作废；图形栈已在主干 |
| [`act-4-ship.md`](act-4-ship.md) | Act 4 工程化（DPI/theming/性能/线程/打包/i18n/a11y） | 🚧 增量写 |
| [`act-5-capstone.md`](act-5-capstone.md) | Act 5 比较版毕业项目（2 实现 + 1 哲学对比文） | 🚧 增量写 |
| [`migration.md`](migration.md) | 逐文件迁移清单 + 站点配置同步 + 编号修复 | ⚠ 已废弃 |

## 内容修复清单

| 文件 | 内容 |
|:---|:---|
| [`content-fixes.md`](content-fixes.md) | 文字质量修复（错别字 / 病句 / 术语 / 断链）。P0/技术类已在各 act 的「复查待修」节，不重复 |

## 参考

| 文件 | 内容 |
|:---|:---|
| [`references.md`](references.md) | 可借鉴教程系列（带 URL）+ 定位依据 |
| [`optimize_site.md`](optimize_site.md) | VitePress 站点优化 T1–T23（**目录重构必须同步 `sidebar.ts`**，见 T1/T2/T5） |

---

## 状态图例
- 🚧 待写 —— 全新内容，未动笔
- 🔄 待执行 / 迁移中 —— 资产已有，待搬动或修复
- ✅ 完成
- ⚠️ 阻塞 —— 需外部决策
