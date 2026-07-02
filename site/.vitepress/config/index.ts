import { defineConfig } from 'vitepress'
import { navZh } from './nav'
import { buildSidebar } from './sidebar'
import { cppTemplateEscapePlugin } from '../plugins/escape-cpp-templates'
import { codeFoldPlugin } from '../plugins/code-fold-plugin'
import { kbdPlugin } from '../plugins/kbd-plugin'
import { mermaidPlugin } from '../plugins/mermaid-plugin'
import { viteCppEscape } from '../plugins/vite-escape-cpp'

const SITE = 'https://charliechen114514.github.io/anatomy_gui'

export default defineConfig({
  srcDir: '../tutorial',

  title: 'anatomy_gui 文档',
  description: '面向 C++ 开发者的图形界面编程完整路线图',
  lang: 'zh-CN',
  base: '/anatomy_gui/',
  cleanUrls: true,
  lastUpdated: true,

  sitemap: {
    hostname: SITE,
  },

  vite: {
    build: {
      chunkSizeWarningLimit: 5000,
    },
    plugins: [viteCppEscape()],
  },

  vue: {
    template: {
      compilerOptions: {
        isCustomElement: (tag: string) => tag.includes('-') || tag.includes('.'),
      },
    },
  },

  head: [
    ['link', { rel: 'icon', href: '/anatomy_gui/favicon.svg', type: 'image/svg+xml' }],
    ['link', { rel: 'icon', href: '/anatomy_gui/favicon.ico' }],
    ['meta', { name: 'theme-color', media: '(prefers-color-scheme: light)', content: '#F7F3EC' }],
    ['meta', { name: 'theme-color', media: '(prefers-color-scheme: dark)', content: '#161412' }],
    ['link', { rel: 'canonical', href: SITE }],
    ['meta', { property: 'og:site_name', content: 'anatomy_gui' }],
    ['meta', { property: 'og:type', content: 'website' }],
    ['meta', { property: 'og:title', content: '剖析 GUI · C++ GUI 解剖学教程' }],
    ['meta', { property: 'og:description', content: '从零造一具骨架，看穿每一个框架的内部设计' }],
    ['meta', { property: 'og:url', content: SITE }],
    ['meta', { property: 'og:image', content: `${SITE}/og-image.png` }],
    ['meta', { property: 'og:locale', content: 'zh_CN' }],
    ['meta', { name: 'twitter:card', content: 'summary_large_image' }],
    ['meta', { name: 'twitter:image', content: `${SITE}/og-image.png` }],
    ['script', {}, `(function(){try{var fs=localStorage.getItem('vp-font-size');if(fs)document.documentElement.dataset.fontSize=fs;var sw=localStorage.getItem('vp-sidebar-width');if(sw)document.documentElement.style.setProperty('--vp-sidebar-width',sw+'px');var aw=localStorage.getItem('vp-aside-width');if(aw)document.documentElement.style.setProperty('--vp-aside-width',aw+'px')}catch(e){}})()`],
  ],

  markdown: {
    lineNumbers: true,
    math: true,
    theme: {
      light: 'github-light',
      dark: 'github-dark',
    },
    languageAlias: {
      rc: 'c',
      hlsl: 'cpp',
    },
    config(md) {
      cppTemplateEscapePlugin(md)
      codeFoldPlugin(md)
      kbdPlugin(md)
      mermaidPlugin(md)
    },
  },

  themeConfig: {
    nav: navZh,
    sidebar: buildSidebar(),

    search: {
      provider: 'local',
    },

    editLink: {
      pattern: 'https://github.com/Charliechen114514/anatomy_gui/edit/main/tutorial/:path',
      text: '在 GitHub 上编辑此页',
    },

    footer: {
      message: '基于 VitePress 构建',
      copyright: 'Copyright 2025-2026 Charliechen',
    },

    socialLinks: [
      { icon: 'github', link: 'https://github.com/Charliechen114514/anatomy_gui' },
    ],
  },
})
