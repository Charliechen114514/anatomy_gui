# Phase 6 — 新图形内容（ImGui + Vulkan + OpenGL 更新）

## 前置依赖

- Phase 1（Win32 进阶 + 第三篇收尾，提供图形渲染基础）

## 6A. Dear ImGui（4 节）

| 编号 | 文件名 | 标题 | 说明 |
|:-----|:-------|:-----|:-----|
| 86 | `86_ProgrammingGUI_ImGui_Basics.md` | Dear ImGui 基础 | 即时模式 GUI 原理、初始化、基本控件、Dear ImGui 生态 |
| 87 | `87_ProgrammingGUI_ImGui_D3D11.md` | ImGui D3D11 后端集成 | D3D11 渲染后端、字体加载、输入处理、与 Win32 集成 |
| 88 | `88_ProgrammingGUI_ImGui_OpenGL.md` | ImGui OpenGL 后端集成 | OpenGL3 后端、GLFW/GLAD 集成、多视口 |
| 89 | `89_ProgrammingGUI_ImGui_Custom.md` | ImGui 自定义控件与主题 | 自定义绘制、Docking、主题定制、ImGui 工具开发实战 |

### 代码示例（`src/tutorial/imgui/`）

| 目录 | 内容 | 依赖 |
|:-----|:-----|:-----|
| `01_imgui_hello/` | ImGui + Win32 + D3D11 最小示例 | Dear ImGui 源码, d3d11.lib |
| `02_imgui_d3d11/` | D3D11 后端完整工具应用 | Dear ImGui 源码, d3d11.lib |
| `03_imgui_opengl/` | OpenGL3 后端 + Win32 集成 | Dear ImGui 源码, opengl32.lib, GLAD |
| `04_imgui_custom/` | 自定义控件 + Docking + 主题 | Dear ImGui 源码 |

## 6B. Vulkan（2 节）

| 编号 | 文件名 | 标题 | 说明 |
|:-----|:-------|:-----|:-----|
| 90 | `90_ProgrammingGUI_Vulkan_Basics.md` | Vulkan 基础 | 实例/物理设备/逻辑设备/队列、与 D3D12 概念对比 |
| 91 | `91_ProgrammingGUI_Vulkan_Rendering.md` | Vulkan 渲染管线与 SwapChain | SwapChain、Render Pass、Pipeline、命令缓冲、帧同步 |

### 代码示例（`src/tutorial/vulkan/`）

| 目录 | 内容 | 依赖 |
|:-----|:-----|:-----|
| `01_vulkan_hello/` | Vulkan 初始化 + 清屏 | Vulkan SDK |
| `02_vulkan_triangle/` | 渲染管线完整：三角形渲染 | Vulkan SDK |

## 6C. OpenGL 更新

- [ ] 确认 ch52（Qt QOpenGLWidget）已在 Phase 0 中删除
- [ ] 确认 ch51 结尾引用已在 Phase 0 中修复

## 完成后更新

- [ ] `tutorial/index.md` — 添加 ImGui/Vulkan 篇表格
- [ ] `Roadmap.md` — 第三篇添加 ImGui/Vulkan 小节，更新进度
- [ ] `CHANGELOG.md` — 记录变更
