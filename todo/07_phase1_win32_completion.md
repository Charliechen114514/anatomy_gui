# Phase 1 — Win32 进阶与图形收尾

> **优先级：最高** | 将第一篇从 75% 推到 100%，第三篇从 95% 推到 100%

## 前置依赖

- Phase 0（现有内容清理）必须先完成

## 1A. Win32 第一篇进阶章节

> Roadmap.md 中已有详细提纲，需要实际撰写完整教程。

### 教程文件（`tutorial/native_win32/`）

| 编号 | 文件名 | 标题 | Roadmap 参考 |
|:-----|:-------|:-----|:-------------|
| 59 | `59_ProgrammingGUI_NativeWindows_MsgMechanism.md` | 消息机制补充（SendMessage/PostMessage/跨线程） | 1.2 (L109-131) |
| 60 | `60_ProgrammingGUI_NativeWindows_SystemMessages.md` | 常用系统消息补充（WM_MOUSEWHEEL/WM_NCHITTEST/Raw Input） | 1.3 (L135-151) |
| 53 | `53_ProgrammingGUI_NativeWindows_Subclassing.md` | 子类化与超类化 | 1.7.1 (L155-183) |
| 54 | `54_ProgrammingGUI_NativeWindows_Hook.md` | Hook 机制 | 1.7.2 (L187-203) |
| 55 | `55_ProgrammingGUI_NativeWindows_SystemTray.md` | 系统托盘 | 1.7.3 (L207-225) |
| 56 | `56_ProgrammingGUI_NativeWindows_DragDrop.md` | 拖放（Drag & Drop） | 1.7.4 (L229-253) |
| 57 | `57_ProgrammingGUI_NativeWindows_Timer.md` | 定时器 | 1.7.5 (L257-273) |
| 58 | `58_ProgrammingGUI_NativeWindows_TextEditor.md` | 阶段项目：纯 Win32 文本编辑器（全功能） | L276-290 |

**阶段项目功能规格**：
- 菜单栏（文件、编辑、帮助）
- 工具栏（新建、打开、保存、剪切、复制、粘贴）
- 状态栏（行号、列号、文件名、修改状态）
- 核心（Edit/RichEdit 控件、文件对话框、快捷键加速表）
- 可选（查找/替换、行号边栏、语法高亮 via RichEdit）
- 正确处理大文件、编码检测、未保存确认

### 代码示例（`src/tutorial/native_win32/`）

| 目录 | 内容 | 链接库 |
|:-----|:-----|:-------|
| `11_subclassing/` | 子类化 Edit 控件，拦截 Enter 键 | (纯 Win32) |
| `12_hook_keyboard/` | 全局键盘钩子监听 Print Screen | (纯 Win32) |
| `13_system_tray/` | 最小托盘程序 + 右键菜单 + WM_TASKBARCREATED | shell32.lib |
| `14_drag_drop/` | WM_DROPFILES 文件拖入 + IDropTarget OLE 示例 | ole32.lib |
| `15_timer_clock/` | WM_TIMER 模拟时钟（GDI 绘制时分秒针） | gdi32.lib |
| `exercises/07_text_editor/` | 全功能文本编辑器阶段项目 | comctl32.lib, riched20.lib |
| `exercises/08_cross_thread_msg/` | 跨线程 PostMessage 进度条 | (纯 Win32) |

### 需修改的文件

- [ ] `src/tutorial/native_win32/CMakeLists.txt` — 添加 7 个 `add_subdirectory`
- [ ] 可选：新建 `common/ScopedSelectObject.hpp` RAII GDI 工具类
- [ ] 可选：新建 `common/Win32Helpers.hpp` 子类化/托盘辅助函数

---

## 1B. 第三篇图形渲染收尾

### 教程文件

| 编号 | 文件名 | 标题 |
|:-----|:-------|:-----|
| 61 | `61_ProgrammingGUI_Graphics_D3D11_SpriteRenderer.md` | 阶段项目：D3D11 2D 精灵渲染器 |

**精灵渲染器功能规格**：
- 支持贴图精灵渲染（位置/旋转/缩放/颜色叠加/Alpha 透明）
- 批处理合并（Sprite Batch）：同纹理精灵合并为一次 Draw Call
- 精灵图集（Texture Atlas / Sprite Sheet）：UV 子区域采样
- 文字渲染集成：DirectWrite 离屏渲染到纹理 → 精灵系统绘制

### 代码示例

| 目录 | 内容 | 链接库 |
|:-----|:-----|:-------|
| `src/tutorial/graphics/sprite_renderer/` | 完整 D3D11 Sprite Batch 项目 | d3d11.lib, d2d1.lib, dwrite.lib, d3dcompiler.lib |

### 需创建的文件

- [ ] `src/tutorial/graphics/CMakeLists.txt` — 新建图形示例构建框架
- [ ] 精灵渲染器完整项目（SpriteBatch 类、Atlas 支持、DirectWrite 文字渲染）

---

## 完成后更新

- [ ] `tutorial/index.md` — 添加新章节行到对应篇
- [ ] `Roadmap.md` — 更新第一篇完成度 75% → 100%，第三篇 95% → 100%
- [ ] `CHANGELOG.md` — 记录变更

## 验证

```bash
# 文档
mkdocs build --clean --strict

# Win32 代码
cd src/tutorial/native_win32 && cmake -B build && cmake --build build --config Release

# 图形代码
cd src/tutorial/graphics && cmake -B build && cmake --build build --config Release
```
