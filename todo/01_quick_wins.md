# Phase 1 — Quick Wins（现有问题修复）

## 1.1 修复 README.md 重复段落

**文件**：`README.md`
**问题**：第 52-60 行 "Win32 阶段进度" 重复

**当前代码**（第 52-60 行）：
```markdown
## Win32 阶段进度

当前进度：**65%** | 18个章节已完成

详细的章节目录请查看：[**教程首页 →**](tutorial/index.md)

涵盖：基础篇(4章) | 控件篇(5章) | 对话框篇(3章) | 资源篇(6章)

完整的章节目录请查看：[**教程首页 →**](tutorial/index.md)

涵盖：基础篇(4章) | 控件篇(5章) | 对话框篇(3章) | 资源篇(6章)
```

**修复为**：
```markdown
## Win32 阶段进度

当前进度：**75%** | 19个章节已完成

详细的章节目录请查看：[**教程首页 →**](tutorial/index.md)

涵盖：基础篇(4章) | 控件篇(5章) | 对话框篇(3章) | 资源篇(6章) | 工具栏与状态栏(1章)
```

> 注意：此处先做最小修复（去重+更新数字）。Phase 3 会全面重写 README。

---

## 1.2 修复 mkdocs.yml 社交链接

**文件**：`mkdocs.yml` 第 186 行

**当前**：
```yaml
- icon: fontawesome/brands/github
  link: https://github.com/Awesome-Embedded-Learning-Studio
  name: GitHub
```

**改为**：
```yaml
- icon: fontawesome/brands/github
  link: https://github.com/Charliechen114514/anatomy_gui
  name: GitHub
```

---

## 1.3 创建 requirements.txt

**新文件**：`requirements.txt`

```
mkdocs-material>=9.5,<10
mkdocs-awesome-pages-plugin>=2.9,<3
mkdocs-git-revision-date-localized-plugin>=1.2,<2
mkdocs-minify-plugin>=0.8,<1
```

---

## 1.4 改进 .gitignore

**文件**：`.gitignore`

**替换为**：
```gitignore
# AI / Editor
.claude/
.vscode/
.idea/

# Python
__pycache__/
*.py[cod]
*.egg-info/
.venv/
venv/
env/

# MkDocs
site/

# Build artifacts (CMake)
build/
cmake-build-*/

# OS
.DS_Store
Thumbs.db
desktop.ini

# Compiled objects
*.obj
*.o
*.exe
*.dll
*.lib
*.pdb
*.ilk
```

---

## 1.5 添加 .editorconfig

**新文件**：`.editorconfig`

```ini
root = true

[*]
charset = utf-8
end_of_line = lf
insert_final_newline = true
trim_trailing_whitespace = true
indent_style = space
indent_size = 2

[*.md]
trim_trailing_whitespace = false

[*.{cpp,hpp,h,c}]
indent_size = 4

[CMakeLists.txt]
indent_size = 2

[*.yml]
indent_size = 2
```

---

## 1.6 启用 deploy.yml pip 缓存

**文件**：`.github/workflows/deploy.yml`

**修改 setup-python 步骤**（第 30-33 行）：

当前：
```yaml
- name: 设置 Python
  uses: actions/setup-python@v5
  with:
    python-version: "3.11"
    # cache: 'pip'  # 缓存依赖，加速构建
```

改为：
```yaml
- name: 设置 Python
  uses: actions/setup-python@v5
  with:
    python-version: "3.11"
    cache: 'pip'
```

**修改安装依赖步骤**（第 36-39 行）：

当前：
```yaml
- name: 安装依赖
  run: |
    pip install mkdocs-material
    pip install mkdocs-awesome-pages-plugin
    pip install mkdocs-git-revision-date-localized-plugin
```

改为：
```yaml
- name: 安装依赖
  run: pip install -r requirements.txt
```

---

## 验证

```bash
pip install -r requirements.txt
mkdocs build --clean
```
