# 站点优化清单（VitePress）

> 本文件是 **anatomy_gui** VitePress 站点（配置在 `site/.vitepress/`，内容 srcDir 在 `tutorial/`）的可执行优化清单。每条都标注了 `文件:行` 与可直接粘贴的改动，可按优先级独立实施。
>
> 站点基本信息：VitePress 1.6.4 + Vue 3，部署在 GitHub Pages，在线地址 `https://charliechen114514.github.io/anatomy_gui/`，`base='/anatomy_gui/'`，`srcDir='../tutorial'`。
>
> 覆盖：站点配置 + 侧边栏 + 主题 + SEO。**未执行真实构建**（当前仓库无 `node_modules`），性能类项需在 `pnpm install && pnpm build` 后验证。

---

## 快速清单（按优先级）

| 级别 | 编号 | 一句话 | 改动量 |
|:---:|:---:|---|:---|
| 🔴 P0 | T1 | 侧边栏分组几乎全部错位（prefix 填成组末而非组首） | 改 16 行 |
| 🔴 P0 | T2 | MiniUI 实战（10 篇）整块内容线上不可达 | 配置+补 1 个 index |
| 🔴 P0 | T3 | 正文字号 0.82rem（≈13px）对中文过小 | 改 5 行 CSS |
| 🔴 P0 | T4 | 缺 OG/Twitter 元信息 + 无 sitemap | 粘配置 + 1 张图 |
| 🔴 P0 | T5 | 34 个 hlsl 代码块无语法高亮 | 改 1 行 |
| 🟠 P1 | T6 | 「补充专题」深链到单篇文章而非板块索引 | 补 index + 改链接 |
| 🟠 P1 | T7 | GitHub 入口在 nav 与 socialLinks 重复 | 删 1 行 |
| 🟠 P1 | T8 | 侧边栏 16 组全部展开，过长 | 改 1 行 |
| 🟠 P1 | T9 | 无 CJK 字体栈、无品牌色 | 加 CSS 变量 |
| 🟠 P1 | T10 | 74/76 篇无 frontmatter | 批量补 |
| 🟠 P1 | T11 | 两个重复的 C++ 转义插件 | 删 1 文件+1 import |
| 🟡 P2 | T12–T18 | 斑马纹/响应式/!important/孤儿图/lazy/搜索/options | 见下 |
| ⚪ P3 | T19–T23 | blockquote/focus/favicon/锁文件/fullread | 见下 |

---

## 🔴 P0 — 必须修

### T1　修复侧边栏分组错位

**文件**：`site/.vitepress/config/sidebar.ts:41-58`（`WIN32_GROUPS`）

**根因**：`getGroupIndex`（`:60-70`）把 `prefix` 当作下界（`num >= groupNum` 返回该组），但 `WIN32_GROUPS` 里的 prefix 实际填的是每组**最后一篇**的编号。结果每个分组都吞掉了下一组的开头文章。已用真实文件模拟确认 16 组里只有 3 个边界正确（例如：文章 18–24 的 GDI 内容全部落在「工具栏与状态栏」下；文章 9–10 对话框落在「控件篇」；文章 4–7 ListView/TreeView 落在「基础篇」）。

**改法**：把每个 prefix 改成该组**第一篇**文章号（改后与 `tutorial/native_win32/index.md` 的章节表完全一致）：

