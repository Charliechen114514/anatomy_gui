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

> **适用范围**：本规范适用于 **Win32 / graphics 篇**（`tutorial/native_win32/`）——工程教程基调，每章配 `src/` 可编译示例。**MiniUI 篇**（`tutorial/hands_on_ur_own_gui/`）为启发式手册基调（不给函数签名 / 骨架 / 代码，引导读者自行实现框架），不在此规范约束内。

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
# 文档站点（VitePress）
pnpm install
pnpm build          # 构建产物；或 pnpm dev 本地实时预览

# 代码示例 —— Windows（Win32 / graphics）
cmake -B build -S src/tutorial/native_win32
cmake --build build --config Release

# 代码示例 —— Linux（MiniUI stages，需 xcb + cairo）
sudo apt-get install -y libxcb1-dev libxcb-util-dev libxcb-keysyms1-dev libcairo2-dev
python src/tutorial/build_anatomy_gui_for_tutorials.py --target all
```

## 有问题？

欢迎在 [Discussions](https://github.com/Charliechen114514/anatomy_gui/discussions) 中提问。
