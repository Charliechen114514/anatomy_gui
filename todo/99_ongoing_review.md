# 持续进行 — 代码示例与内容审核

> 贯穿所有 Phase 的持续性工作，不产生独立 TODO 文件

## 现有 54 篇教程审查

### Win32 基础篇（ch0-17）

- [ ] 审核 ch0 Windows 程序本质 — API 是否过时
- [ ] 审核 ch1 消息机制与窗口创建 — 代码示例可编译性
- [ ] 审核 ch2 常用系统消息 — 补充缺失的现代消息
- [ ] 审核 ch3 DPI（Phase 0 重写后） — 验证 Windows 内容完整性
- [ ] 审核 ch4-8 控件篇 — 控件 API 是否过时
- [ ] 审核 ch9-11 对话框篇 — 代码可编译性
- [ ] 审核 ch12-17.5 资源与工具栏 — VS 2022 兼容性

### 图形渲染篇（ch18-52）

- [ ] 审核 ch18-25 GDI 系列 — GDI API 是否过时
- [ ] 审核 ch26-28 GDI+ 系列 — GDI+ 初始化模式
- [ ] 审核 ch29-33 Direct2D 系列 — 设备丢失处理是否正确
- [ ] 审核 ch34-36 HLSL 系列 — Shader Model 版本
- [ ] 审核 ch37-42 D3D11 系列 — SwapChain 创建参数
- [ ] 审核 ch43-47 D3D12 系列 — 与最新 D3D12 SDK 兼容性
- [ ] 审核 ch48-50 自定义控件 — 代码可编译性
- [ ] 审核 ch51 OpenGL（Phase 0 修复后） — 确认引用完整

### 通用检查

- [ ] 统一 frontmatter tags（参考标签分类体系）
- [ ] 检查所有内部链接是否有效（Phase 0 后重新检查）
- [ ] 检查代码块语法高亮标记是否正确

---

## 现有 16 个代码示例审查

### 基础示例（01-10）

- [ ] Phase 0 完成后验证所有 10 个示例最新 MSVC 编译通过
- [ ] 确认 C++20 特性使用一致
- [ ] 确认 .clang-format 格式化一致
- [ ] 确认所有 README.md 内容准确

### 练习项目（exercises/01-06）

- [ ] 验证 6 个练习项目编译运行
- [ ] 确认练习 README 中的学习要点与实际代码匹配

---

## 图形渲染代码示例补齐

> 来自 `todo/04_code_examples.md` 4.5 节。18-52 章均无配套代码示例。

### 按优先级排序

| 优先级 | 系列 | 章节范围 | 链接库 | 示例数 |
|:-------|:-----|:---------|:-------|:-------|
| P1 | GDI | 18-25 | gdi32.lib | 8 |
| P2 | 自定义控件 | 48-50 | (纯 Win32) | 3 |
| P3 | Direct2D | 29-33 | d2d1.lib, dwrite.lib | 5 |
| P4 | D3D11 | 37-42 | d3d11.lib, dxgi.lib, d3dcompiler.lib | 6 |
| P5 | GDI+ | 26-28 | gdiplus.lib | 3 |
| P6 | HLSL | 34-36 | d3dcompiler.lib + .hlsl 文件 | 3 |
| P7 | D3D12 | 43-47 | d3d12.lib, dxgi.lib | 5 |
| P8 | OpenGL | 51 | opengl32.lib + GLAD | 1 |

### 目录结构

```
src/tutorial/graphics/
  CMakeLists.txt          ← 新建总构建文件
  gdi/                    ← P1: 8 个示例
  gdiplus/                ← P5: 3 个示例
  direct2d/               ← P3: 5 个示例
  d3d11/                  ← P4: 6 个示例
  hlsl/                   ← P6: 3 个示例 + .hlsl 文件
  d3d12/                  ← P7: 5 个示例
  opengl/                 ← P8: 1 个示例
  custom_controls/        ← P2: 3 个示例
  sprite_renderer/        ← Phase 1 阶段项目
```

> 每个系列需要不同的 CMake 配置（链接库、编译定义）。