```ts
const WIN32_GROUPS: ChapterGroup[] = [
  { title: '基础篇',                    prefix: '0' },
  { title: '控件篇',                    prefix: '4' },     // 原 '8'
  { title: '对话框篇',                  prefix: '9' },     // 原 '11'
  { title: '资源篇',                    prefix: '12' },    // 原 '17'
  { title: '工具栏与状态栏',            prefix: '17_5' },
  { title: 'GDI 图形篇',                prefix: '18' },    // 原 '25'
  { title: 'GDI+ 篇',                   prefix: '26' },    // 原 '28'
  { title: 'Direct2D / DirectWrite 篇', prefix: '29' },    // 原 '33'
  { title: 'HLSL 着色器篇',             prefix: '34' },    // 原 '36'
  { title: 'Direct3D 11 篇',            prefix: '37' },    // 原 '42'
  { title: 'Direct3D 12 篇',            prefix: '43' },    // 原 '47'
  { title: '自定义控件篇',              prefix: '48' },    // 原 '50'
  { title: 'OpenGL 篇',                 prefix: '51' },
  { title: '高级消息篇',                prefix: '52' },
  { title: 'Win32 进阶篇',              prefix: '53' },    // 原 '60'
  { title: '阶段项目',                  prefix: '61' },
]
```

> 17/17_5/18 三者经核验可正确分流。文章 61（"2D 精灵渲染器"）内容实为 D3D11，归"阶段项目"是内容定性问题，由作者确认是否改挂 D3D11 篇。

---

### T2　接回 MiniUI 实战（hands_on_ur_own_gui）

**文件**：`site/.vitepress/config/sidebar.ts`、`site/.vitepress/config/nav.ts`、`tutorial/hands_on_ur_own_gui/`（需补 index.md）、`site/.vitepress/theme/components/HomeRoadmap.vue`

**问题**：`tutorial/hands_on_ur_own_gui/handbook/` 下 `62_xcb_window` ~ `71_threadpool` 共 10 篇系列长文，在 nav、sidebar、首页 features、HomeRoadmap **四处全无入口**（全站 grep 零命中）。`buildSidebar()` 只扫描 `native_win32` 和 `bonus`。读者除手敲 URL 外无法发现。

**改法**（4 步）：

① 在 `sidebar.ts` 的 `buildSidebar()` 内（仿现有 bonus 块，约 `:124` 处）新增：

```ts
const handsOnDir = join(DOCS_ROOT, 'hands_on_ur_own_gui/handbook')
if (existsSync(handsOnDir)) {
  const entries = readdirSync(handsOnDir)
    .filter(e => !e.startsWith('.') && e.endsWith('.md') && e !== 'index.md')
    .sort(sortEntries)
  if (entries.length > 0) {
    sidebar['/hands_on_ur_own_gui/'] = entries.map(name => {
      const title = extractTitle(join(handsOnDir, name)) || humanize(name.replace(/\.md$/, ''))
      return { text: title, link: `/hands_on_ur_own_gui/handbook/${name.replace(/\.md$/, '')}` }
    })
  }
}
```

② 在 `nav.ts` 的 `navZh` 增加（建议放 Win32 之后）：
```ts
{ text: '自己动手写 GUI', link: '/hands_on_ur_own_gui/' },
```

③ **必须**新建 `tutorial/hands_on_ur_own_gui/index.md`（否则上一步的 nav link 会 404）。风格仿 `tutorial/native_win32/index.md`：板块概述 + 前置知识（C++ 基础、RAII、Win32 经验）+ 10 章索引表。

④ 在 `HomeRoadmap.vue` 的 `layers` 数组末尾追加：
```ts
{
  id: 'miniui', title: 'MiniUI 实战', status: 'done', statusText: '已完成',
  count: '10 章 · 从 XCB 到线程池',
  desc: '用 XCB+Cairo 从零手搓教学级 GUI 框架：事件循环、控件树、布局、信号、渲染管线、协程、响应式、线程池',
  link: '/hands_on_ur_own_gui/',
},
```

---

### T3　正文字号回调到中文友好尺度

**文件**：`site/.vitepress/theme/custom.css:2-5` 等

**问题**：`.vp-doc { font-size: 0.82rem }` ≈ 13px，低于 VitePress 默认 16px 与中文舒适下限（15–16px）。代码块 0.75rem、行内代码 0.78em、表格 0.78rem 同步偏小。

