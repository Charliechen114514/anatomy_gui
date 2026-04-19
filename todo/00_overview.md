# anatomy_gui 仓库优化 — 总览

## 仓库现状

| 篇章 | 已完成 | 完成度 |
|:-----|:-------|:-------|
| 第〇篇 前言与准备 | 0 章 | 0% |
| 第一篇 Win32 原生编程 | 19 章 | ~75% |
| 第二篇 GUI 核心概念 | 0 章 | 0% |
| 第三篇 图形渲染 | 35 章 | ~95% |
| 第四~八篇 | 0 章 | 0% |

**核心问题**：README 仅展示 "18章65%"，54 篇文章中有 35 篇未被展示。

---

## 执行顺序

```
Phase 1 (01_quick_wins)     ──→ 无依赖，立即执行
Phase 2 (02_ci_cd)          ──→ 依赖 Phase 1 的 requirements.txt
Phase 3 (03_readme_rewrite) ──→ 依赖 Phase 1 的重复段落修复
Phase 4 (04_code_examples)  ──→ 无依赖，可与 Phase 5/6 并行
Phase 5 (05_reading_infra)  ──→ 依赖 Phase 1 的 requirements.txt 更新
Phase 6 (06_community)      ──→ 无依赖，可随时执行
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
