# Phase 2 — CI/CD 流水线增强

## 2.1 重构 deploy.yml：拆分验证与部署

**文件**：`.github/workflows/deploy.yml`

**完整替换为**：

```yaml
name: 自动部署 MkDocs

on:
  push:
    branches:
      - main
  workflow_dispatch:

permissions:
  contents: write

jobs:
  validate:
    runs-on: ubuntu-latest
    steps:
      - name: 检出仓库
        uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: 设置 Python
        uses: actions/setup-python@v5
        with:
          python-version: "3.11"
          cache: 'pip'

      - name: 安装依赖
        run: pip install -r requirements.txt

      - name: 验证构建
        run: mkdocs build --clean --strict

  deploy:
    needs: validate
    runs-on: ubuntu-latest
    steps:
      - name: 检出仓库
        uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: 设置 Python
        uses: actions/setup-python@v5
        with:
          python-version: "3.11"
          cache: 'pip'

      - name: 安装依赖
        run: pip install -r requirements.txt

      - name: 构建网站
        run: mkdocs build --clean

      - name: 部署到 GitHub Pages
        uses: peaceiris/actions-gh-pages@v4
        with:
          github_token: ${{ secrets.GITHUB_TOKEN }}
          publish_dir: ./site
```

**关键变化**：
- 拆为 `validate` + `deploy` 两个 job
- validate 使用 `--strict` 标志，任何构建警告都会失败
- deploy 依赖 validate 成功后才执行
- 启用 pip 缓存
- 使用 requirements.txt

---

## 2.2 新建代码示例构建验证

**新文件**：`.github/workflows/build-examples.yml`

```yaml
name: Build Code Examples

on:
  push:
    branches: [main]
    paths:
      - 'src/tutorial/native_win32/**'
      - '.github/workflows/build-examples.yml'
  pull_request:
    paths:
      - 'src/tutorial/native_win32/**'
  workflow_dispatch:

jobs:
  build:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4

      - name: Setup MSVC Developer Command Prompt
        uses: ilammy/msvc-dev-cmd@v1
        with:
          arch: x64

      - name: Configure CMake
        run: cmake -B build -S src/tutorial/native_win32

      - name: Build all examples
        run: cmake --build build --config Release
```

**说明**：
- 仅在代码示例变更时触发，节省 CI 资源
- 使用 windows-latest 运行器（预装 Visual Studio）
- `ilammy/msvc-dev-cmd@v1` 设置 MSVC 编译环境

---

## 2.3 新建 PR 检查工作流

**新文件**：`.github/workflows/pr-check.yml`

```yaml
name: PR Validation

on:
  pull_request:
    branches: [main]

jobs:
  check:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - uses: actions/setup-python@v5
        with:
          python-version: "3.11"
          cache: 'pip'

      - run: pip install -r requirements.txt

      - name: MkDocs strict build
        run: mkdocs build --clean --strict
```

**说明**：
- 每个 PR 自动运行
- `--strict` 确保无断链、无构建警告

---

## 验证

推送后检查 GitHub Actions：
1. `自动部署 MkDocs` workflow 应显示 validate → deploy 两步
2. 创建测试 PR，确认 `PR Validation` 自动运行
3. 手动触发 `Build Code Examples`，确认编译成功