**改法**：见 T9 的合并 CSS（统一在 `:root` 块改）。正文 1rem(16px)、代码/表格 0.875rem(14px)，最低不低于 0.95rem。

---

### T4　补全 OG/Twitter + sitemap + robots

**文件**：`site/.vitepress/config/index.ts`

**问题**：`head[]` 只有一个 favicon；顶层无 `sitemap`；`public/` 无 robots.txt、无 og 图。

**改法**：

① 在 `site/.vitepress/public/` 放一张 1200×630 的 `og-image.png`（可临时用 `tutorial/Awesome-Embedded.png` 压缩版，见 T15）。

② 顶部抽常量并改 `head`（替换 `index.ts:32-34`）：

```ts
const SITE = 'https://charliechen114514.github.io/anatomy_gui/'

// ... defineConfig 内：
head: [
  ['link', { rel: 'icon', href: '/anatomy_gui/favicon.ico' }],
  ['meta', { name: 'theme-color', content: '#2f6fed' }],
  ['link', { rel: 'canonical', href: SITE }],
  ['meta', { property: 'og:site_name', content: 'anatomy_gui 文档' }],
  ['meta', { property: 'og:type', content: 'website' }],
  ['meta', { property: 'og:title', content: 'anatomy_gui · GUI 编程全栈教程' }],
  ['meta', { property: 'og:description', content: '面向 C++ 开发者的图形界面编程完整路线图：Win32/GDI/Direct2D/Direct3D 11/12 与自研 MiniUI 框架。' }],
  ['meta', { property: 'og:url', content: SITE }],
  ['meta', { property: 'og:image', content: SITE + 'og-image.png' }],
  ['meta', { property: 'og:locale', content: 'zh_CN' }],
  ['meta', { name: 'twitter:card', content: 'summary_large_image' }],
  ['meta', { name: 'twitter:image', content: SITE + 'og-image.png' }],
],
```

③ 顶层（与 `base` 同级）加：
```ts
sitemap: { hostname: SITE },
```

④ 新建 `site/.vitepress/public/robots.txt`：
```
User-agent: *
Allow: /

Sitemap: https://charliechen114514.github.io/anatomy_gui/sitemap.xml
```

---

### T5　给 hlsl 代码块加语法高亮

**文件**：`site/.vitepress/config/index.ts:43-45`（`languageAlias`）

**问题**：全站有 **34 个 `hlsl` 代码块**（34–36 章为核心 HLSL 章节），但 `hlsl` 不是 Shiki 捆绑语言、也未别名，当前以纯文本渲染。`languages:['c']`（`:42`）是冗余（c 本就捆绑，该选项是"追加"非"替换"），可删。

**改法**：
```ts
markdown: {
  lineNumbers: true,
  theme: { light: 'github-light', dark: 'github-dark' },
  languageAlias: {
    rc: 'c',
    hlsl: 'cpp',   // 新增：HLSL 用 cpp 近似高亮
  },
  config(md) { cppTemplateEscapePlugin(md) },
},
```
> 删掉原来的 `languages: ['c']`。若发现那 1 个 `glsl` 块也无高亮，再加 `glsl: 'cpp'`。

---

## 🟠 P1 — 建议尽快修

### T6　「补充专题」改为指向板块索引

**文件**：`site/.vitepress/config/nav.ts:6`、`tutorial/index.md:14`、新建 `tutorial/bonus/index.md`

**问题**：nav「补充专题」深链到单篇文章 `/bonus/EnableThemeDialogTexture_TabControl`，文案像类目但落地是单文；首页 hero alt 按钮（`index.md:13-14`）同样深链。

**改法**：① 新建 `tutorial/bonus/index.md`（板块概述 + 文章索引）；② `nav.ts:6` 改 `{ text: '补充专题', link: '/bonus/' }`；③ `index.md:14` hero alt 的 `link` 改 `/bonus/`。

---

### T7　删除重复的 GitHub nav 项

**文件**：`site/.vitepress/config/nav.ts:7`

