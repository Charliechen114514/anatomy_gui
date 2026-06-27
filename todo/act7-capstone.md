# 草稿 07 · 毕业项目与研究实验室

> 目标：用若干中大型项目验证整套 GUI 解剖学，而不是只做一个最终 demo。毕业项目应覆盖传统 Win32、现代 Win32、Anatomy GUI v2、工业框架对照、渲染、产品化与测试。

---

## 一、毕业项目的角色

毕业项目不是“教程结束奖励”，而是全书的压力测试：

- 理论是否能指导真实设计？
- Win32 现代范式是否能写出不冻结 UI 的应用？
- Anatomy GUI v2 是否真的有足够器官？
- 比较解剖是否能帮助读者迁移到其他框架？
- 渲染、输入法、DPI、a11y、打包是否闭环？

建议从“单一 Markdown 编辑器”扩展为“项目实验室”。

---

## 二、项目分级

### Level 1 · 单器官实验

用于每章结尾：

- 消息循环模拟器。
- 控件树 dump。
- dirty region visualizer。
- IME logger。
- binding counter。
- coroutine file loader。

特点：

- 小。
- 只验证一个概念。
- 可作为后续项目部件。

### Level 2 · 多器官小应用

用于每个大板块收束：

- Settings Panel。
- Async Image Viewer。
- Log Viewer。
- Mini TextBox。
- Theme Playground。
- UI Inspector Lite。

特点：

- 2-5 个章节共同完成。
- 有真实交互。
- 能进入产品化 checklist。

### Level 3 · 毕业项目

用于全书收束：

- Markdown Studio。
- Desktop Inspector。
- File Explorer Lite。
- Vector Notes。
- Data Dashboard。

特点：

- 至少一个月级别。
- 多框架或多后端对照。
- 有交付包、测试、性能目标。

---

## 三、推荐毕业项目 1：Markdown Studio

### 规格

- 左侧 Markdown 编辑。
- 右侧预览。
- 文件打开、保存、另存为。
- 最近文件。
- 未保存修改提醒。
- 中文输入。
- 10MB 文件异步加载。
- 搜索。
- 深色模式。
- 高 DPI。
- 打包。

### 实现版本

| 版本 | 目的 |
|:---|:---|
| Win32 + WebView2 | 现代 Win32、COM、协程、混合应用 |
| Anatomy GUI v2 + WebView2 或自绘预览 | 验证自造框架器官 |
| GTK4 或 Qt 6 | 工业框架迁移对照 |

### 关键教学点

- 文件 IO 不能阻塞 UI。
- WebView2 与原生通信。
- IME 和文本编辑。
- DPI 和布局。
- 打包与 WebView2 runtime。

---

## 四、推荐毕业项目 2：Desktop Inspector

### 规格

- 显示窗口树 / 控件树。
- 显示当前鼠标下控件 bounds。
- 显示 DPI、focus、message count。
- 记录事件日志。
- 显示 dirty region / paint count。
- 可导出诊断报告。

### 实现版本

| 版本 | 目的 |
|:---|:---|
| Anatomy GUI v2 DevTools | 框架自我解剖 |
| Win32 外部 Inspector | 理解 HWND、消息、DPI |
| Web DOM Inspector 旁证 | 对照浏览器 DevTools 思想 |

### 关键教学点

- 一个框架要能被观察，才方便调试。
- 控件树、布局树、语义树可以同时可视化。
- 性能问题需要指标，不靠感觉。

---

## 五、推荐毕业项目 3：File Explorer Lite

### 规格

- 目录树。
- 文件列表。
- 图标和文件属性。
- breadcrumb。
- 异步枚举。
- 搜索。
- 拖放。
- 右键菜单。
- 大目录虚拟化。

### 实现版本

| 版本 | 目的 |
|:---|:---|
| Modern Win32 | Shell API、COM、ListView、虚拟化 |
| Qt / GTK | 工业模型视图对照 |
| Anatomy GUI v2 | 验证 ListView、异步、图标缓存 |

### 关键教学点

- Shell 集成。
- 大数据列表性能。
- 背景线程和 UI 回投。
- 文件系统错误处理。

---

## 六、推荐毕业项目 4：Vector Notes

### 规格

- 无限画布。
- 文本块。
- 矩形、连线、图片。
- 选择、拖拽、缩放。
- undo / redo。
- 导出 PNG / SVG。

### 实现版本

| 版本 | 目的 |
|:---|:---|
| Anatomy GUI v2 | hit test、render、layout、input 综合 |
| ImGui Tool | 即时模式工具 UI 对照 |
| Web Canvas 旁证 | Canvas / DOM 混合模型 |

### 关键教学点

- scene graph。
- hit testing。
- dirty region。
- transform。
- command pattern。

---

## 七、推荐毕业项目 5：Data Dashboard

### 规格

- 表格。
- 图表。
- 过滤。
- 排序。
- 大量数据虚拟化。
- 后台刷新。
- 可访问性。

### 实现版本

| 版本 | 目的 |
|:---|:---|
| Win32 / D2D | 自绘表格和性能 |
| Qt / GTK | Model/View 对照 |
| Web / React | 声明式数据 UI 对照 |

### 关键教学点

- Model/View。
- 虚拟化。
- diff。
- 性能观测。
- 键盘导航和 a11y。

---

## 八、研究实验室主题

这些可以作为附加专题，不必全部做成主教程：

- Retained vs Immediate 同题 benchmark。
- 自制 Flexbox 子集。
- 自制 CSS selector-lite。
- UIA provider from scratch。
- Text shaping playground。
- DirectComposition layer playground。
- WebView2 安全沙箱实验。
- GUI 自动化测试工具。
- Screenshot golden test pipeline。
- Frame profiler。
- Cross-platform backend swap：Win32/D2D 与 XCB/Cairo。

---

## 九、统一验收标准

毕业项目至少满足：

- [ ] 可编译。
- [ ] 有 README 和架构图。
- [ ] 有正常使用路径。
- [ ] 有错误状态和空状态。
- [ ] 支持中文路径或中文输入。
- [ ] 支持高 DPI。
- [ ] UI 不因大文件或慢操作冻结。
- [ ] 有至少一个自动化或半自动化验证。
- [ ] 有性能指标。
- [ ] 有打包说明。

---

## 十、最推荐的首个项目

建议第一个正式毕业项目仍然选 Markdown Studio。

理由：

- 需求足够真实，但范围可控。
- 能自然覆盖 Win32、WebView2、协程、IME、DPI、打包。
- 也能迁移到 Anatomy GUI v2、GTK、Qt 做对照。
- 文本编辑和预览天然牵涉 GUI 深水区。

第二个项目建议做 Desktop Inspector，因为它能反过来服务全书调试和教学。
