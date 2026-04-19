# Phase 5 — 阅读基建（MkDocs 增强）

## 5.1 添加 mkdocs-minify-plugin

### requirements.txt 追加

在 `requirements.txt` 中追加（Phase 1 已包含此行，确认存在）：

```
mkdocs-minify-plugin>=0.8,<1
```

### mkdocs.yml 修改

在 `plugins` 段添加：

```yaml
plugins:
  - search:
      # ... 现有配置不变
  - awesome-pages
  - git-revision-date-localized:
      # ... 现有配置不变
  - minify:
      minify_html: true
```

---

## 5.2 自定义 404 页面

### mkdocs.yml 修改

在 `theme` 段添加 `custom_dir`：

```yaml
theme:
  name: material
  custom_dir: overrides  # 添加此行
  language: zh
  # ... 其余配置不变
```

### 新建 overrides/404.html

**新文件**：`overrides/404.html`

```html
{% extends "base.html" %}

{% block content %}
  <h1 style="margin-top: 2rem;">404 - 页面未找到</h1>
  <p>抱歉，您访问的页面不存在或已被移动。</p>
  <p>
    <a href="{{ config.site_url }}" class="md-button md-button--primary">
      返回首页
    </a>
  </p>
{% endblock %}
```

---

## 5.3 .pages 导航控制

### tutorial/native_win32/.pages

**新文件**：`tutorial/native_win32/.pages`

```yaml
title: Win32 原生编程
```

### tutorial/bonus/.pages

**新文件**：`tutorial/bonus/.pages`

```yaml
title: 补充专题
```

**说明**：awesome-pages 插件已安装，`.pages` 文件可控制侧边栏标题和排序。目前文件名以数字前缀排列，自动排序已正确，只需设置分组标题。

---

## 5.4 标签分类系统

### mkdocs.yml 修改

在 `plugins` 段添加 tags：

```yaml
plugins:
  - tags:
      tags_file: tags.md
  - search:
      # ...
  - awesome-pages
  - git-revision-date-localized:
      # ...
  - minify:
      minify_html: true
```

### tutorial/tags.md

**新文件**：`tutorial/tags.md`

```markdown
# 标签索引

以下是所有教程文章的标签分类。

[TAGS]
```

### 为教程添加 tags（渐进进行）

在各教程文件的 YAML frontmatter 中添加标签。示例：

```yaml
---
tags:
  - Win32
  - 入门
  - 消息机制
---
```

**建议标签体系**：

| 标签 | 适用范围 |
|:-----|:---------|
| Win32 | 第一篇全部章节 |
| GDI | 第 18-25 章 |
| GDI+ | 第 26-28 章 |
| Direct2D | 第 29-33 章 |
| DirectWrite | 第 32 章 |
| HLSL | 第 34-36 章 |
| D3D11 | 第 37-42 章 |
| D3D12 | 第 43-47 章 |
| OpenGL | 第 51-52 章 |
| 自定义控件 | 第 48-50 章 |
| 入门 | 第 0-3 章 |
| 控件 | 第 4-8 章 |
| 对话框 | 第 9-11 章 |
| 资源 | 第 12-17 章 |
| 图形渲染 | 第三篇全部 |

> 此步骤可渐进进行，不阻塞其他 Phase。

---

## 5.5 mkdocs-redirects 插件（可选）

如需在未来移动文件位置，可预先安装：

### requirements.txt 追加

```
mkdocs-redirects>=1.2,<2
```

### mkdocs.yml 配置

```yaml
plugins:
  - redirects:
      redirect_maps:
        # 示例：如果将来移动了文件
        # 'old_path.md': 'new_path.md'
```

---

## 5.6 添加导航分组（可选，大改）

**问题**：54 篇文章全在 `tutorial/native_win32/` 下，侧边栏非常长。

**方案**：拆分为子目录：

```
tutorial/native_win32/
├── .pages
├── basics/          (0-3)
├── controls/        (4-8)
├── dialogs/         (9-11)
├── resources/       (12-17, 17_5)
├── gdi/             (18-25)
├── gdiplus/         (26-28)
├── direct2d/        (29-33)
├── hlsl/            (34-36)
├── d3d11/           (37-42)
├── d3d12/           (43-47)
├── custom_controls/ (48-50)
└── opengl/          (51-52)
```

**每个子目录的 .pages**：

```yaml
title: GDI 图形
nav:
  - ...
```

**风险**：
- 涉及 54 个文件移动
- 所有内部链接需更新
- 搜索引擎索引会失效
- 必须配合 mkdocs-redirects

**建议**：作为独立 PR 执行，不在本次优化中实施。可记录为后续改进项。

---

## 验证

```bash
pip install -r requirements.txt
mkdocs build --clean --strict
```

检查：
- 构建无错误无警告
- 访问 `/nonexistent-page/` 确认 404 页面显示
- 侧边栏分组标题正确
- 标签索引页可访问