**问题**：GitHub 在 nav 文本项（`nav.ts:7`）与右上角 `socialLinks` 图标（`index.ts:69-71`）重复。

**改法**：删 `nav.ts:7` 那条 `{ text: 'GitHub', ... }`，保留 `socialLinks` 图标（腾出 nav 槽位给 T2 的 MiniUI 入口）。

---

### T8　侧边栏分组默认折叠

**文件**：`site/.vitepress/config/sidebar.ts:106-111`

**问题**：16 组全部硬编码 `collapsed: false`，平铺 63 篇，侧栏过长。

**改法**：把 `:109` 的 `collapsed: false` 改为 `collapsed: true`。VitePress 会自动展开当前文章所在组，不影响到达性。
> 若想保留首个组（基础篇）默认展开，可改成 `collapsed: i !== 0`（注意此处的 `i` 是 `for (const [_, group] of groups)` 的 Map 插入序，非数组下标，但因首条插入的恰是基础篇，结论可达成）。

---

### T9　补 CJK 字体栈 + 品牌色 + 字号（合并改 custom.css）

**文件**：`site/.vitepress/theme/custom.css`

**问题**：① 无 `--vp-font-family` 覆盖，跨平台中文字形回落不一致；② 从未覆盖 `--vp-c-brand-*`，品牌色为默认绿，hero 渐变还拼接 绿+靛+紫 三色，HomeRoadmap 又用第二套硬编码绿；③ 字号过小（见 T3）。

**改法**（替换文件顶部 `:root` 与 `.vp-doc` 块）：

```css
:root {
  --vp-layout-max-width: 80rem;

  /* CJK 优先字体栈 */
  --vp-font-family: system-ui, -apple-system, 'Segoe UI', Roboto,
    'PingFang SC', 'Hiragino Sans GB', 'Microsoft YaHei', '微软雅黑',
    'Source Han Sans SC', 'Noto Sans CJK SC', sans-serif;
  --vp-font-family-mono: 'JetBrains Mono', 'Cascadia Code', 'Fira Code',
    Consolas, ui-monospace, SFMono-Regular, Menlo, monospace;

  /* 聚焦品牌色（蓝青系，呼应 Windows/DirectX） */
  --vp-c-brand-1: #2f6fed;
  --vp-c-brand-2: #1e54c7;
  --vp-c-brand-3: #6f9bff;
  --vp-c-brand-soft: rgba(47, 111, 237, 0.14);
  --vp-c-brand: var(--vp-c-brand-1);
}
.dark {
  --vp-c-brand-1: #6f9bff;
  --vp-c-brand-2: #4f86ff;
  --vp-c-brand-3: #aac6ff;
  --vp-c-brand-soft: rgba(111, 155, 255, 0.16);
}

/* 正文字号（中文友好） */
.vp-doc { line-height: 1.85; font-size: 1rem; }
.vp-doc h1 { line-height: 1.4; }
.vp-doc h2 { line-height: 1.45; }
.vp-doc h3 { line-height: 1.5; }
.vp-doc h4 { line-height: 1.55; }
.vp-doc p { margin-bottom: 1.2em; }

/* 代码块/表格同步上调 */
.vp-doc div[class*='language-'] { border-radius: 8px; font-size: 0.875rem; line-height: 1.65; }
.vp-doc div[class*='language-'] code { font-size: 0.875rem; }
.vp-doc :not(pre) > code { font-size: 0.875em; padding: 0.15em 0.4em; border-radius: 4px; }
.vp-doc table { font-size: 0.875rem; line-height: 1.65; border-radius: 8px; overflow: hidden; }
```

并修改 hero 名称渐变（收敛为同色系，去掉 purple）：
```css
.VPHero .name.clip {
  background: linear-gradient(135deg, var(--vp-c-brand-1), var(--vp-c-brand-3));
  -webkit-background-clip: text;
  background-clip: text;
  -webkit-text-fill-color: transparent;
}
```

