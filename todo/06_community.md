# Phase 6 — 社区化完善

## 6.1 CONTRIBUTING.md

**新文件**：`CONTRIBUTING.md`

```markdown
# 贡献指南

感谢你对 anatomy_gui 项目的关注！欢迎任何形式的贡献。

## 如何贡献

### 报告问题

如果你发现教程内容有误、代码示例无法编译，或有其他建议：

1. 在 [Issues](https://github.com/Charliechen114514/anatomy_gui/issues) 中搜索是否已有类似问题
2. 如果没有，创建新 Issue，使用对应模板：
   - **Bug Report**：报告内容错误或代码问题
   - **Content Suggestion**：建议新增内容

### 提交修改

1. Fork 本仓库
2. 创建功能分支：`git checkout -b feature/your-feature`
3. 提交修改：`git commit -m "feat: 简要描述"`
4. 推送分支：`git push origin feature/your-feature`
5. 创建 Pull Request

### Commit 消息约定

| 前缀 | 用途 |
|:-----|:-----|
| `feat:` | 新增内容或功能 |
| `fix:` | 修复错误 |
| `docs:` | 文档修改 |
| `chore:` | 基础设施（CI、配置等） |

## 内容贡献规范

### 教程内容

- 保持与现有文章一致的写作风格
- 每节遵循：概念讲解 → 代码结构 → 常见坑 → 小作业
- 使用 Markdown 编写，注意 frontmatter 中的 tags 字段

### 代码示例

- 每个示例放在独立目录中
- 包含 `CMakeLists.txt`，遵循现有 CMake 结构
- 使用 C++20 标准，Unicode 字符集
- 代码需通过 `.clang-format` 格式化
- 确保示例可编译运行

## 开发环境

详见 [README.md](README.md) 中的「开发环境」章节。

## 本地验证

```bash
# 安装依赖
pip install -r requirements.txt

# 验证文档构建
mkdocs build --clean --strict

# 验证代码示例编译（Windows）
cd src/tutorial/native_win32
cmake -B build && cmake --build build --config Release
```

## 有问题？

欢迎在 [Discussions](https://github.com/Charliechen114514/anatomy_gui/discussions) 中提问。
```

---

## 6.2 Issue 模板

### .github/ISSUE_TEMPLATE/bug_report.md

**新文件**：

```markdown
---
name: Bug Report
about: 报告教程内容或代码示例的问题
title: '[BUG] '
labels: bug
---

## 问题描述
<!-- 清楚描述问题是什么 -->

## 所在位置
<!-- 哪个章节或代码示例？请提供链接或文件名 -->

## 期望行为
<!-- 应该是什么样的？ -->

## 实际行为
<!-- 实际是什么样的？ -->

## 环境信息（如适用）
- 操作系统：
- 编译器/版本：
- 其他：
```

### .github/ISSUE_TEMPLATE/content_suggestion.md

**新文件**：

```markdown
---
name: Content Suggestion
about: 建议新增内容或改进现有章节
title: '[SUGGESTION] '
labels: enhancement
---

## 建议内容
<!-- 你希望看到什么内容？ -->

## 相关章节
<!-- 与哪个章节相关？ -->

## 参考资料（可选）
<!-- 有没有参考的文档或教程链接？ -->
```

### .github/ISSUE_TEMPLATE/config.yml

**新文件**：

```yaml
blank_issues_enabled: false
contact_links:
  - name: 提问
    url: https://github.com/Charliechen114514/anatomy_gui/discussions
    about: 一般性问题请在 Discussions 中提出
```

---

## 6.3 PR 模板

### .github/PULL_REQUEST_TEMPLATE.md

**新文件**：

```markdown
## 变更类型

- [ ] 内容修正（修复教程中的错误）
- [ ] 新增内容（新增章节或段落）
- [ ] 代码示例（新增或修复代码示例）
- [ ] 基础设施（CI/CD、配置文件等）

## 变更说明

<!-- 简要描述本次变更的内容和原因 -->

## 关联 Issue

<!-- 关联的 Issue 编号，如 fixes #123 -->

## 检查清单

- [ ] 我已在本地构建验证（`mkdocs build --clean --strict`）
- [ ] 如果修改了代码示例，我已验证其可以编译运行
- [ ] 我的变更与现有内容风格一致
```

---

## 6.4 CHANGELOG.md

**新文件**：`CHANGELOG.md`

```markdown
# 更新日志

本项目的所有重要变更都会记录在此文件中。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/)。

## [Unreleased]

## [0.2.0] - 2025-04-XX

### Added
- 图形渲染系列教程（GDI、GDI+、Direct2D、HLSL、D3D11、D3D12、自定义控件、OpenGL）
- 工具栏与状态栏章节
- MkDocs 文档站点与自动部署
- 代码示例构建验证 CI
- 仓库基础设施完善（CONTRIBUTING.md、Issue/PR 模板等）

## [0.1.0] - 2025-03-22

### Added
- 初始公开版本
- Win32 原生编程系列（基础篇、控件篇、对话框篇、资源篇）
- 10 个渐进式代码示例
- 6 个实战练习项目
- Roadmap 完整规划
```

> 注意：日期需根据实际 git 历史调整。

---

## 验证

- 在 GitHub 仓库 Settings → Features 中确认 Issue 模板已生效
- 创建测试 PR，确认模板自动填充
- CHANGELOG.md 在仓库根目录可见
- CONTRIBUTING.md 在新建 Issue 时自动提示
