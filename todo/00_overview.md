# anatomy_gui 仓库优化 — 总览

## 仓库现状

| 篇章 | 已完成 | 完成度 |
|:-----|:-------|:-------|
| 第〇篇 前言与准备 | 0 章 | 0% |
| 第一篇 Win32 原生编程 | 19 章 | ~75% |
| 第二篇 GUI 核心概念 | 0 章 | 0% |
| 第三篇 图形渲染 | 34 章 | ~95% |
| 第四篇 跨平台框架（GTK） | 0 章 | 0% |
| 第五篇 现代 Windows 技术 | 0 章 | 0% |
| 第六篇 Web 混合方案 | 0 章 | 0% |
| 第七篇 工程化专题 | 0 章 | 0% |
| 第八篇 综合实战项目 | 0 章 | 0% |

**已移除的内容**：
- Qt 相关（第四篇 4.1-4.8、ch52 QOpenGLWidget）→ 有专门仓库维护
- wxWidgets 相关（第四篇 4.9-4.10）
- Electron / Tauri（第六篇 6.4）
- 辅助功能 Accessibility（第七篇 7.6）

---

## 执行顺序（基础设施 TODO）

```
Phase 1 (01_quick_wins)     ──→ 无依赖，立即执行
Phase 2 (02_ci_cd)          ──→ 依赖 Phase 1 的 requirements.txt
Phase 3 (03_readme_rewrite) ──→ 依赖 Phase 1 的重复段落修复
Phase 4 (04_code_examples)  ──→ 无依赖，可与 Phase 5/6 并行
Phase 5 (05_reading_infra)  ──→ 依赖 Phase 1 的 requirements.txt 更新
Phase 6 (06_community)      ──→ 无依赖，可随时执行
```

## 执行顺序（内容扩展 TODO）

```
Phase 0 (00_phase0_cleanup)          ──→ 已完成：Qt 内容移除、C++ 标准统一、ch3 重写
Phase 1 (07_phase1_win32_completion) ──→ Win32 进阶 + 图形收尾（最高优先级）
Phase 2 (08_phase2_gui_core)         ──→ GUI 核心概念（跨框架通识）
Phase 3 (09_phase3_gtk)              ──→ GTK3+GTK4 深度教程
Phase 4 (10_phase4_modern_windows)   ──→ COM 基础 + WinUI 3
Phase 5 (11_phase5_web_hybrid)       ──→ WebView2 + CEF
Phase 6 (12_phase6_new_graphics)     ──→ ImGui + Vulkan
Phase 7 (13_phase7_engineering)      ──→ 工程化专题（8 节）
Phase 8 (14_phase8_projects)         ──→ Markdown 编辑器三框架实现
Ongoing (99_ongoing_review)          ──→ 持续审核 + 图形代码示例补齐
```

---

## 文件清单

| Phase | 改动文件 | 新建/修改 |
|:------|:---------|:----------|
| 1 | README.md, mkdocs.yml, .gitignore, deploy.yml | 修改 4 + 新建 2（requirements.txt, .editorconfig） |
| 2 | deploy.yml | 修改 1 + 新建 3（build-examples.yml, pr-check.yml, 可选 link-check） |
| 3 | README.md, tutorial/index.md | 修改 2 |
| 4 | src/tutorial/native_win32/ | 新建 18（README.md × 17 + .clang-format + build.bat） |
| 5 | mkdocs.yml, requirements.txt | 修改 2 + 新建 4（404.html, .pages × 2, tags.md） |
| 6 | 根目录 + .github/ | 新建 6（CONTRIBUTING.md, CHANGELOG.md, issue × 2, config.yml, PR template） |

---

## 验证总方案

每个 Phase 完成后执行：

```bash
# 1. 依赖安装
pip install -r requirements.txt

# 2. 构建验证
mkdocs build --clean --strict

# 3. 代码示例编译（Windows 环境）
cd src/tutorial/native_win32 && cmake -B build && cmake --build build --config Release

# 4. 推送后检查 GitHub Actions 全部通过
```