同步 `HomeRoadmap.vue` 的 `<style scoped>`（徽标并入品牌色）：
```css
.node-check { background: var(--vp-c-brand-1); }              /* 原 var(--vp-c-green-1) */
.node-badge.done { background: var(--vp-c-brand-soft); color: var(--vp-c-brand-1); }
.node-badge { font-size: 0.75rem; }
.node-desc { font-size: 0.9rem; }
```

---

### T10　批量补 frontmatter（title + description）

**文件**：`tutorial/**/*.md`（76 个中 74 个无 frontmatter）

**问题**：几乎所有内容页首行即 `# 标题`，无 per-page title/description，OG/分享卡片/搜索结果全部回退到全局通用文案。同时导致侧边栏标签是冗长的完整 H1（带书名号、编号、副标题）。

**改法**：写脚本读取每篇首行 `#` 作 `title`（并精简，如去掉"通用GUI编程技术——"前缀），抽取首段摘要作 `description`（120–160 字，含关键词）。示例：
```md
---
title: DPI 适配完全指南
description: 系统 DPI、Per-Monitor DPI、DPI Aware manifest 与 Win32 API 适配方法，解决高 DPI 屏幕下控件错位与文字模糊。
---

# 编程GUI到底什么是DPI —— Windows 平台 DPI 适配实战指南
...
```
> 建议同时精简侧边栏显示标题（`sidebar.ts:extractTitle` 已优先读 frontmatter title）。

---

### T11　删除重复的 C++ 转义插件

**文件**：`site/.vitepress/plugins/vite-escape-cpp.ts`（删）、`site/.vitepress/config/index.ts:5,21`

**问题**：两个功能几乎重复的插件同时挂载：`vite-escape-cpp.ts`（Vite enforce:'pre'）与 `escape-cpp-templates.ts`（markdown-it，含 post-render sanitize）。两者都对全部 ~76 个 .md 跑正则，HTML_TAGS 白名单不同步（190 vs 143）。

**改法**：只保留 markdown-it 版（更安全）。
① 删除文件 `site/.vitepress/plugins/vite-escape-cpp.ts`；
② `config/index.ts` 去掉 `import { viteCppEscape } from '../plugins/vite-escape-cpp'`（`:5`）；
③ 把 `vite` 块改为（去掉 `plugins`）：
```ts
vite: {
  build: { chunkSizeWarningLimit: 5000 },
},
```
`markdown.config` 里的 `cppTemplateEscapePlugin(md)` 保持不变。

> **转义插件已知失败案例（内容规范）**：两个插件的转义正则都是 `/<([^<>\n]+)>/`，**不能跨内部尖括号匹配**。因此散文里裸写的嵌套模板（如 `std::vector<std::vector<int>>`、`std::function<void(int)>`）不会被转义，可能被 Vue 当 HTML 标签误解析。**约定：散文里的 C++ 类型/模板一律用反引号包裹**（`` `std::vector<std::vector<int>>` ``），转义插件已显式跳过行内代码。

---

## 🟡 P2 — 体验/性能打磨

### T12　修复亮色下"看不见的斑马纹"
`custom.css:52` 的 `rgba(0,0,0,0.02)` 在白底上近乎不可见。改：
```css
.vp-doc table tr:nth-child(even) { background-color: var(--vp-c-bg-soft); }
```

### T13　补全响应式断点
`custom.css` 仅 `max-width:639px` 一处断点（且只管 feature 卡片）。补平板/移动：
```css
@media (max-width: 768px) {
  .vp-doc { font-size: 0.95rem; }
  .vp-doc div[class*='language-'] { font-size: 0.8rem; }
  .vp-doc table { font-size: 0.8rem; display: block; overflow-x: auto; }
  .vp-doc :not(pre) > code { font-size: 0.82em; }
}
```

### T14　渐进清理 `!important`
`custom.css` 共 33 处 `!important`（主要在 `.VPHero/.VPFeature` 子选择器）。改用更高特异性（如 `.VPHome .VPFeature .title`）替代，可分批迁移。

### T15　删除/压缩孤儿图
`tutorial/Awesome-Embedded.png`（1.4MB）与 `tutorial/hands_on_ur_own_gui/fullread/asset/stage1-mini-window.png`（222KB）在全部 .md/.vue/.ts/.css 中零引用。
- 确认无历史用途后删除；或若要在首页用，先转 webp 压到 200KB 以内（`cwebp Awesome-Embedded.png -o og-image.webp`）。

### T16　给正文图加懒加载与尺寸
`tutorial/native_win32/48_...OwnerDraw.md:75` 的 `![...](../images/48.png)` 缺 `loading="lazy"`/尺寸。改 HTML img：
```html
<img src="../images/48.png" alt="Owner-Draw 控件示例运行效果" width="640" height="360" loading="lazy" />
```
图片多时可写 markdown-it 插件统一注入。

### T17　本地搜索加权
`config/index.ts:55-57` 的 `search` 无 options。补：
```ts
search: {
  provider: 'local',
  options: { minisearch: { searchOptions: { boost: { title: 4, headings: 2 } } } },
},
```

### T18　复核 chunkSizeWarningLimit 与 isCustomElement
- `config/index.ts:18-22`：`chunkSizeWarningLimit: 5000` 无注释、依赖极少本不应触达。**构建后**若没有 chunk 接近 5000KB 就删；若确有大 chunk 改用 `manualChunks` 拆分而非抬阈值。
- `config/index.ts:24-30`：`isCustomElement: tag => tag.includes('-') || tag.includes('.')` 过宽，会静默吞掉真实模板错误。收窄为白名单（保留 `mjx-`/`vp-`/`client-`/标准 web-component 形），去掉 `.`：
```ts
isCustomElement: (tag) =>
  tag.startsWith('mjx-') || tag.startsWith('vp-') ||
  tag.startsWith('client-') || tag === 'content' ||
  /^[a-z][a-z0-9]*-[a-z0-9-]+$/.test(tag),
```

---

## ⚪ P3 — 打磨

- **T19**：`custom.css:59,266` blockquote 用固定靛蓝 `rgba(81,107,232,…)`，改 `var(--vp-c-brand-soft)`。
- **T20**：补 `:focus-visible` 品牌色轮廓（当前默认已合规，锦上添花）。
- **T21**：favicon 仅 .ico；补 svg/png 并在 head 按优先级声明，加 `apple-touch-icon`。
- **T22**：仓库同时有 `pnpm-lock.yaml` 与 `package-lock.json`，删 `package-lock.json` 并加入 `.gitignore`（项目用 pnpm）。
- **T23**：`tutorial/hands_on_ur_own_gui/fullread/` 仅含 1 张图无 md，确认用途：做"长读"视图就补 index，废弃就删。

---

## 值得保留（不要改坏）

`cleanUrls` / `lastUpdated` / `editLink` / `socialLinks` / `lang:'zh-CN'` 均已正确配置；`line-height:1.85` 对中文友好；feature 卡入场动画已正确处理 `prefers-reduced-motion`；链接 dotted 下划线可访问性良好；`print` 媒体查询齐全；README 已有"在线阅读"链接。

---

## 推荐落地顺序

1. **T1**（侧边栏 prefix，16 行）+ **T5**（hlsl 别名，1 行）—— 小改动、立竿见影
2. **T3+T9**（字号/字体/品牌色，合并改 custom.css）
3. **T2**（接回 MiniUI）+ **T6**（bonus 索引）—— 内容发现
4. **T4**（OG + sitemap）+ **T7**（删冗余 GitHub）
5. **T11**（删重复转义插件）+ **T8**（侧栏折叠）
6. **T10**（批量 frontmatter，工作量大但收益高）
7. P2/P3 按需
